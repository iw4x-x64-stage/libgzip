// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libgzip/decompressor.hxx>

#include <utility>   // move()
#include <algorithm> // min(), max()

#include <libgzip/contract.hxx>

using namespace std;

namespace gzip
{
  namespace
  {
    // Return the number stored in the n bytes at p. The format stores all
    // its multi-byte numbers least significant byte first (2.1).
    //
    uint32_t
    load_le (const char* p, size_t n) noexcept
    {
      LIBGZIP_PRE (n <= 4);

      uint32_t r (0);

      for (size_t i (n); i != 0; --i)
        r = (r << 8) | static_cast<unsigned char> (p[i - 1]);

      return r;
    }
  }

  decompressor::
  decompressor (size_t max_string)
    : max_string_ (max_string)
  {
    reset ();
  }

  void decompressor::
  reset () noexcept
  {
    state_        = state::fixed_header;
    in_           = {};
    in_pos_       = 0;
    out_          = {};
    out_pos_      = 0;
    members_      = 0;
    pending_      = gzip::header ();
    flags_        = 0;
    extra_length_ = 0;
    size_         = 0;

    header_.reset ();
    field_.clear ();
    header_crc_.reset ();
    deflate_.reset ();
    crc_.reset ();

    LIBGZIP_ASSERT (invariant ());
  }

