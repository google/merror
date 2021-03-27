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
// This library defines `Print()`. It's an error domain extension that allows
// other extensions to stringize objects for the sake of generating
// human-readable error descriptions. It adds the following method to the policy
// and the builder:
//
//   template <class T>
//   void PrintTo(const T& obj, std::ostream* strm) const;
//
// Other extensions can either call this method directly or use a helper
// function `TryPrint()` that evaluates to `PrintTo()` if possible and does
// nothing otherwise. `TryPrint()` allows extensions to degrade gracefully if
// the error domain doesn't support stringification.
//
// `PrintTo()` provided by the `Print()` extension works for streamable objects.
// Extensions can add specific `PrintTo()` methods.
//

#ifndef MERROR_5EDA97_DOMAIN_PRINT_H_
#define MERROR_5EDA97_DOMAIN_PRINT_H_

#include <assert.h>

#include <iosfwd>
#include <type_traits>
#include <utility>

#include "merror/domain/base.h"

namespace merror {

namespace internal_print {

template <class Base>
struct Printer : Base {
  template <class T, class E = decltype(std::declval<std::ostream&>()
                                        << std::declval<const T&>())>
  void PrintTo(const T& obj, std::ostream* strm) const {
    *strm << obj;
  }
};

template <class Base>
struct Policy : Printer<Base> {};

template <class Base>
struct Builder : Printer<Base> {};

template <
    class PolicyOrBuilder, class T,
    class = decltype(std::declval<const PolicyOrBuilder&>().derived().PrintTo(
        std::declval<const T&>(), std::declval<std::ostream*>()))>
std::true_type TryPrintImpl(const PolicyOrBuilder& x, const T& obj,
                            std::ostream* strm, int) {
  x.derived().PrintTo(obj, strm);
  return {};
}

template <class PolicyOrBuilder, class T>
std::false_type TryPrintImpl(const PolicyOrBuilder&, const T&, std::ostream*,
                             unsigned) {
  return {};
}

}  // namespace internal_print

// Error domain extension that allows other extensions to stringize objects.
using Print = Domain<internal_print::Policy, internal_print::Builder>;

// The result type of `TryPrint<PolicyOrBuilder, T>`. See below.
template <class PolicyOrBuilder, class T>
using CanPrint = decltype(internal_print::TryPrintImpl(
    std::declval<const PolicyOrBuilder&>(), std::declval<const T&>(),
    std::declval<std::ostream*>(), 0));

// If `x.PrintTo(obj, strm)` is a valid expression, evaluates it and returns
// std::true_type. Otherwise returns `std::false_type`.
template <class PolicyOrBuilder, class T>
CanPrint<PolicyOrBuilder, T> TryPrint(const PolicyOrBuilder& x, const T& obj,
                                      std::ostream* strm) {
  return internal_print::TryPrintImpl(x, obj, strm, 0);
}

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_PRINT_H_
