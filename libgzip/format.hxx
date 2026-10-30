// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <cstddef> // size_t
#include <cstdint> // uint8_t

// The constants of the gzip file format, as RFC 1952 defines them. Section
// numbers below refer to that document.
//
namespace gzip
{
  // Identification bytes that every member starts with (2.3.1).
  //
  inline constexpr std::uint8_t id1 (0x1f);
  inline constexpr std::uint8_t id2 (0x8b);

  // Compression method of the deflate format, the only one that the format
  // defines. The methods from 0 to 7 are reserved (2.3.1).
  //
  inline constexpr std::uint8_t deflate_method (8);

  // Bits of the flags byte (2.3.1). The reserved bits must be zero.
  //
  inline constexpr std::uint8_t flag_text     (0x01); // FTEXT
  inline constexpr std::uint8_t flag_hcrc     (0x02); // FHCRC
  inline constexpr std::uint8_t flag_extra    (0x04); // FEXTRA
  inline constexpr std::uint8_t flag_name     (0x08); // FNAME
  inline constexpr std::uint8_t flag_comment  (0x10); // FCOMMENT
  inline constexpr std::uint8_t flag_reserved (0xe0);

  // Sizes of the part of the header that every member has, from the
  // identification bytes to the operating system, and of the trailer, the
  // CRC-32 and the size of the original data (2.3).
  //
  inline constexpr std::size_t fixed_header_size (10);
  inline constexpr std::size_t trailer_size (8);

  // Largest size of the extra field, which its two-byte length limits, and
  // the size of the subfield ID and length that each subfield starts with
  // (2.3.1.1).
  //
  inline constexpr std::size_t max_extra (65535);
  inline constexpr std::size_t subfield_header_size (4);
}
