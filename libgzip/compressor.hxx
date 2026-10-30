// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <span>
#include <string>
#include <cstddef> // size_t
#include <cstdint> // uint32_t
#include <string_view>

#include <libdeflate/compressor.hxx>

#include <libgzip/crc32.hxx>
#include <libgzip/types.hxx>
#include <libgzip/format.hxx>
#include <libgzip/header.hxx>

#include <libgzip/export.hxx>

namespace gzip
{
  // Compression level, from 0 to 9. This is libdeflate's level, which the
  // deflate data is compressed at (see deflate::compression_level for
  // details).
  //
  using deflate::compression_level;

  // Incremental compressor into gzip data (RFC 1952).
  //
  // The compressor writes a single member with the specified header, which
  // it verifies on construction. It takes the data in parts of any size and
  // writes the compressed data into output buffers of any size. The result
  // only depends on the data, the level, and the header, so it is the same
  // however the data is split into parts.
  //
  // For example:
  //
  // header h;
  // h.name = "hello.txt";
  //
  // compressor c (compression_level::smallest, h);
  // char out[4096];
  //
  // while (!c.finished ())
  // {
  //   string_view in (...); // Next part of the data.
  //   bool end (...);       // Whether it is the last part.
  //
  //   for (progress p {0, sizeof (out)};
  //        !c.finished () && (!in.empty () || p.produced == sizeof (out)); )
  //   {
  //     p = c.next (in, out, end);
  //     in.remove_prefix (p.consumed);
  //
  //     ... // Write the first p.produced bytes of out.
  //   }
  // }
  //
  class LIBGZIP_SYMEXPORT compressor
  {
  public:
    // The level must be in the range from 0 to 9. Set the extra flags of
    // the header from the level, replacing whatever the header has: maximum
    // for level 9, fastest for levels 0 and 1, and none for the rest. The
    // flags describe how the data was compressed, so only we can tell.
    //
    // Throw std::invalid_argument if the header cannot be written as it is,
    // which is the case if its file name or file comment contains a zero
    // byte (which would terminate it early), if its extra field is longer
    // than its two-byte length allows, or if one of its subfields has a
    // reserved ID.
    //
    explicit
    compressor (compression_level = compression_level::balanced,
                const header& = header ());

    // Compress the next part of the data from the input into the output.
    // Stop when the output fills up, or when the input runs out and all that
    // can be compressed of it is written, and return how much of each was
    // used.
    //
    // If end is true, then the input is the last part of the data, and the
    // compressor finishes the compressed data once it has taken all of it.
    // Once end is true, it must stay so in the calls that follow.
    //
    // Note that the deflate compressor holds on to the data that later input
    // may repeat. So until end is true, a call may take all of its input and
    // write nothing.
    //
    progress
    next (std::string_view in, std::span<char> out, bool end);

    // Return true if all of the compressed data has been written.
    //
    bool
    finished () const noexcept {return state_ == state::finished;}

    // Reset to the state after construction, to compress other data at the
    // same level and with the same header.
    //
    void
    reset () noexcept;

  private:
    // Which part of the member we are writing.
    //
    enum class state
    {
      header,
      data,
      trailer,
      finished
    };

    state state_;
    bool  ending_; // End of the data was given.

    // We serialize the header on construction and the trailer once the
    // deflate data is finished, and then copy them into the output as it
    // has room.
    //
    std::string head_;
    char        tail_[trailer_size];
    std::size_t pos_;                // Bytes of head_ or tail_ written.

    // Deflate data of the member and what the trailer says about the data
    // that it compresses.
    //
    deflate::compressor deflate_;
    crc32               crc_;  // CRC-32 of the data.
    std::uint32_t       size_; // Size of the data modulo 2^32.
  };

  // Compress the whole data into a member with the header at the level and
  // return the result. The level must be in the range from 0 to 9. Throw
  // std::invalid_argument if the header cannot be written (see compressor
  // for details).
  //
  LIBGZIP_SYMEXPORT std::string
  compress (std::string_view,
            compression_level = compression_level::balanced,
            const header& = header ());
}
