// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <ios>       // ios_base
#include <print>
#include <string>
#include <cstddef>   // size_t
#include <cstdio>    // stdin, stdout
#include <iostream>
#include <optional>
#include <string_view>

#ifdef _WIN32
#  include <io.h>    // _setmode(), _fileno()
#  include <fcntl.h> // _O_BINARY
#endif

#include <libgzip/types.hxx>
#include <libgzip/header.hxx>
#include <libgzip/stream.hxx>
#include <libgzip/contract.hxx>
#include <libgzip/compressor.hxx>
#include <libgzip/decompressor.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace gzip;

namespace
{
  [[noreturn]] void
  throwing_handler (const contract_violation& v)
  {
    throw v;
  }
}

// Usage: argv[0] -c <level> [--name <file-name>] [--reopen]
//        argv[0] -d [--limit <n>] [--names]
//
// -c  compress stdin at the level by writing it a character at a time to a
//     gzip::ostream over stdout, with the file name in the header if
//     specified
//
// --reopen
//
//     open the gzip::ostream again before writing to it, with a contract
//     handler that throws, and write the violation that the handler
//     receives to stderr as '<kind>: <expression>'
//
// -d  decompress stdin by reading it a character at a time from a
//     gzip::istream over stdin, with the limit of the file name and file
//     comment if specified, and write the result to stdout
//
// --names
//
//     before each character whose member's file name differs from that of
//     the previous character, write the file name in brackets
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

  cin.exceptions (ios_base::badbit);
  cout.exceptions (ios_base::badbit | ios_base::failbit);

  const string_view m (argv[1]);

  if (m == "-c")
  {
    assert (argc >= 3);

    const compression_level l (
      static_cast<compression_level> (stoul (argv[2])));

    header h;
    bool reopen (false);

    for (int i (3); i != argc; ++i)
    {
      const string_view a (argv[i]);

      if (a == "--reopen")
        reopen = true;
      else if (a == "--name")
      {
        assert (i + 1 != argc);
        h.name = argv[++i];
      }
      else
        assert (false);
    }

    gzip::ostream os (cout, l, h);

    if (reopen)
    {
      set_contract_handler (&throwing_handler);

      try
      {
        os.open (cout, l, h);
        assert (false);
      }
      catch (const contract_violation& v)
      {
        println (cerr, "{}: {}", contract_kind_name (v.kind), v.expression);
      }

      set_contract_handler (nullptr);
    }

    for (char c; cin.get (c); )
      os.put (c);

    os.close ();
    cout.flush ();
    return 0;
  }

  assert (m == "-d");

  size_t limit (decompressor::default_max_string);
  bool names (false);

  for (int i (2); i != argc; ++i)
  {
    const string_view a (argv[i]);

    if (a == "--names")
      names = true;
    else if (a == "--limit")
    {
      assert (i + 1 != argc);
      limit = stoul (argv[++i]);
    }
    else
      assert (false);
  }

  try
  {
    gzip::istream is (cin, limit, ios_base::badbit);

    string r;
    optional<string> name;

    for (char c; is.get (c); )
    {
      if (names)
      {
        const optional<header>& h (is.header ());
        assert (h);

        if (h->name != name)
        {
          name = h->name;
          r += '[' + name.value_or ("") + ']';
        }
      }

      r += c;
    }

    is.close ();
    print ("{}", r);
  }
  catch (const invalid_data& e)
  {
    println (cerr, "error: {}", e.what ());
    return 1;
  }
}
