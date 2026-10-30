// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libgzip/crc32.hxx>

#include <array>

using namespace std;

namespace gzip
{
  namespace
  {
    // Return the table of the CRC of each byte value, for the byte at a time
    // calculation of Sarwate that RFC 1952 refers to (8).
    //
    // The CRC is calculated with the bits of each byte taken least
    // significant first, so the table is of the reflected polynomial
    // 0xedb88320.
    //
    constexpr array<uint32_t, 256>
    make_table () noexcept
    {
      array<uint32_t, 256> r {};

      for (uint32_t n (0); n != r.size (); ++n)
      {
        uint32_t c (n);

        for (unsigned k (0); k != 8; ++k)
          c = (c & 1) != 0 ? 0xedb88320 ^ (c >> 1) : c >> 1;

        r[n] = c;
      }

      return r;
    }

    constexpr array<uint32_t, 256> table (make_table ());
  }

  void crc32::
  append (string_view s) noexcept
  {
    // The calculation works on the complement of the value (which is what
    // makes leading zero bytes count), so complement it on the way in and on
    // the way out.
    //
    uint32_t c (value_ ^ 0xffffffff);

    for (const char x: s)
      c = table[(c ^ static_cast<unsigned char> (x)) & 0xff] ^ (c >> 8);

    value_ = c ^ 0xffffffff;
  }
}
