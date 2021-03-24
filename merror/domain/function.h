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

#ifndef MERROR_5EDA97_DOMAIN_FUNCTION_H_
#define MERROR_5EDA97_DOMAIN_FUNCTION_H_

#include <functional>
#include <type_traits>
#include <utility>

#include "merror/domain/base.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/return.h"

namespace merror {

namespace internal_function {

template <class Fun>
struct Acceptor {
  bool IsError() const { return fun == nullptr; }
  Fun&& GetValue() && { return std::forward<Fun>(fun); }
  std::nullptr_t GetCulprit() && { return nullptr; }
  Fun&& fun;
};

template <class Base>
struct AcceptFunction : Hook<Base> {
  using Hook<Base>::MTry;

  template <class R, class Sig>
  Acceptor<R> MTry(Ref<R, std::function<Sig>> val) const {
    return {val.Forward()};
  }
};

template <class Base>
struct MakeFunction : Hook<Base> {
  using Hook<Base>::MakeMError;

  template <class Sig, class Culprit>
  std::function<Sig> MakeMError(ResultType<std::function<Sig>>,
                                const Culprit& culprit) const {
    return nullptr;
  }
};

}  // namespace internal_function

// Error domain extension that enables MTRY() to accept std::function<T> as an
// argument.
using AcceptFunction = Policy<internal_function::AcceptFunction>;

// Error domain extension that enables merror macros to return std::function<T>
// on error.
using MakeFunction = Builder<internal_function::MakeFunction>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_FUNCTION_H_
