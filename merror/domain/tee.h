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
// Defines `Tee`. This error domain extension adds method `Tee()` to the
// policy and the builder. When `Tee(sink)` is called, the sink gets stored by
// value in the policy/builder. Later, if an error is detected, it gets passed
// to the sink before returning. The sink can be a pointer, a functor with no
// parameters or a functor with one parameter.
//
// When `Tee()` is called multiple times, all sinks are stored. In the case of
// an error the sinks are used in the same order as they were passed.
//
// The sink's parameter type doesn't have to be the same as culprit (a.k.a.
// source error type) or the result error type.
//
//   void Async(int n, std::function<Status> f) {
//     const auto MErrorDomain = merror::MErrorDomain().Tee(f).Return();
//     // Unless `n > 0` is true, assembles an error status and passes it to
//     // `f()`, then returns.
//     //
//     // Note the three different error types:
//     //
//     //   * Culprit (the argument of MVERIFY): `bool`.
//     //   * Result (what we are returning): `void`.
//     //   * Sink: `Status`.
//     MVERIFY(n > 0).ErrorCode(INVALID_ARGUMENT);
//     ...
//   }
//
// `NoTee()` removes all previously stored sinks from the policy/builder.

#ifndef MERROR_5EDA97_DOMAIN_TEE_H_
#define MERROR_5EDA97_DOMAIN_TEE_H_

#include <assert.h>

#include <tuple>
#include <type_traits>
#include <utility>

#include "merror/domain/base.h"
#include "merror/domain/defer.h"
#include "merror/domain/observer.h"
#include "merror/domain/return.h"
#include "merror/types.h"

namespace merror {

namespace internal_tee {

struct TeeAnnotation {};

template <size_t I>
struct TeeAll {
  template <class Builder, class... Sinks>
  void operator()(const Builder& builder, std::tuple<Sinks...>&& sinks) const {
    const auto& culprit = builder.context().culprit;
    builder.FillError(culprit, std::get<I - 1>(std::move(sinks)));
    TeeAll<I - 1>()(builder, std::move(sinks));
  }
};

template <>
struct TeeAll<0> {
  template <class Builder, class... Sinks>
  void operator()(const Builder&, std::tuple<Sinks...>&&) const {}
};

template <class Base>
struct Policy : Base {
  template <class Out>
  constexpr auto Tee(Out&& out) const
      -> decltype(AddAnnotation<TeeAnnotation>(*this, std::forward<Out>(out))) {
    return AddAnnotation<TeeAnnotation>(*this, std::forward<Out>(out));
  }

  template <class X = void>
  constexpr auto NoTee() const
      -> decltype(RemoveAnnotations<TeeAnnotation>(Defer<X>(*this))) {
    return RemoveAnnotations<TeeAnnotation>(*this);
  }
};

template <class Base>
struct Builder : Observer<Base> {
  template <class Out>
  auto Tee(Out&& out) && -> decltype(AddAnnotation<TeeAnnotation>(
      std::move(*this), std::forward<Out>(out))) {
    return AddAnnotation<TeeAnnotation>(std::move(*this),
                                        std::forward<Out>(out));
  }

  template <class X = void>
  auto NoTee() && -> decltype(RemoveAnnotations<TeeAnnotation>(
      std::move(Defer<X>(*this)))) {
    return RemoveAnnotations<TeeAnnotation>(std::move(*this));
  }

  template <class RetVal>
  void ObserveRetVal(const RetVal& ret_val) {
    auto sinks = GetAnnotations<TeeAnnotation>(std::move(*this));
    TeeAll<std::tuple_size<decltype(sinks)>::value>()(this->derived(),
                                                      std::move(sinks));
    Observer<Base>::ObserveRetVal(ret_val);
  }
};

}  // namespace internal_tee

// Error domain extension that adds `Tee()` to the policy and the builder.
// See comments at the top of the file for details.
using Tee = Domain<internal_tee::Policy, internal_tee::Builder>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_TEE_H_
