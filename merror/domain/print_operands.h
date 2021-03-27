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
#include "merror/domain/internal/stringstream.h"
#include "merror/domain/print.h"

namespace merror {

namespace internal_print_operands {

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
    internal::StringStream strm(left_str);
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
