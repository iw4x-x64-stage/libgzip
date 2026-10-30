// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <cstddef> // size_t
#include <cstdint> // uint8_t

// Constants of the gzip file format. The section numbers in the comments
// below refer to RFC 1952.
//
namespace gzip
{
  // Identification bytes, the first two bytes of every member (2.3.1).
  //
  inline constexpr std::uint8_t id1 (0x1f);
  inline constexpr std::uint8_t id2 (0x8b);

  // Compression method value of deflate (2.3.1). This is the only method
  // that gzip defines: it reserves 0 to 7 and leaves the rest unassigned.
  //
  inline constexpr std::uint8_t deflate_method (8);

  // Bits of the flags byte (2.3.1). A reserved bit could announce a field
  // that a decompressor doesn't know how to skip, so the format requires it
  // to be zero.
  //
  inline constexpr std::uint8_t flag_text     (0x01); // FTEXT
  inline constexpr std::uint8_t flag_hcrc     (0x02); // FHCRC
  inline constexpr std::uint8_t flag_extra    (0x04); // FEXTRA
  inline constexpr std::uint8_t flag_name     (0x08); // FNAME
  inline constexpr std::uint8_t flag_comment  (0x10); // FCOMMENT
  inline constexpr std::uint8_t flag_reserved (0xe0);

  // Size of the fixed part of the header (identification bytes to operating
  // system) and of the trailer (CRC-32 and size of the original data), in
  // bytes (2.3).
  //
  inline constexpr std::size_t fixed_header_size (10);
  inline constexpr std::size_t trailer_size (8);

  // Largest extra field, whose length is stored in two bytes, and the size
  // of the ID and length that start each of its subfields (2.3.1.1).
  //
  inline constexpr std::size_t max_extra (65535);
  inline constexpr std::size_t subfield_header_size (4);
}
