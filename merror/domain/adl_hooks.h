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
// Defines `AdlHooks`. This error domain extension implements methods `Verify()`
// and `Try()` on the policy and 'MakeError()` and `FillError()` on the builder
// via unqualified calls to `MVerify()`, `MTry()`, `MakeMError() and
// `FillMError()`: `policy.Verify(ref)` resolves to the unqualified call to
// `MVerify(policy, ref)` if it's valid, and falls through to `Verify(ref)` from
// the base class otherwise; the other methods are implemented similarly.
//
//                           Implements | Calls
// -------------------------------------+--------------------------------------
//                   policy.Verify(ref) | MVerify(policy, ref)
//                      policy.Try(ref) | MTry(policy, ref)
//        builder.MakeError(r, culprit) | MakeMError(builder, r, culprit)
//     builder.FillError(r, culprit, e) | FillMError(builder, r, culprit, e)
// -------------------------------------+--------------------------------------
//
//                              EXAMPLE
//
// Suppose you've forked `StatusOr` into `StatusOr2` and would like merror
// macros to work with your forked type just like they work with the original.
// You can do that by defining `MTry()` and `MakeMError()` in the same namespace
// as `StatusOr2`.
//
//   // `S` is `[const] [volatile] StatusOr2<T>`.
//   template <class S>
//   auto ValueOrDie(S&& val) -> decltype(std::move(val.ValueOrDie())) {
//     return std::move(val.ValueOrDie());
//   }
//
//   // `S` is `[const] [volatile] StatusOr2<T>`.
//   template <class S>
//   auto ValueOrDie(S& val) -> decltype(val.ValueOrDie()) {
//     return val.ValueOrDie();
//   }
//
//   // `R` is a reference to `[const] [volatile] StatusOr2<T>`.
//   template <class R>
//   struct Acceptor {
//     R status_or;
//     bool IsError() const { return !status_or.ok(); }
//     auto GetValue() && -> decltype(ValueOrDie(std::forward<R>(status_or))) {
//       return ValueOrDie(std::forward<R>(status_or));
//     }
//     const util::Status& GetCulprit() && { return status_or.status(); }
//   };
//
//   // `R` is a reference to `[const] [volatile] StatusOr2<T>`.
//   template <class Policy, class R, class T>
//   Acceptor<R> MTry(const Policy&, merror::Ref<R, StatusOr2<T>> status_or) {
//     return {status_or.Forward()};
//   }
//
//   template <class Builder, class T, class Culprit>
//   StatusOr2<T> MakeMError(const Builder& builder,
//                           merror::ResultType<StatusOr2<T>>,
//                           const Culprit& culprit) {
//     // Piggyback on the existing algorithm for building util::Status.
//     return builder.MakeError(merror::ResultType<util::Status>(), culprit);
//   }
//
// Now you can pass expressions of type `StatusOr2<T>` to `MTRY()` and use all
// merror macros in functions returning `StatusOr2<T>`. Error type conversions
// will happen automatically: you can use `MVERIFY(status)` or `MTRY(status_or)`
// in a function returning `StatusOr2<T>`; or use `MTRY(status_or2)` in a
// function returning `Status`, `StatusOr<T>` or even `bool`.

#ifndef MERROR_5EDA97_DOMAIN_ADL_HOOKS_H_
#define MERROR_5EDA97_DOMAIN_ADL_HOOKS_H_

#include <type_traits>
#include <utility>

#include "merror/domain/base.h"
#include "merror/domain/defer.h"
#include "merror/domain/return.h"

