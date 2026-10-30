// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <span>
#include <string>
#include <cstddef> // size_t
#include <cstdint> // uint8_t, uint32_t
#include <optional>
#include <string_view>

#include <libdeflate/decompressor.hxx>

#include <libgzip/crc32.hxx>
#include <libgzip/types.hxx>
#include <libgzip/format.hxx>
#include <libgzip/header.hxx>

#include <libgzip/export.hxx>

namespace gzip
{
  // Incremental decompressor of gzip data (RFC 1952).
  //
  // The data is a series of one or more members, each of which holds
  // deflate data between a header and a trailer, and the decompressed data
  // is the decompressed data of all of the members, one after another.
  //
  // The decompressor takes the data in parts of any size, down to a byte at
  // a time, and writes the result into output buffers of any size, so the
  // caller decides how much memory either side takes. It accepts every data
  // set that the format defines and rejects everything else, throwing
  // invalid_data on the first fault it finds. In particular, it rejects a
  // member with an unknown compression method, a reserved flag bit set,
  // extra flags other than those the deflate method defines, an extra field
  // that doesn't consist of whole subfields, a subfield with a reserved ID,
  // a header CRC16, data CRC-32, or data size that doesn't match, as well as
  // anything after the last member. The only header value it takes as it is
  // is the operating system (see operating_system for details). After an
  // exception the decompressor can only be reset.
  //
  // The header of each member is available once it has been read (see
  // header() for details). Since the format puts no limit on the size of
  // the file name and file comment, the decompressor is given one at
  // construction and rejects the members whose file name or file comment
  // exceeds it, so that the memory it takes stays bounded.
  //
  // For example:
  //
  // decompressor d;
  // char out[4096];
  //
  // while (!d.finished ())
  // {
  //   string_view in (...); // Next part of the data.
  //   bool end (...);       // Whether it is the last part.
  //
  //   for (progress p {0, sizeof (out)};
  //        !d.finished () && (!in.empty () || p.produced == sizeof (out)); )
  //   {
  //     p = d.next (in, out, end);
  //     in.remove_prefix (p.consumed);
  //
  //     ... // Write the first p.produced bytes of out.
  //   }
  // }
  //
  class LIBGZIP_SYMEXPORT decompressor
  {
  public:
    // Default size limit of the file name and of the file comment, which is
    // the same as the format's limit of the extra field.
    //
    static constexpr std::size_t default_max_string = max_extra;

    // The file name and file comment of a member may each be up to
    // max_string bytes, the terminating zero byte excluded.
    //
    explicit
    decompressor (std::size_t max_string = default_max_string);

    // Decompress the next part of the data from the input into the output.
    // Stop when the input runs out, the output fills up, a member ends, or
    // the data ends, and return how much of each was used. Throw
    // invalid_data if the data is invalid.
    //
    // If end is true, then the input is the last part of the data, which
    // must therefore end in it. In this case throw invalid_data if the input
    // runs out first.
    //
    // Note that since a call stops at the end of each member, the output of
    // a call always comes from a single member, the one that header()
    // returns the header of after the call. A call after the end of the data
    // does nothing.
    //
    progress
    next (std::string_view in, std::span<char> out, bool end);

    // Return true if the end of the data has been reached.
    //
    bool
    finished () const noexcept {return state_ == state::finished;}

    // Return the header of the current member, which is the last member
    // whose header has been read, or nullopt if there is none yet.
    //
    const std::optional<gzip::header>&
    header () const noexcept {return header_;}

    // Return the number of members whose header has been read.
    //
    std::size_t
    members () const noexcept {return members_;}

    // Reset to the state after construction, to decompress other data with
    // the same limit.
    //
    void
    reset () noexcept;

  private:
    // Where we are in the data. Note that the header fields are in the order
    // in which they appear in the header.
    //
    enum class state
    {
      fixed_header, // Identification bytes to operating system.
      extra_length, // Length of the extra field.
      extra,        // Extra field.
      name,         // File name.
      comment,      // File comment.
      header_crc,   // CRC16 of the header.
      data,         // Deflate data.
      trailer,      // CRC-32 and size of the data.
      finished
    };

    bool
    invariant () const noexcept;

    // Take the input into the field until it has n bytes. Return false if
    // the input runs out first.
    //
    bool
    collect (std::size_t n);

    // Take the input into the field up to the terminating zero byte, which
    // is taken but not stored. Return false if the input runs out first.
    // Throw invalid_data if the string exceeds the limit, calling it what in
    // the description.
    //
    bool
    collect_string (const char* what);

    // Move to the first of the header fields, from the specified one on,
    // that the flags say the member has. If there is none, then start the
    // deflate data.
    //
    void
    enter (state);

    // Parse the subfields of the extra field, which is in the field, into
    // the header being read.
    //
    void
    parse_extra ();

  private:
    std::size_t max_string_;
    state       state_;

    // Input and output of the next() call in progress.
    //
    std::string_view in_;
    std::size_t      in_pos_;
    std::span<char>  out_;
    std::size_t      out_pos_;

    // Header of the current member, the number of members whose header has
    // been read, and the header being read.
    //
    std::optional<gzip::header> header_;
    std::size_t                 members_;
    gzip::header                pending_;

    // Flags of the member, the bytes of the header field being read, the
    // length of the extra field, and the CRC-32 of the header up to the
    // field being read.
    //
    std::uint8_t flags_;
    std::string  field_;
    std::size_t  extra_length_;
    crc32        header_crc_;

    // Decompressor of the deflate data, and the CRC-32 and size (modulo
    // 2^32) of the data decompressed from it.
    //
    deflate::decompressor deflate_;
    crc32                 crc_;
    std::uint32_t         size_;
  };

  // Decompress the whole data and return the result. Throw invalid_data if
  // the data is invalid or incomplete. Use the default limit of the file
  // name and file comment (see decompressor for details).
  //
  LIBGZIP_SYMEXPORT std::string
  decompress (std::string_view);
}