  progress decompressor::
  next (string_view in, span<char> out, bool end)
  {
    LIBGZIP_INVARIANT (invariant ());

    in_      = in;
    in_pos_  = 0;
    out_     = out;
    out_pos_ = 0;

    // Go through the data until the input runs out, the output fills up, a
    // member ends, or the data ends.
    //
    // The input may end anywhere, including in the middle of a header field.
    // So each header step first collects its whole field in field_ and only
    // then interprets it and changes the state. If the input runs out first,
    // then what was collected stays in field_ and the same step picks up
    // from there on the next call.
    //
    bool starved (false);
    bool full (false);
    bool boundary (false);

    while (!starved && !full && !boundary && state_ != state::finished)
    {
      switch (state_)
      {
        case state::fixed_header:
        {
          // See if this is the end of the data. Since the data is at least
          // one member, it may only end here after a member.
          //
          if (field_.empty () && in_pos_ == in_.size () && end)
          {
            if (members_ == 0)
              throw invalid_data ("empty gzip data");

            state_ = state::finished;
            break;
          }

          // Verify each byte as soon as we have it. This way data in another
          // format is rejected at its first byte, even if it ends before the
          // fixed part of the header would (in which case we would otherwise
          // only see incomplete gzip data).
          //
          while (field_.size () != fixed_header_size)
          {
            if (!collect (field_.size () + 1))
            {
              starved = true;
              break;
            }

            const uint8_t b (static_cast<uint8_t> (field_.back ()));

            switch (field_.size () - 1)
            {
              case 0:
              case 1:
              {
                const uint8_t id (field_.size () == 1 ? id1 : id2);

                if (b != id)
                  throw invalid_data (
                    "expected identification byte {:#04x} instead of {:#04x}",
                    id,
                    b);

                break;
              }
              case 2:
              {
                if (b != deflate_method)
                  throw invalid_data ("invalid compression method {}", b);

                break;
              }
              case 3:
              {
                if ((b & flag_reserved) != 0)
                  throw invalid_data (
                    "invalid flags {:#04x} with reserved bits set", b);

                break;
              }
              case 8:
              {
                // Besides 0, these are the only values that the deflate method
                // defines (2.3.1).
                //
                const extra_flags f (static_cast<extra_flags> (b));

                if (f != extra_flags::none    &&
                    f != extra_flags::maximum &&
                    f != extra_flags::fastest)
                  throw invalid_data ("invalid extra flags {}", b);

                break;
              }
            }
          }

          if (starved)
            break;

          // Start the header from scratch, since it is a new member.
          //
          flags_ = static_cast<uint8_t> (field_[3]);

          pending_       = gzip::header ();
          pending_.text  = (flags_ & flag_text) != 0;
          pending_.mtime = load_le (field_.data () + 4, 4);
          pending_.flags = static_cast<extra_flags> (field_[8]);
          pending_.os    = static_cast<operating_system> (field_[9]);
          pending_.crc16 = (flags_ & flag_hcrc) != 0;

          header_crc_.reset ();
          header_crc_.append (field_);

          enter (state::extra_length);
          break;
        }
        case state::extra_length:
        {
          if (!collect (2))
          {
            starved = true;
            break;
          }

          header_crc_.append (field_);

          // The extra field is present even if it is empty, which is a
          // series of no subfields.
          //
          extra_length_ = load_le (field_.data (), 2);
          pending_.extra.emplace ();

          if (extra_length_ == 0)
          {
            enter (state::name);
            break;
          }

          field_.clear ();
          state_ = state::extra;
          break;
        }
        case state::extra:
        {
          if (!collect (extra_length_))
          {
            starved = true;
            break;
          }

          header_crc_.append (field_);
          parse_extra ();

          enter (state::name);
          break;
        }
        case state::name:
        case state::comment:
        {
          const bool n (state_ == state::name);

          if (!collect_string (n ? "file name" : "file comment"))
          {
            starved = true;
            break;
          }

          // The file name and file comment differ only in where they go and
          // what comes after them. Note that the terminating zero byte is part
          // of the header and so of its CRC.
          //
          header_crc_.append (field_);
          header_crc_.append (string_view ("", 1));

          (n ? pending_.name : pending_.comment) = move (field_);

          enter (n ? state::comment : state::header_crc);
          break;
        }
        case state::header_crc:
        {
          if (!collect (2))
          {
            starved = true;
            break;
          }

          // The CRC16 is the lower half of the CRC-32 of the header bytes
          // that precede it (2.3.1), which header_crc_ has by now.
          //
          const uint32_t c (load_le (field_.data (), 2));
          const uint32_t e (header_crc_.value () & 0xffff);

          if (c != e)
            throw invalid_data (
              "invalid header CRC16 {:#06x} for header with CRC16 {:#06x}",
              c,
              e);

          enter (state::data);
          break;
        }
        case state::data:
        {
          // The deflate decompressor never takes input past the end of the
          // deflate data, so the trailer is left in the input for us. If end
          // is true, we pass it on, since the deflate data then has to end in
          // this input as well. Whatever fault the deflate decompressor finds,
          // we report it as a fault of the gzip data.
          //
          progress p;

          try
          {
            p = deflate_.next (in_.substr (in_pos_),
                               out_.subspan (out_pos_),
                               end);
          }
          catch (const deflate::invalid_data& e)
          {
            throw invalid_data ("{}", e.what ());
          }

          crc_.append (string_view (out_.data () + out_pos_, p.produced));
          size_ += static_cast<uint32_t> (p.produced); // Modulo 2^32.

          in_pos_  += p.consumed;
          out_pos_ += p.produced;

          if (deflate_.finished ())
          {
            field_.clear ();
            state_ = state::trailer;
            break;
          }

          // Otherwise, the deflate decompressor stopped because one of the
          // buffers ran out. Note that if both did, we treat it as full output,
          // since the next call will have more room and take it from there.
          //
          if (out_pos_ == out_.size ())
            full = true;
          else
            starved = true;

          break;
        }
        case state::trailer:
        {
          if (!collect (trailer_size))
          {
            starved = true;
            break;
          }

          // Verify the trailer against what we have decompressed.
          //
          const uint32_t c (load_le (field_.data (), 4));
          const uint32_t n (load_le (field_.data () + 4, 4));

          if (c != crc_.value ())
            throw invalid_data (
              "invalid CRC-32 {:#010x} for data with CRC-32 {:#010x}",
              c,
              crc_.value ());

          if (n != size_)
            throw invalid_data (
              "invalid size {} for data of size {} modulo 2^32", n, size_);

          // The member ends here and so does this call, which keeps the
          // output of a call to a single member (see next() for details). If
          // the last part of the input ends here as well, then so does the
          // data.
          //
          field_.clear ();
          state_ = in_pos_ == in_.size () && end
                   ? state::finished
                   : state::fixed_header;

          boundary = true;
          break;
        }
        case state::finished:
          LIBGZIP_UNREACHABLE ();
      }
    }

    // If we ran out of the last part of the input, then the data is
    // incomplete. Note that the deflate decompressor has already thrown if
    // that happened in the deflate data.
    //
    if (starved && end)
    {
      LIBGZIP_ASSERT (state_ != state::data);

      if (state_ == state::trailer)
        throw invalid_data ("incomplete member trailer");

      throw invalid_data ("incomplete member header");
    }

    const progress r {in_pos_, out_pos_};

    in_  = {};
    out_ = {};

    return r;
  }

