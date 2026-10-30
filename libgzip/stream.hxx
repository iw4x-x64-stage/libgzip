// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <memory>   // unique_ptr
#include <cstddef>  // size_t
#include <istream>
#include <ostream>
#include <optional>
#include <streambuf>

#include <libgzip/header.hxx>
#include <libgzip/compressor.hxx>
#include <libgzip/decompressor.hxx>

#include <libgzip/export.hxx>

namespace gzip
{
  // Stream buffer that decompresses the gzip data it reads from another
  // stream.
  //
  // The gzip data must end the other stream, which is verified at the end
  // of the stream. Note that the buffer reads the other stream's buffer
  // directly, so the other stream's state stays as it is.
  //
  class LIBGZIP_SYMEXPORT istreambuf: public std::streambuf
  {
  public:
    // Start decompressing from the stream, with the limit of the file name
    // and file comment (see decompressor for details).
    //
    void
    open (std::istream&,
          std::size_t max_string = decompressor::default_max_string);

    bool
    is_open () const noexcept {return is_ != nullptr;}

    // Stop decompressing. Nothing more can be read until the buffer is
    // opened again.
    //
    void
    close () noexcept;

    // Return the header of the member that the characters being read come
    // from, or nullopt if nothing has been read yet. The buffer must be
    // open.
    //
    const std::optional<gzip::header>&
    header () const noexcept;

  protected:
    // Decompress more of the data. Throw invalid_data if the data is invalid
    // or incomplete.
    //
    virtual int_type
    underflow () override;

  private:
    // Take what the other stream's buffer has to give into the input
    // buffer. Return false if the other stream has ended.
    //
    bool
    load ();

  private:
    static constexpr std::size_t buffer_size = 16384;

    std::istream*               is_ = nullptr;
    bool                        eof_; // Other stream has ended.
    std::optional<decompressor> d_;
    std::unique_ptr<char[]>     ib_;  // Input (compressed data).
    std::size_t                 in_;  // Input size.
    std::size_t                 ip_;  // Input used.
    std::unique_ptr<char[]>     ob_;  // Output (get area).
  };

  // Input stream of the data that gzip data in another stream decompresses
  // into. For example:
  //
  // try
  // {
  //   ifstream ifs (..., ios::binary);
  //   ifs.exceptions (ifstream::badbit);
  //
  //   gzip::istream is (ifs);
  //   ... // Read from is.
  // }
  // catch (const gzip::invalid_data& e)
  // {
  //   ... // Invalid gzip data, see e.what () for the description.
  // }
  //
  // The badbit exception must be enabled, since that is how the errors of
  // the decompression and of the other stream come through.
  //
  class LIBGZIP_SYMEXPORT istream: public std::istream
  {
  public:
    explicit
    istream (iostate = badbit | failbit);

    explicit
    istream (std::istream&,
             std::size_t max_string = decompressor::default_max_string,
             iostate = badbit | failbit);

    // See istreambuf::open() for details.
    //
    void
    open (std::istream& is,
          std::size_t max_string = decompressor::default_max_string)
    {
      buf_.open (is, max_string);
    }

    bool
    is_open () const noexcept {return buf_.is_open ();}

    void
    close () noexcept {buf_.close ();}

    // See istreambuf::header() for details.
    //
    const std::optional<gzip::header>&
    header () const noexcept {return buf_.header ();}

  private:
    istreambuf buf_;
  };

  // Stream buffer that compresses what is written to it into gzip data that
  // it writes to another stream, as a single member with the specified
  // header.
  //
  // Note that syncing the buffer does nothing, since the deflate compressor
  // only writes what it has compressed in whole blocks.
  //
  class LIBGZIP_SYMEXPORT ostreambuf: public std::streambuf
  {
  public:
    // Start compressing into the stream at the level, which must be in the
    // range from 0 to 9, with the header. The stream must have the badbit
    // and failbit exceptions enabled. Throw std::invalid_argument if the
    // header is invalid (see compressor for details).
    //
    void
    open (std::ostream&, compression_level, const gzip::header&);

    bool
    is_open () const noexcept {return os_ != nullptr;}

    // Compress the rest of what was written and write the end of the
    // compressed data to the other stream.
    //
    void
    close ();

  protected:
    virtual int_type
    overflow (int_type) override;

  private:
    // Compress what is in the put area into the other stream, finishing the
    // compressed data if end is true.
    //
    void
    save (bool end);

  private:
    static constexpr std::size_t buffer_size = 16384;

    std::ostream*             os_ = nullptr;
    std::optional<compressor> c_;
    std::unique_ptr<char[]>   ib_; // Input (put area).
    std::unique_ptr<char[]>   ob_; // Output (compressed data).
  };

  // Output stream that compresses what is written to it into gzip data in
  // another stream. For example:
  //
  // ofstream ofs (..., ios::binary);
  // ofs.exceptions (ofstream::badbit | ofstream::failbit);
  //
  // gzip::header h;
  // h.name = "hello.txt";
  //
  // gzip::ostream os (ofs, gzip::compression_level::smallest, h);
  // ... // Write to os.
  //
  // os.close ();
  // ofs.close ();
  //
  // Note that close() must be called explicitly, since the compressed data
  // is incomplete until then: the stream must not be open when it is
  // destroyed unless it is not good or an exception is being thrown.
  //
  class LIBGZIP_SYMEXPORT ostream: public std::ostream
  {
  public:
    explicit
    ostream (iostate = badbit | failbit);

    explicit
    ostream (std::ostream&,
             compression_level = compression_level::balanced,
             const gzip::header& = gzip::header (),
             iostate = badbit | failbit);

    ~ostream () override;

    // See ostreambuf::open() for details.
    //
    void
    open (std::ostream& os,
          compression_level l = compression_level::balanced,
          const gzip::header& h = gzip::header ())
    {
      buf_.open (os, l, h);
    }

    bool
    is_open () const noexcept {return buf_.is_open ();}

    // Write the end of the compressed data (see ostreambuf::close() for
    // details).
    //
    void
    close () {buf_.close ();}

  private:
    ostreambuf buf_;
  };
}
