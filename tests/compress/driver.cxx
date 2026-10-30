// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <span>
#include <print>
#include <string>
#include <vector>
#include <cstddef>   // size_t
#include <cstdint>   // uint8_t, uint32_t
#include <cstdio>    // stdin, stdout
#include <iostream>
#include <iterator>  // istreambuf_iterator
#include <optional>
#include <stdexcept> // invalid_argument
#include <string_view>

#ifdef _WIN32
#  include <io.h>    // _setmode(), _fileno()
#  include <fcntl.h> // _O_BINARY
#endif

#include <libgzip/types.hxx>
#include <libgzip/header.hxx>
#include <libgzip/compressor.hxx>
#include <libgzip/decompressor.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace gzip;

namespace
{
  // Return the bytes that the hex digits make up.
  //
  string
  from_hex (string_view s)
  {
    assert (s.size () % 2 == 0);

    string r;

    for (size_t p (0); p != s.size (); p += 2)
      r += static_cast<char> (stoul (string (s.substr (p, 2)), nullptr, 16));

    return r;
  }

  // Return the subfields of the extra field in the form described in
  // main().
  //
  vector<subfield>
  parse_extra (string_view s)
  {
    vector<subfield> r;

    while (!s.empty ())
    {
      const size_t c (s.find (','));
      const string_view f (s.substr (0, c));
      const size_t p (f.find (':'));

      assert (p != string_view::npos);

      const string id (from_hex (f.substr (0, p)));
      assert (id.size () == 2);

      r.push_back (subfield {{id[0], id[1]}, from_hex (f.substr (p + 1))});

      s.remove_prefix (c != string_view::npos ? c + 1 : s.size ());
    }

    return r;
  }

  // Compress the data feeding the compressor n bytes at a time with room
  // for n bytes of output. If reset is true, then do it twice, resetting the
  // compressor in between, and return the second result.
  //
  string
  compress_chunks (string_view d,
                   compression_level l,
                   const header& h,
                   size_t n,
                   bool reset)
  {
    compressor c (l, h);
    string r;

    string buf (n, '\0');

    for (bool again (reset);; again = false)
    {
      for (size_t p (0); !c.finished ();)
      {
        const string_view in (d.substr (p, n));
        const bool end (p + in.size () == d.size ());

        const progress g (c.next (in, span<char> (buf), end));

        assert (g.consumed <= in.size () && g.produced <= n);

        r.append (buf, 0, g.produced);
        p += g.consumed;
      }

      if (!again)
        break;

      c.reset ();
      r.clear ();
    }

    return r;
  }
}

// Usage: argv[0] -c <level> [--chunk <n>] [--reset] [<header-option>...]
//        argv[0] -d
//        argv[0] -x [<size>]
//
// -c  compress stdin at the level with compress() and write the result to
//     stdout
//
// --chunk <n>
//
//     compress with a compressor object instead, passing it n bytes of the
//     data at a time with room for n bytes of output
//
// --reset
//
//     compress with a compressor object, reset it, and compress again
//
// -d  decompress stdin with decompress() and write the result to stdout
//
// -x  print stdin, or its first size bytes, in hex, two lowercase digits
//     for each byte
//
// The header options set the corresponding values of the header:
//
// --text
// --mtime <seconds>
// --flags <extra-flags>
// --os <operating-system>
// --extra [<id>:<data>[,<id>:<data>]...]
// --pad <size>
// --name <file-name>
// --comment <file-comment>
// --crc16
//
// Where the subfield IDs and data, the file name, and the file comment are
// in hex. The --pad option adds a subfield with the 'pd' ID and the size
// zero bytes of data after the subfields of --extra, if any.
//
int
main (int argc, char* argv[])
{
  assert (argc >= 2);

  // Pass the data through as it is.
  //
#ifdef _WIN32
  _setmode (_fileno (stdin),  _O_BINARY);
  _setmode (_fileno (stdout), _O_BINARY);
#endif

  cin.exceptions (ios::badbit);

  const string_view m (argv[1]);
  const string d (istreambuf_iterator<char> (cin), {});

  if (m == "-x")
  {
    assert (argc <= 3);

    const size_t n (argc == 3 ? stoul (argv[2]) : d.size ());
    assert (n <= d.size ());

    string r;
    for (const char c: string_view (d).substr (0, n))
      r += format ("{:02x}", static_cast<uint8_t> (c));

    println ("{}", r);
    return 0;
  }

  if (m == "-d")
  {
    assert (argc == 2);

    try
    {
      print ("{}", decompress (d));
      return 0;
    }
    catch (const invalid_data& e)
    {
      println (cerr, "error: {}", e.what ());
      return 1;
    }
  }

  assert (m == "-c" && argc >= 3);

  const compression_level l (static_cast<compression_level> (stoul (argv[2])));

  optional<size_t> chunk;
  bool reset (false);
  header h;

  for (int i (3); i != argc; ++i)
  {
    const string_view a (argv[i]);

    // Return the option's value.
    //
    auto value = [argc, argv, &i] ()
    {
      assert (i + 1 != argc);
      return string_view (argv[++i]);
    };

    if      (a == "--chunk")   chunk = stoul (string (value ()));
    else if (a == "--reset")   reset = true;
    else if (a == "--text")    h.text = true;
    else if (a == "--crc16")   h.crc16 = true;
    else if (a == "--name")    h.name = from_hex (value ());
    else if (a == "--comment") h.comment = from_hex (value ());
    else if (a == "--extra")   h.extra = parse_extra (value ());
    else if (a == "--mtime")
      h.mtime = static_cast<uint32_t> (stoul (string (value ())));
    else if (a == "--flags")
      h.flags = static_cast<extra_flags> (stoul (string (value ())));
    else if (a == "--os")
      h.os = static_cast<operating_system> (stoul (string (value ())));
    else if (a == "--pad")
    {
      if (!h.extra)
        h.extra.emplace ();

      h.extra->push_back (
        subfield {{'p', 'd'}, string (stoul (string (value ())), '\0')});
    }
    else
      assert (false);
  }

  assert (!chunk || *chunk != 0);

  try
  {
    const string r (chunk || reset
                    ? compress_chunks (d, l, h, chunk ? *chunk : 4096, reset)
                    : compress (d, l, h));

    print ("{}", r);
  }
  catch (const invalid_argument& e)
  {
    println (cerr, "error: {}", e.what ());
    return 1;
  }
}
