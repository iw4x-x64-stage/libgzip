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
  // Compression level, from 0 to 9 (see deflate::compression_level for
  // details).
  //
  using deflate::compression_level;

  // Incremental compressor into gzip data (RFC 1952).
  //
  // The compressor writes the data as a single member with the specified
  // header, which it verifies on construction. It takes the data in parts
  // of any size and writes the compressed data into output buffers of any
  // size. The compressed data depends only on the data, the level, and the
  // header, so however the data is split into parts, the result is the
  // same.
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
    // The level must be in the range from 0 to 9. The extra flags of the
    // header are set from the level, whatever the header says: maximum for
    // level 9, fastest for levels 0 and 1, and none otherwise.
    //
    // Throw std::invalid_argument if the header is invalid: its file name
    // or file comment contains a zero byte, its extra field is longer than
    // the format allows, or one of its subfields has a reserved ID.
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
    // Note that the deflate compressor holds on to the data that it may
    // still find a repeated string in. So until end is true, the compressor
    // may write nothing for a while.
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
    // Where we are in the compressed data.
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

    // The header and trailer as written, and how much of the one being
    // written is.
    //
    std::string head_;
    char        tail_[trailer_size];
    std::size_t pos_;

    // Compressor of the deflate data, and the CRC-32 and size (modulo 2^32)
    // of the data compressed with it.
    //
    deflate::compressor deflate_;
    crc32               crc_;
    std::uint32_t       size_;
  };

  // Compress the whole data at the level with the header and return the
  // result. The level must be in the range from 0 to 9. Throw
  // std::invalid_argument if the header is invalid (see compressor for
  // details).
  //
  LIBGZIP_SYMEXPORT std::string
  compress (std::string_view,
            compression_level = compression_level::balanced,
            const header& = header ());
}
