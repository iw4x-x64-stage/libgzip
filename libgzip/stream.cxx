// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libgzip/stream.hxx>

#include <span>
#include <exception>   // uncaught_exceptions()
#include <string_view>

#include <libgzip/contract.hxx>

using namespace std;

namespace gzip
{
  // istreambuf
  //

  void istreambuf::
  open (std::istream& is, size_t max_string)
  {
    if (ib_ == nullptr)
    {
      ib_ = make_unique_for_overwrite<char[]> (buffer_size);
      ob_ = make_unique_for_overwrite<char[]> (buffer_size);
    }

    is_  = &is;
    eof_ = false;
    in_  = 0;
    ip_  = 0;

    d_.emplace (max_string);
    setg (ob_.get (), ob_.get (), ob_.get ());
  }

  void istreambuf::
  close () noexcept
  {
    is_ = nullptr;
    setg (nullptr, nullptr, nullptr);
  }

  const optional<gzip::header>& istreambuf::
  header () const noexcept
  {
    LIBGZIP_PRE (is_open ());
    return d_->header ();
  }

  istreambuf::int_type istreambuf::
  underflow ()
  {
    if (gptr () != egptr ())
      return traits_type::to_int_type (*gptr ());

    if (!is_open ())
      return traits_type::eof ();

    // Decompress until we have some output or the data ends. Each round
    // either uses some input or, having used it all, takes more, so we get
    // somewhere.
    //
    // Note that the output of a round comes from a single member (see
    // decompressor::next() for details), which is what header() relies on.
    //
    while (!d_->finished ())
    {
      if (ip_ == in_ && !eof_)
        eof_ = !load ();

      const progress p (
        d_->next (string_view (ib_.get () + ip_, in_ - ip_),
                  span<char> (ob_.get (), buffer_size),
                  eof_));

      ip_ += p.consumed;

      if (p.produced != 0)
      {
        setg (ob_.get (), ob_.get (), ob_.get () + p.produced);
        return traits_type::to_int_type (*gptr ());
      }
    }

    return traits_type::eof ();
  }

  bool istreambuf::
  load ()
  {
    // The gzip data ends the other stream, so we can take as much of it as
    // fits.
    //
    in_ = static_cast<size_t> (
      is_->rdbuf ()->sgetn (ib_.get (), static_cast<streamsize> (buffer_size)));
    ip_ = 0;

    return in_ != 0;
  }

  // istream
  //

  istream::
  istream (iostate e)
    : std::istream (&buf_)
  {
    LIBGZIP_PRE ((e & badbit) != 0);
    exceptions (e);
  }

  istream::
  istream (std::istream& is, size_t max_string, iostate e)
    : istream (e)
  {
    open (is, max_string);
  }

  // ostreambuf
  //

  void ostreambuf::
  open (std::ostream& os, compression_level l, const gzip::header& h)
  {
    LIBGZIP_PRE (!is_open ());
    LIBGZIP_PRE ((os.exceptions () & (std::ostream::badbit |
                                      std::ostream::failbit)) ==
                 (std::ostream::badbit | std::ostream::failbit));

    // Construct the compressor first, since it verifies the header, so
    // that the buffer stays closed if the header doesn't pass.
    //
    c_.emplace (l, h);

    if (ib_ == nullptr)
    {
      ib_ = make_unique_for_overwrite<char[]> (buffer_size);
      ob_ = make_unique_for_overwrite<char[]> (buffer_size);
    }

    os_ = &os;

    // Keep a character of the put area for the one that overflow() gets.
    //
    setp (ib_.get (), ib_.get () + buffer_size - 1);
  }

  void ostreambuf::
  close ()
  {
    if (!is_open ())
      return;

    // Whether or not finishing the data succeeds, the buffer ends up closed
    // since there is no getting back to a consistent state.
    //
    try
    {
      save (true);
    }
    catch (...)
    {
      os_ = nullptr;
      throw;
    }

    os_ = nullptr;
  }

  ostreambuf::int_type ostreambuf::
  overflow (int_type c)
  {
    if (!is_open ())
      return traits_type::eof ();

    if (!traits_type::eq_int_type (c, traits_type::eof ()))
    {
      *pptr () = traits_type::to_char_type (c);
      pbump (1);
    }

    save (false);
    return traits_type::not_eof (c);
  }

  void ostreambuf::
  save (bool end)
  {
    string_view in (pbase (), static_cast<size_t> (pptr () - pbase ()));

    // Compress until all of the input is taken and the compressor has
    // nothing more to write (which we know when it doesn't fill the output)
    // or, at the end, until the compressed data is finished.
    //
    for (;;)
    {
      const progress p (
        c_->next (in, span<char> (ob_.get (), buffer_size), end));

      in.remove_prefix (p.consumed);
      os_->write (ob_.get (), static_cast<streamsize> (p.produced));

      if (end ? c_->finished () : in.empty () && p.produced != buffer_size)
        break;
    }

    setp (ib_.get (), ib_.get () + buffer_size - 1);
  }

  // ostream
  //

  ostream::
  ostream (iostate e)
    : std::ostream (&buf_)
  {
    LIBGZIP_PRE ((e & badbit) != 0);
    exceptions (e);
  }

  ostream::
  ostream (std::ostream& os,
           compression_level l,
           const gzip::header& h,
           iostate e)
    : ostream (e)
  {
    open (os, l, h);
  }

  ostream::
  ~ostream ()
  {
    // Make sure close() was called (see the class for details).
    //
    LIBGZIP_PRE (!is_open () || !good () || uncaught_exceptions () != 0);
  }
}
