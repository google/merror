// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file is internal to merror. Don't include it directly and don't use
// anything that it defines.

#ifndef MERROR_5EDA97_DOMAIN_INTERNAL_INDENTING_STREAM_H_
#define MERROR_5EDA97_DOMAIN_INTERNAL_INDENTING_STREAM_H_

#include <stddef.h>

#include <ostream>
#include <streambuf>
#include <string>

namespace merror {
namespace internal {

// Similar to std::ostringstream, plus automatic indentation.
class IndentingStream : private std::basic_streambuf<char>,
                        public std::ostream {
 public:
  IndentingStream() : std::ostream(this) {}

  // Returns the content of the stream.
  std::string& str() { return s_; }

  // Indents all future lines written to the stream by this many spaces.
  // Indentation happens when writing the first character of a non-empty line.
  void indent(size_t n) { indent_ = n; }

 private:
  using Buf = std::basic_streambuf<char>;

  Buf::int_type overflow(int c = Buf::traits_type::eof()) override {
    if (!Buf::traits_type::eq_int_type(c, Buf::traits_type::eof())) append(c);
    return 1;
  }

  std::streamsize xsputn(const char* s, std::streamsize n) override {
    for (const char* end = s + n; s != end; ++s) append(*s);
    return n;
  }

  void append(char c) {
    // If it's the first character on the line, and it's not \n, indent.
    if ((s_.empty() || s_.back() == '\n') && c != '\n') {
      s_.append(indent_, ' ');
    }
    s_.push_back(c);
  }

  std::string s_;
  size_t indent_ = 0;
};

}  // namespace internal
}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_INTERNAL_INDENTING_STREAM_H_
