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
// Defines `Forward`. This error domain extension adds method `Forward()` to the
// policy. When called, it returns a modified version of the error domain that
// behaves just like the original except that `MTRY(expr)` evaluates to `expr`
// without unwrapping (it still returns if `expr` is an error).
//
//   // Returns null on error.
//   std::unique_ptr<T> Make();
//
//   // The argument must not be null.
//   void Consume(std::unique_ptr<T> x);
//
//   bool Churn() {
//     Consume(MTRY(Forward(), Make()));
//     return true;
//   }

#ifndef MERROR_5EDA97_DOMAIN_FORWARD_H_
#define MERROR_5EDA97_DOMAIN_FORWARD_H_

#include <type_traits>

#include "merror/domain/base.h"
#include "merror/domain/defer.h"

namespace merror {

namespace internal_forward {

namespace impl {

template <class T>
T&& GetValue(T&& value) {
  return std::forward<T>(value);
}

inline void GetValue(Void&&) {}

}  // namespace impl

template <class T, class BaseAcceptor>
struct Acceptor {
  T&& value;
  BaseAcceptor acceptor;  // can be a reference

  bool IsError() { return acceptor.IsError(); }

  auto GetCulprit() && -> decltype(
      std::forward<BaseAcceptor>(acceptor).GetCulprit()) {
    return std::forward<BaseAcceptor>(acceptor).GetCulprit();
  }

  auto GetValue() && -> decltype(impl::GetValue(std::forward<T>(value))) {
    return impl::GetValue(std::forward<T>(value));
  }
};

template <class Base>
struct Override : Base {
  template <class R, class T>
  Acceptor<R, decltype(Defer<R>(std::declval<const Base&>())
                           .Try(std::declval<Ref<R, T>>()))>
  Try(Ref<R, T> val) const {
    return {val.Forward(), Base::Try(std::move(val))};
  }
};

template <class Base>
struct Facade : Base {
  template <class X = void>
  constexpr auto Forward() const
      -> decltype(Defer<X>(this)->With(Policy<Override>())) {
    return this->With(Policy<Override>());
  }
};

}  // namespace internal_forward

// Error domain extension that adds method `Forward()` to the policy. See the
// comments at the top of the file for details.
using Forward = Policy<internal_forward::Facade>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_FORWARD_H_