  bool decompressor::
  invariant () const noexcept
  {
    // We have a header if and only if we have read one, and everything from
    // the deflate data on comes after a header.
    //
    return header_.has_value () == (members_ != 0) &&
           (state_ < state::data || members_ != 0);
  }

  bool decompressor::
  collect (size_t n)
  {
    LIBGZIP_PRE (field_.size () <= n);

    const size_t k (min (n - field_.size (), in_.size () - in_pos_));

    field_.append (in_.substr (in_pos_, k));
    in_pos_ += k;

    return field_.size () == n;
  }

  bool decompressor::
  collect_string (const char* what)
  {
    const string_view s (in_.substr (in_pos_));
    const size_t p (s.find ('\0'));
    const size_t k (p != string_view::npos ? p : s.size ());

    if (k > max_string_ - field_.size ())
      throw invalid_data ("{} longer than {} bytes", what, max_string_);

    field_.append (s.substr (0, k));
    in_pos_ += k;

    if (p == string_view::npos)
      return false;

    ++in_pos_; // Terminating zero byte.
    return true;
  }

  void decompressor::
  enter (state s)
  {
    LIBGZIP_PRE (s == state::extra_length ||
                 s == state::name         ||
                 s == state::comment      ||
                 s == state::header_crc   ||
                 s == state::data);

    // The optional header fields, in the order of the header, with the flag
    // that says whether the member has each of them.
    //
    struct optional_field
    {
      state        field;
      uint8_t flag;
    };

    static const optional_field fields[]
    {
      {state::extra_length, flag_extra},
      {state::name,         flag_name},
      {state::comment,      flag_comment},
      {state::header_crc,   flag_hcrc}
    };

    field_.clear ();

    for (const optional_field& f: fields)
    {
      if (f.field >= s && (flags_ & f.flag) != 0)
      {
        state_ = f.field;
        return;
      }
    }

    // The header is complete. Make it the current one and get ready for the
    // deflate data that follows.
    //
    header_ = move (pending_);
    ++members_;

    deflate_.reset ();
    crc_.reset ();
    size_ = 0;

    state_ = state::data;
  }

  void decompressor::
  parse_extra ()
  {
    // The extra field is a series of subfields, each with a two-byte ID and
    // a two-byte length followed by that many bytes of data (2.3.1.1). So the
    // field must end where a subfield ends.
    //
    vector<subfield>& r (*pending_.extra);

    for (string_view s (field_); !s.empty (); )
    {
      if (s.size () < subfield_header_size)
        throw invalid_data ("incomplete extra subfield header");

      const uint8_t si1 (static_cast<uint8_t> (s[0]));
      const uint8_t si2 (static_cast<uint8_t> (s[1]));

      if (si2 == 0)
        throw invalid_data ("reserved extra subfield ID {:#04x} {:#04x}",
                            si1,
                            si2);

      const size_t n (load_le (s.data () + 2, 2));

      if (n > s.size () - subfield_header_size)
        throw invalid_data ("extra subfield length {} exceeds extra field",
                            n);

      r.push_back (subfield {{s[0], s[1]},
                             string (s.substr (subfield_header_size, n))});

      s.remove_prefix (subfield_header_size + n);
    }
  }

  string
  decompress (string_view in)
  {
    decompressor d;

    // Decompress into the result, growing it as it fills up. Since we tell
    // the decompressor that the input is all there is, each call either gets
    // somewhere or throws.
    //
    const size_t initial (4096);

    string r;
    size_t n (0);

    while (!d.finished ())
    {
      if (n == r.size ())
        r.resize (max ({r.size () * 2, in.size () * 4, initial}));

      const progress p (
        d.next (in, span<char> (r.data (), r.size ()).subspan (n), true));

      in.remove_prefix (p.consumed);
      n += p.produced;
    }

    // The decompressor only finishes at the end of the input, so there is
    // nothing left over to complain about.
    //
    LIBGZIP_ASSERT (in.empty ());

    r.resize (n);
    return r;
  }
}
