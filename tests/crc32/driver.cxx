// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <ios>       // ios_base
#include <print>
#include <string>
#include <cstddef>   // size_t
#include <cstdio>    // stdin
#include <iostream>
#include <iterator>  // istreambuf_iterator
#include <algorithm> // min()
#include <optional>
#include <string_view>

#ifdef _WIN32
#  include <io.h>    // _setmode(), _fileno()
#  include <fcntl.h> // _O_BINARY
#endif

#include <libgzip/crc32.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace gzip;

// Usage: argv[0] [--chunk <n>] [--reset]
//
// Calculate the CRC-32 of stdin and print it as 8 hex digits.
//
// --chunk <n>
//
//   Append the data n bytes at a time instead of in one go.
//
// --reset
//
//   Append the data, reset the calculator, and append the data again.
//
int
main (int argc, char* argv[])
{
  optional<size_t> chunk;
  bool reset (false);

  for (int i (1); i != argc; ++i)
  {
    const string_view a (argv[i]);

    if (a == "--reset")
      reset = true;
    else if (a == "--chunk")
    {
      assert (i + 1 != argc);
      chunk = stoul (argv[++i]);
    }
    else
      assert (false);
  }

  assert (!chunk || *chunk != 0);

  // Read the data as it is.
  //
#ifdef _WIN32
  _setmode (_fileno (stdin), _O_BINARY);
#endif

  cin.exceptions (ios_base::badbit);

  const string d (istreambuf_iterator<char> (cin), {});

  auto append = [&chunk] (crc32& c, string_view s)
  {
    if (!chunk)
    {
      c.append (s);
      return;
    }

    for (; !s.empty (); s.remove_prefix (min (*chunk, s.size ())))
      c.append (s.substr (0, *chunk));
  };

  crc32 c;
  append (c, d);

  if (reset)
  {
    c.reset ();
    append (c, d);
  }

  println ("{:08x}", c.value ());
}
