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
  // Gzip data is a series of one or more members, each holding deflate data
  // between a header and a trailer, and it decompresses into the data of
  // all its members, one after another.
  //
  // The decompressor takes the data in parts of any size, down to a byte at
  // a time, and writes the result into output buffers of any size, so the
  // caller decides how much memory either side takes. It accepts every data
  // set that the format defines and rejects everything else, throwing
  // invalid_data on the first fault it finds. We are strict here since a
  // value that the format reserves or leaves undefined could mean anything,
  // including a header that we would misread. So, for example, a reserved
  // flag bit is a fault, and so is anything after the last member. The one
  // exception is the operating system, whose list of values the format
  // leaves open (see operating_system for details). After an exception the
  // decompressor can only be reset.
  //
  // The decompressor keeps the header of the current member (see header()
  // for details). The format puts no limit on the size of its file name and
  // file comment, so to keep the memory bounded we limit them ourselves and
  // reject a member that goes over (see the constructor for details).
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
    // Default limit of the file name and file comment size. We use the
    // format's own limit of the extra field, which is plenty for any real
    // name or comment.
    //
    static constexpr std::size_t default_max_string = max_extra;

    // Limit the file name and the file comment of each member to max_string
    // bytes, not counting the terminating zero byte.
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
    // Note that stopping at the end of each member means that the output of
    // a call comes from a single member, whose header header() returns after
    // the call. A call after the end of the data does nothing.
    //
    progress
    next (std::string_view in, std::span<char> out, bool end);

    // Return true if the end of the data has been reached.
    //
    bool
    finished () const noexcept {return state_ == state::finished;}

    // Return the header of the current member, which is the last one whose
    // header has been read, or nullopt if no header has been read yet.
    //
    const std::optional<gzip::header>&
    header () const noexcept {return header_;}

    // Return the number of members whose header has been read. A caller
    // that needs the header of every member can watch it change between the
    // calls to next().
    //
    std::size_t
    members () const noexcept {return members_;}

    // Reset to the state after construction, to decompress other data with
    // the same limit.
    //
    void
    reset () noexcept;

  private:
    // Where we are in the data. Note that the header states are in the
    // order of the fields in the header, which enter() relies on.
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

    // Take bytes from the input into field_ until it has n of them. Return
    // false if the input runs out first.
    //
    bool
    collect (std::size_t n);

    // Take bytes from the input into field_ up to the terminating zero byte,
    // which is consumed but not stored. Return false if the input runs out
    // first. Throw invalid_data if the string goes over the limit, with what
    // naming the string in the description (for example, "file name").
    //
    bool
    collect_string (const char* what);

    // Move to the first header field, starting from the specified one, that
    // the member's flags say is present. If there is none, then the header
    // is complete and we move to the deflate data.
    //
    void
    enter (state);

    // Parse the extra field in field_ into the subfields of the header being
    // read.
    //
    void
    parse_extra ();

  private:
    std::size_t max_string_;
    state       state_;

    // Input and output of the next() call in progress, with the positions
    // up to which they have been used.
    //
    std::string_view in_;
    std::size_t      in_pos_;
    std::span<char>  out_;
    std::size_t      out_pos_;

    // The header is read into pending_ and only becomes the current one
    // once it is complete, so that header() never returns a partial header.
    //
    std::optional<gzip::header> header_;
    std::size_t                 members_;
    gzip::header                pending_;

    // State of the header being read. A field is collected in field_ until
    // it is complete, since it may arrive split across the next() calls.
    //
    std::uint8_t flags_;        // FLG of the member.
    std::string  field_;        // Bytes of the field being read.
    std::size_t  extra_length_; // XLEN of the member.
    crc32        header_crc_;   // CRC-32 of the header fields read so far.

    // Deflate data of the member and what we know about its decompressed
    // data, for verifying against the trailer.
    //
    deflate::decompressor deflate_;
    crc32                 crc_;  // CRC-32 of the decompressed data.
    std::uint32_t         size_; // Size of the decompressed data modulo 2^32.
  };

  // Decompress the whole data and return the result, with the default
  // limit of the file name and file comment (see decompressor for details).
  // Throw invalid_data if the data is invalid or incomplete.
  //
  LIBGZIP_SYMEXPORT std::string
  decompress (std::string_view);
}
