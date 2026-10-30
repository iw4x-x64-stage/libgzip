// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libgzip/crc32.hxx>

#include <array>

using namespace std;

namespace gzip
{
  namespace
  {
    // Return the CRC of each byte value, which lets us calculate the CRC a
    // byte at a time. This is Sarwate's method, which RFC 1952 cites (8),
    // and the table is the same as in the sample code of the RFC.
    //
    // Note that the CRC takes the bits of each byte least significant first,
    // which is why the polynomial is in its reflected form, 0xedb88320.
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
    // The CRC register starts as all ones and the value is its complement
    // (without this, leading zero bytes would not change the value). Since we
    // keep the value, we complement it back into the register first.
    //
    uint32_t c (value_ ^ 0xffffffff);

    for (const char x: s)
      c = table[(c ^ static_cast<unsigned char> (x)) & 0xff] ^ (c >> 8);

    value_ = c ^ 0xffffffff;
  }
}
