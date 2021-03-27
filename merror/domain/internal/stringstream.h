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

#ifndef MERROR_5EDA97_DOMAIN_INTERNAL_STRINGSTREAM_H_
#define MERROR_5EDA97_DOMAIN_INTERNAL_STRINGSTREAM_H_

#include <ostream>
#include <streambuf>

namespace merror::internal {

class StringStream : private std::basic_streambuf<char>, public std::ostream {
 public:
  // Export the same types as ostringstream does; for use info, see
  // http://en.cppreference.com/w/cpp/io/basic_ostringstream
  typedef std::string::allocator_type allocator_type;

  // These types are defined in both basic_streambuf and ostream, and although
  // they are identical, they cause compiler ambiguities, so we define them to
  // avoid that.
  using std::ostream::char_type;
  using std::ostream::int_type;
  using std::ostream::off_type;
  using std::ostream::pos_type;
  using std::ostream::traits_type;

  // The argument can be null, in which case you'll need to call str(p) with a
  // non-null argument before you can write to the stream.
  //
  // The destructor of StringStream doesn't use the string. It's OK to destroy
  // the string before the stream.
  explicit StringStream(std::string* s) : std::ostream(this), s_(s) {}

  std::string* str() { return s_; }
  const std::string* str() const { return s_; }
  void str(std::string* s) { s_ = s; }

  // These functions are defined in both basic_streambuf and ostream, but it's
  // ostream definition that affects the strings produced.
  using std::ostream::getloc;
  using std::ostream::imbue;

 private:
  using Buf = std::basic_streambuf<char>;

  Buf::int_type overflow(int c) override {
    assert(s_);
    if (!Buf::traits_type::eq_int_type(c, Buf::traits_type::eof()))
      s_->push_back(static_cast<char>(c));
    return 1;
  }
  std::streamsize xsputn(const char* s, std::streamsize n) override {
    assert(s_);
    s_->append(s, n);
    return n;
  }

  std::string* s_;
};

}  // namespace merror::internal

#endif  // MERROR_5EDA97_DOMAIN_INTERNAL_STRINGSTREAM_H_
