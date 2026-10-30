// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libgzip/compressor.hxx>

#include <algorithm> // min(), max(), ranges::copy()
#include <stdexcept> // invalid_argument

#include <libgzip/contract.hxx>

using namespace std;

namespace gzip
{
  namespace
  {
    // Store the value in the n bytes at p, least significant byte first, as
    // the format stores all its multi-byte numbers (2.1).
    //
    void
    store_le (char* p, uint32_t v, size_t n) noexcept
    {
      LIBGZIP_PRE (n <= 4);

      for (size_t i (0); i != n; ++i, v >>= 8)
        p[i] = static_cast<char> (v & 0xff);
    }

    // Append the value to the string as n bytes (see store_le() for
    // details).
    //
    void
    append_le (string& s, uint32_t v, size_t n)
    {
      char b[4];
      store_le (b, v, n);
      s.append (b, n);
    }

    // Return the header as written to the member, with the extra flags of
    // the level. Throw invalid_argument if the header cannot be written.
    //
    string
    serialize (const header& h, compression_level l)
    {
      // Verify the fields that the format puts requirements on, adding up
      // the length of the extra field while at it.
      //
      if (h.name && h.name->find ('\0') != string::npos)
        throw invalid_argument ("file name contains zero byte");

      if (h.comment && h.comment->find ('\0') != string::npos)
        throw invalid_argument ("file comment contains zero byte");

      size_t xlen (0);

      if (h.extra)
      {
        for (const subfield& f: *h.extra)
        {
          if (f.id[1] == '\0')
            throw invalid_argument ("extra subfield with reserved ID");

          // Check the length after each subfield, which stops the sum long
          // before it could overflow.
          //
          xlen += subfield_header_size + f.data.size ();

          if (xlen > max_extra)
            throw invalid_argument ("extra field longer than 65535 bytes");
        }
      }

      // Figure out the extra flags. The deflate method defines one value for
      // the slowest algorithm and one for the fastest (2.3.1). We take level
      // 9 for the slowest and levels 0 and 1 for the fastest, the same as
      // zlib does, and say nothing for the levels in between.
      //
      extra_flags x (extra_flags::none);

      if (l == compression_level::smallest)
        x = extra_flags::maximum;
      else if (l <= compression_level::fastest)
        x = extra_flags::fastest;

      // Write the fixed part, with a flag for each field that follows.
      //
      uint8_t flg (0);

      if (h.text)    flg |= flag_text;
      if (h.crc16)   flg |= flag_hcrc;
      if (h.extra)   flg |= flag_extra;
      if (h.name)    flg |= flag_name;
      if (h.comment) flg |= flag_comment;

      string r;
      r += static_cast<char> (id1);
      r += static_cast<char> (id2);
      r += static_cast<char> (deflate_method);
      r += static_cast<char> (flg);
      append_le (r, h.mtime, 4);
      r += static_cast<char> (x);
      r += static_cast<char> (h.os);

      // Write the optional fields, in the order of the header.
      //
      if (h.extra)
      {
        append_le (r, static_cast<uint32_t> (xlen), 2);

        for (const subfield& f: *h.extra)
        {
          r.append (f.id.data (), f.id.size ());
          append_le (r, static_cast<uint32_t> (f.data.size ()), 2);
          r += f.data;
        }
      }

      if (h.name)
      {
        r += *h.name;
        r += '\0';
      }

      if (h.comment)
      {
        r += *h.comment;
        r += '\0';
      }

      // Finish with the CRC16, which is the lower half of the CRC-32 of the
      // header bytes that precede it (2.3.1).
      //
      if (h.crc16)
        append_le (r, crc32 (r).value () & 0xffff, 2);

      return r;
    }
  }

  compressor::
  compressor (compression_level l, const header& h)
    : head_ (serialize (h, l)),
      deflate_ (l)
  {
    reset ();
  }

  void compressor::
  reset () noexcept
  {
    state_  = state::header;
    ending_ = false;
    pos_    = 0;
    size_   = 0;

    deflate_.reset ();
    crc_.reset ();
  }

  progress compressor::
  next (string_view in, span<char> out, bool end)
  {
    LIBGZIP_PRE (!ending_ || end);

    ending_ = end;

    size_t ip (0);
    size_t op (0);

    // Copy what is left of the bytes, from pos_ on, into the output as far
    // as it has room. Return true if everything is copied.
    //
    auto copy = [&out, &op, this] (string_view s)
    {
      const size_t n (min (s.size () - pos_, out.size () - op));

      ranges::copy (s.substr (pos_, n), out.data () + op);
      pos_ += n;
      op   += n;

      return pos_ == s.size ();
    };

    // Write the parts of the member in order until the output fills up or,
    // in the deflate data, the input runs out.
    //
    for (bool stop (false); !stop && state_ != state::finished; )
    {
      switch (state_)
      {
        case state::header:
        {
          if (!copy (head_))
          {
            stop = true;
            break;
          }

          pos_   = 0;
          state_ = state::data;
          break;
        }
        case state::data:
        {
          const progress p (
            deflate_.next (in.substr (ip), out.subspan (op), end));

          crc_.append (in.substr (ip, p.consumed));
          size_ += static_cast<uint32_t> (p.consumed); // Modulo 2^32.

          ip += p.consumed;
          op += p.produced;

          // If the deflate compressor hasn't finished, then it has done all
          // it can with this input and output. Otherwise, the trailer follows,
          // which we can only write now that we have seen all the data.
          //
          if (!deflate_.finished ())
          {
            stop = true;
            break;
          }

          store_le (tail_,     crc_.value (), 4);
          store_le (tail_ + 4, size_,         4);

          state_ = state::trailer;
          break;
        }
        case state::trailer:
        {
          if (!copy (string_view (tail_, trailer_size)))
          {
            stop = true;
            break;
          }

          state_ = state::finished;
          break;
        }
        case state::finished:
          LIBGZIP_UNREACHABLE ();
      }
    }

    return progress {ip, op};
  }

  string
  compress (string_view in, compression_level l, const header& h)
  {
    compressor c (l, h);

    // Compress into the result, growing it as it fills up. Since we tell
    // the compressor that the input is all there is, it finishes once it
    // has written everything.
    //
    const size_t initial (4096);

    string r;
    size_t n (0);

    while (!c.finished ())
    {
      if (n == r.size ())
        r.resize (max ({r.size () * 2, in.size () / 2, initial}));

      const progress p (
        c.next (in, span<char> (r.data (), r.size ()).subspan (n), true));

      in.remove_prefix (p.consumed);
      n += p.produced;
    }

    r.resize (n);
    return r;
  }
}
