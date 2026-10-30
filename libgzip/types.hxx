// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <format>
#include <utility>   // forward()
#include <stdexcept> // invalid_argument

#include <libdeflate/types.hxx>

namespace gzip
{
  // Data that doesn't conform to the gzip format, with the description of
  // the fault as the message. This includes the faults in the deflate data
  // of a member. For example:
  //
  // throw invalid_data ("invalid compression method {}", m);
  //
  // Note that the class has only inline members and so isn't exported (see
  // export.hxx for the reason).
  //
  class invalid_data: public std::invalid_argument
  {
  public:
    template <std::formattable<char>... A>
    explicit
    invalid_data (std::format_string<A...> f, A&&... a)
      : std::invalid_argument (std::format (f, std::forward<A> (a)...)) {}
  };

  // Number of bytes that a step of the compressor or decompressor took from
  // its input and wrote to its output.
  //
  using deflate::progress;
}
