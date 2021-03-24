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
//
// This library defines `PrintOperands()`. It's an error domain extension that
// enables printing of operands in `MVERIFY(x relop y)`. It uses the `PrintTo()`
// extension protocol for stringizing objects. See print.h for details.

#ifndef MERROR_5EDA97_DOMAIN_PRINT_OPERANDS_H_
#define MERROR_5EDA97_DOMAIN_PRINT_OPERANDS_H_

#include <assert.h>

#include <iosfwd>
#include <ostream>
#include <streambuf>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "merror/domain/base.h"
#include "merror/domain/print.h"

namespace merror {

namespace internal_print_operands {

// Copied from strings/ostreams to avoid dependencies.
// TODO(iserna): Use this in description.h too.
class OStringStream : private std::basic_streambuf<char>, public std::ostream {
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
  // The destructor of OStringStream doesn't use the string. It's OK to destroy
  // the string before the stream.
  explicit OStringStream(std::string* s) : std::ostream(this), s_(s) {}

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

// If one operand is `[const] char*` and the other is one of the strings classes
// whose relational operators treat `const char*` as nul-terminated string,
// then we can also safely treat the `[const] char*` as nul-terminated string
// while printing it.
template <class T, class Other>
using Operand =
    typename std::conditional<(std::is_same<T, char*>() ||
                               std::is_same<T, const char*>()) &&
                                  (std::is_same<Other, std::string_view>() ||
                                   std::is_same<Other, std::string>() ||
                                   std::is_same<Other, std::string>()),
                              std::string_view, const T&>::type;

template <class Base>
struct Printer : Base {
  template <class L, class R>
  bool PrintOperands(const L& left, const R& right, std::string* left_str,
                     std::string* right_str) const {
    if (!CanPrint<typename Base::PolicyType, Operand<L, R>>() ||
        !CanPrint<typename Base::PolicyType, Operand<R, L>>())
      return false;
    assert(left_str);
    assert(right_str);
    OStringStream strm(left_str);
    merror::TryPrint(this->derived(), Operand<L, R>(left), &strm);
    strm.str(right_str);
    merror::TryPrint(this->derived(), Operand<R, L>(right), &strm);
    return true;
  }
};

}  // namespace internal_print_operands

// Error domain extension that enables printing of operands in
// `MVERIFY(x relop y)`.
using PrintOperands = Policy<internal_print_operands::Printer>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_PRINT_OPERANDS_H_
