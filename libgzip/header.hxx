// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <array>
#include <string>
#include <vector>
#include <cstdint> // uint8_t, uint32_t
#include <optional>

// The header of a gzip member and the types of its values. The section
// numbers in the comments below refer to RFC 1952.
//
namespace gzip
{
  // Extra flags of the deflate compression method (2.3.1). The method
  // defines two values, which say that the compressor used its slowest or
  // its fastest algorithm, and 0 says neither.
  //
  enum class extra_flags: std::uint8_t
  {
    none    = 0,
    maximum = 2, // Maximum compression, slowest algorithm.
    fastest = 4  // Fastest algorithm.
  };

  // Operating system on which the compression took place, which is really
  // about the type of its file system and so its end-of-line convention
  // (2.3.1).
  //
  // The format calls the values below the currently defined ones, so we
  // treat the list as open: the decompressor passes any value through as it
  // is and the enumeration, with its uint8_t base, can hold all of them.
  //
  enum class operating_system: std::uint8_t
  {
    fat          = 0,  // FAT file system (MS-DOS, OS/2, NT/Win32).
    amiga        = 1,
    vms          = 2,  // VMS (or OpenVMS).
    unix_system  = 3,  // GCC predefines unix as a macro in GNU modes.
    vm_cms       = 4,
    atari_tos    = 5,
    hpfs         = 6,  // HPFS file system (OS/2, NT).
    macintosh    = 7,
    z_system     = 8,
    cp_m         = 9,
    tops_20      = 10,
    ntfs         = 11, // NTFS file system (NT).
    qdos         = 12,
    acorn_riscos = 13,
    unknown      = 255
  };

  // Subfield of the extra field (2.3.1.1).
  //
  // The ID is normally two ASCII letters with some mnemonic value (the
  // format mentions 'AP' for Apollo file type information). The IDs whose
  // second byte is zero are reserved for future use, which is why the
  // compressor refuses to write them and the decompressor rejects them.
  //
  struct subfield
  {
    std::array<char, 2> id;   // SI1 and SI2.
    std::string         data;
  };

  // Header of a member (2.3).
  //
  // The file name and file comment are ISO 8859-1 strings that the header
  // terminates with a zero byte, so they can contain any byte but zero. We
  // don't convert or check them any further. In particular, it is up to the
  // caller to strip the directory components from the file name (and to
  // lower-case it for a file system with case-insensitive names) and to
  // separate the lines of the comment with a single line feed, as the
  // format expects.
  //
  // Note that the compressor sets the extra flags from the compression
  // level, so the flags member only matters in the headers that the
  // decompressor returns.
  //
  struct header
  {
    bool             text  = false; // Data is probably ASCII text.
    std::uint32_t    mtime = 0;     // Seconds since the epoch, 0 if unknown.
    extra_flags      flags = extra_flags::none;
    operating_system os    = operating_system::unknown;

    // Optional fields, each present in the header if it has a value. Note
    // that an empty value is a value: the format tells, say, an empty file
    // name from no file name at all.
    //
    std::optional<std::vector<subfield>> extra;
    std::optional<std::string>           name;
    std::optional<std::string>           comment;

    // Whether the header ends with a CRC16 of the bytes that precede it.
    //
    bool crc16 = false;
  };
}
