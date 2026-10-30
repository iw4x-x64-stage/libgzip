// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <span>
#include <print>
#include <string>
#include <cstddef>   // size_t
#include <cstdint>   // uint8_t
#include <sstream>
#include <iostream>
#include <optional>
#include <algorithm> // ranges::all_of()
#include <string_view>

#ifdef _WIN32
#  include <io.h>    // _setmode(), _fileno()
#  include <fcntl.h> // _O_BINARY
#endif

#include <libgzip/types.hxx>
#include <libgzip/header.hxx>
#include <libgzip/decompressor.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace gzip;

namespace
{
  // Return the bytes in hex, two lowercase digits each.
  //
  string
  to_hex (string_view s)
  {
    string r;

    for (const char c: s)
      r += format ("{:02x}", static_cast<uint8_t> (c));

    return r;
  }

  // Print the header in the form described in main().
  //
  void
  print_header (const header& h)
  {
    println ("text: {}", h.text);
    println ("mtime: {}", h.mtime);
    println ("flags: {}", static_cast<unsigned> (h.flags));
    println ("os: {}", static_cast<unsigned> (h.os));

    if (h.extra)
    {
      string s ("extra:");

      for (const subfield& f: *h.extra)
        s += format (" {}:{}",
                     to_hex (string_view (f.id.data (), f.id.size ())),
                     to_hex (f.data));

      println ("{}", s);
    }

    if (h.name)
      println ("name: '{}'", *h.name);

    if (h.comment)
      println ("comment: '{}'", *h.comment);

    println ("crc16: {}", h.crc16);
  }

  // Decompress the data with a decompressor object that has the limit,
  // feeding it n bytes at a time with room for n bytes of output, and
  // return the result. If headers is true, then print the header of each
  // member as it is read.
  //
  string
  decompress_chunks (string_view d, size_t n, size_t limit, bool headers)
  {
    decompressor z (limit);
    string r;

    string buf (n, '\0');

    for (size_t p (0); !z.finished ();)
    {
      const string_view in (d.substr (p, n));
      const bool end (p + in.size () == d.size ());

      const size_t m (z.members ());
      const progress g (z.next (in, span<char> (buf), end));

      assert (g.consumed <= in.size () && g.produced <= n);

      // A call stops at the end of each member, so it reads at most one
      // header, which we can therefore print right after the call.
      //
      assert (z.members () - m <= 1);

      if (headers && z.members () != m)
      {
        if (m != 0)
          println ();

        print_header (*z.header ());
      }

      r.append (buf, 0, g.produced);
      p += g.consumed;
    }

    return r;
  }
}

// Usage: argv[0] [--chunk <n>] [--limit <n>] [--header]
//
// Read gzip data in the hex form described below from stdin, decompress it
// with decompress(), and print the result to stdout.
//
// --chunk <n>
//
//   Decompress with a decompressor object, passing it n bytes of the data
//   at a time with room for n bytes of output.
//
// --limit <n>
//
//   Decompress with a decompressor object whose limit of the file name and
//   file comment is n bytes.
//
// --header
//
//   Decompress with a decompressor object and print the header of each
//   member, separating the members with a blank line, and leave out the
//   result. A header is printed as the following lines, where the extra,
//   name, and comment lines are there only if the member has the field:
//
//   text: <true|false>
//   mtime: <seconds>
//   flags: <extra-flags>
//   os: <operating-system>
//   extra: [<id>:<data>]...
//   name: '<file-name>'
//   comment: '<file-comment>'
//   crc16: <true|false>
//
//   Where the subfield ID and data are in hex.
//
// The hex form is a sequence of whitespace-separated fields, each of which
// is an even number of hex digits that appends the bytes they make up to the
// data. A '#' starts a comment that runs to the end of the line.
//
int
main (int argc, char* argv[])
{
  optional<size_t> chunk;
  optional<size_t> limit;
  bool headers (false);

  for (int i (1); i != argc; ++i)
  {
    const string_view a (argv[i]);

    if (a == "--header")
      headers = true;
    else if (a == "--chunk" || a == "--limit")
    {
      assert (i + 1 != argc);
      (a == "--chunk" ? chunk : limit) = stoul (argv[++i]);
    }
    else
      assert (false);
  }

  assert (!chunk || *chunk != 0);

  // Switch stdout to the binary mode, since on Windows the text mode would
  // translate the line endings in the result. The input is text.
  //
#ifdef _WIN32
  _setmode (_fileno (stdout), _O_BINARY);
#endif

  cin.exceptions (ios::badbit);

  // Read the data, dropping the comments.
  //
  const auto hex = [] (char c)
  {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  };

  string d;

  for (string l; getline (cin, l); )
  {
    istringstream is (l.substr (0, l.find ('#')));

    for (string f; is >> f; )
    {
      if (f.size () % 2 != 0 || !ranges::all_of (f, hex))
      {
        println (cerr, "error: invalid hex field '{}'", f);
        return 1;
      }

      for (size_t p (0); p != f.size (); p += 2)
        d += static_cast<char> (stoul (f.substr (p, 2), nullptr, 16));
    }
  }

  try
  {
    const bool object (chunk || limit || headers);

    const string r (
      object
      ? decompress_chunks (d,
                           chunk ? *chunk : max<size_t> (d.size (), 1),
                           limit ? *limit : decompressor::default_max_string,
                           headers)
      : decompress (d));

    if (!headers)
      print ("{}", r);
  }
  catch (const invalid_data& e)
  {
    println (cerr, "error: {}", e.what ());
    return 1;
  }
}
