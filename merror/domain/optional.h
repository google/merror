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
// This library defines error domain extensions for dealing with
// `std::optional<T>` as an error type (`absl::nullopt` is an error). They are
// a part of `merror::MErrorDomain()`.
//
// `MTRY(opt)` returns an error from the current function if `opt` is `nullopt`.
// Otherwise it evaluates to `*opt`.
//
//   std::optional<int> DoStuff();
//
//   std::optional<string> Foo(int x) {
//     MVERIFY(x > 0);
//     return StrCat(MTRY(DoStuff()));
//   }

#ifndef MERROR_5EDA97_DOMAIN_OPTIONAL_H_
#define MERROR_5EDA97_DOMAIN_OPTIONAL_H_

#include <optional>
#include <type_traits>
#include <utility>

#include "merror/domain/base.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/return.h"

namespace merror {

namespace internal_optional {

template <class Opt>
struct Acceptor {
  bool IsError() const { return opt == std::nullopt; }
  decltype(*std::declval<Opt>()) GetValue() && {
    return *std::forward<Opt>(opt);
  }
  std::nullopt_t GetCulprit() && { return std::nullopt; }
  Opt&& opt;
};

template <class Base>
struct AcceptOptional : Hook<Base> {
  using Hook<Base>::MTry;

  template <class R, class T>
  Acceptor<R> MTry(Ref<R, std::optional<T>> val) const {
    return {val.Forward()};
  }
};

template <class Base>
struct MakeOptional : Hook<Base> {
  using Hook<Base>::MakeMError;

  template <class T, class Culprit>
  std::optional<T> MakeMError(ResultType<std::optional<T>>,
                               const Culprit& culprit) const {
    return std::nullopt;
  }

  template <class Culprit>
  std::nullopt_t MakeMError(ResultType<std::nullopt_t>,
                             const Culprit& culprit) const {
    return std::nullopt;
  }
};

}  // namespace internal_optional

// Error domain extension that enables MTRY() to accept std::optional<T> as an
// argument.
using AcceptOptional = Policy<internal_optional::AcceptOptional>;

// Error domain extension that enables merror macros to return std::optional<T>
// on error.
using MakeOptional = Builder<internal_optional::MakeOptional>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_OPTIONAL_H_
