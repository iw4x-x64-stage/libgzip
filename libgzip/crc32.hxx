// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <cstdint> // uint32_t
#include <string_view>

#include <libgzip/export.hxx>

namespace gzip
{
  // CRC-32 calculator.
  //
  // This is the CRC of ISO 3309 and ITU-T V.42, which gzip uses for the data
  // of a member and, if the member says so, for its header (RFC 1952
  // 2.3.1). The data can be appended in parts of any size and the value is
  // the same as for the whole data appended at once. For example:
  //
  // crc32 c;
  // c.append ("1234");
  // c.append ("56789");
  //
  // assert (c.value () == 0xcbf43926);
  //
  class LIBGZIP_SYMEXPORT crc32
  {
  public:
    crc32 () = default;

    explicit
    crc32 (std::string_view s) {append (s);}

    // Append the data to what has been appended so far.
    //
    void
    append (std::string_view) noexcept;

    // Return the CRC-32 of the data appended so far, 0 if none.
    //
    std::uint32_t
    value () const noexcept {return value_;}

    // Reset to the default-constructed state.
    //
    void
    reset () noexcept {value_ = 0;}

  private:
    std::uint32_t value_ = 0;
  };
}