namespace merror {

namespace internal_adl_hooks {

template <class Base>
struct Policy;

template <class Base>
struct Builder;

namespace impl {

template <class Base, class Ref>
auto Verify(const Policy<Base>& p, Ref&& ref, int)
    -> decltype(MVerify(p.derived(), std::forward<Ref>(ref))) {
  return MVerify(p.derived(), std::forward<Ref>(ref));
}

template <class Base, class Ref>
auto Verify(const Policy<Base>& p, Ref&& ref, unsigned)
    -> decltype(p.Base::Verify(std::forward<Ref>(ref))) {
  return p.Base::Verify(std::forward<Ref>(ref));
}

template <class Base, class Ref>
auto Try(const Policy<Base>& p, Ref&& ref, int)
    -> decltype(MTry(p.derived(), std::forward<Ref>(ref))) {
  return MTry(p.derived(), std::forward<Ref>(ref));
}

template <class Base, class Ref>
auto Try(const Policy<Base>& p, Ref&& ref, unsigned)
    -> decltype(p.Base::Try(std::forward<Ref>(ref))) {
  return p.Base::Try(std::forward<Ref>(ref));
}

template <class Base, class R, class Culprit>
auto MakeError(const Builder<Base>& b, ResultType<R> r, const Culprit& culprit,
               int) -> decltype(MakeMError(b.derived(), r, culprit)) {
  return MakeMError(b.derived(), r, culprit);
}

template <class Base, class R, class Culprit>
R MakeError(const Builder<Base>& b, ResultType<R> r, const Culprit& culprit,
            unsigned) {
  return b.Base::MakeError(r, culprit);
}

template <class Base, class Culprit, class E>
auto FillError(const Builder<Base>& b, const Culprit& culprit, E&& error, int)
    -> decltype(FillMError(b.derived(), culprit, std::forward<E>(error))) {
  return FillMError(b.derived(), culprit, std::forward<E>(error));
}

template <class Base, class Culprit, class E>
void FillError(const Builder<Base>& b, const Culprit& culprit, E&& error,
               unsigned) {
  b.Base::FillError(culprit, std::forward<E>(error));
}

}  // namespace impl

template <class Base>
struct Policy : public Base {
  template <class Ref>
  auto Verify(Ref&& ref) const
      -> decltype(impl::Verify(*this, std::forward<Ref>(ref), 0)) {
    return impl::Verify(*this, std::forward<Ref>(ref), 0);
  }

  template <class Ref>
  auto Try(Ref&& ref) const
      -> decltype(impl::Try(*this, std::forward<Ref>(ref), 0)) {
    return impl::Try(*this, std::forward<Ref>(ref), 0);
  }
};

template <class Base>
struct Builder : public Base {
  template <class R, class Culprit>
  auto MakeError(ResultType<R> r, const Culprit& culprit) const
      -> decltype(impl::MakeError(*this, r, culprit, 0)) {
    return impl::MakeError(*this, r, culprit, 0);
  }

  template <class Culprit, class E>
  auto FillError(const Culprit& culprit, E&& error) const
      -> decltype(impl::FillError(*this, culprit, std::forward<E>(error), 0)) {
    return impl::FillError(*this, culprit, std::forward<E>(error), 0);
  }
};

}  // namespace internal_adl_hooks

// Implements methods `Verify()` and `Try()` on the policy and 'MakeError()` and
// `FillError()` on the builder via unqualified calls to `MVerify()`, `MTry()`,
// `MakeMError() and `FillMError()`. See comments at the top of the file for
// details.
//
// `policy.Verify(ref)` resolves to the unqualified call to
// `MVerify(policy, ref)` if it's valid, and falls through to `Verify(ref)` from
// the base class otherwise. Other methods are implemented similarly.
//
//                           Implements | Calls
// -------------------------------------+--------------------------------------
//                   policy.Verify(ref) | MVerify(policy, ref)
//                      policy.Try(ref) | MTry(policy, ref)
//        builder.MakeError(r, culprit) | MakeMError(builder, r, culprit)
//     builder.FillError(r, culprit, e) | FillMError(builder, r, culprit, e)
// -------------------------------------+--------------------------------------
using AdlHooks =
    Domain<internal_adl_hooks::Policy, internal_adl_hooks::Builder>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_ADL_HOOKS_H_
