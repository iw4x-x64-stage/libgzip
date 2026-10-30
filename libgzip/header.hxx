// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <array>
#include <string>
#include <vector>
#include <cstdint> // uint8_t, uint32_t
#include <optional>

// The member header and its values, as RFC 1952 defines them. Section
// numbers below refer to that document.
//
namespace gzip
{
  // Extra flags, which say how the deflate data was compressed (2.3.1).
  //
  enum class extra_flags: std::uint8_t
  {
    none    = 0,
    maximum = 2, // Maximum compression, slowest algorithm.
    fastest = 4  // Fastest algorithm.
  };

  // Operating system (more precisely, the type of file system) on which the
  // compression took place (2.3.1).
  //
  // The format lists these values as the ones currently defined, which
  // leaves the list open, so the operating system may well be another
  // value.
  //
  enum class operating_system: std::uint8_t
  {
    fat          = 0,  // FAT file system (MS-DOS, OS/2, NT/Win32).
    amiga        = 1,
    vms          = 2,  // VMS (or OpenVMS).
    unix_system  = 3,  // Named so since GCC predefines unix as a macro.
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
  // The subfield ID is typically two ASCII letters, and its second byte
  // must not be zero, since the IDs with a zero second byte are reserved.
  //
  struct subfield
  {
    std::array<char, 2> id;   // SI1 and SI2.
    std::string         data;
  };

  // Header of a member (2.3).
  //
  // The file name and file comment are in ISO 8859-1 and can contain any
  // character but the zero byte, which terminates them in the header. The
  // file name is the name of the original file with any directory
  // components removed (and, for a file system with case-insensitive names,
  // in lower case), and the comment separates lines with a single line feed
  // character.
  //
  struct header
  {
    bool             text  = false; // Data is probably ASCII text.
    std::uint32_t    mtime = 0;     // Seconds since the epoch, 0 if unknown.
    extra_flags      flags = extra_flags::none;
    operating_system os    = operating_system::unknown;

    // The optional fields, each present if it has a value.
    //
    std::optional<std::vector<subfield>> extra;
    std::optional<std::string>           name;
    std::optional<std::string>           comment;

    // Whether the header ends with the CRC16 of what precedes it.
    //
    bool crc16 = false;
  };
}
