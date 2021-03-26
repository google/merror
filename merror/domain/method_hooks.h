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
// Defines domain extension `MethodHooks()` and class `Hook<Base>` for the hook
// providers to inherit from. See comments at the bottom of the file.

#ifndef MERROR_5EDA97_DOMAIN_METHOD_HOOKS_H_
#define MERROR_5EDA97_DOMAIN_METHOD_HOOKS_H_

#include <type_traits>
#include <utility>

#include "merror/domain/base.h"
#include "merror/domain/defer.h"
#include "merror/domain/return.h"

namespace merror {

namespace internal_method_hooks {

template <class Base>
struct Policy;

template <class Base>
struct Builder;

namespace impl {

template <class Base, class Ref>
auto Verify(const Policy<Base>& p, Ref&& ref, int)
    -> decltype(p.derived().MVerify(std::forward<Ref>(ref))) {
  return p.derived().MVerify(std::forward<Ref>(ref));
}

template <class Base, class Ref>
auto Verify(const Policy<Base>& p, Ref&& ref, unsigned)
    -> decltype(p.Base::Verify(std::forward<Ref>(ref))) {
  return p.Base::Verify(std::forward<Ref>(ref));
}

template <class Base, class Ref>
auto Try(const Policy<Base>& p, Ref&& ref, int)
    -> decltype(p.derived().MTry(std::forward<Ref>(ref))) {
  return p.derived().MTry(std::forward<Ref>(ref));
}

template <class Base, class Ref>
auto Try(const Policy<Base>& p, Ref&& ref, unsigned)
    -> decltype(p.Base::Try(std::forward<Ref>(ref))) {
  return p.Base::Try(std::forward<Ref>(ref));
}

template <class Base, class R, class Culprit>
auto MakeError(const Builder<Base>& b, ResultType<R> r, const Culprit& culprit,
               int) -> decltype(b.derived().MakeMError(r, culprit)) {
  return b.derived().MakeMError(r, culprit);
}

template <class Base, class R, class Culprit>
R MakeError(const Builder<Base>& b, ResultType<R> r, const Culprit& culprit,
            unsigned) {
  return b.Base::MakeError(r, culprit);
}

template <class Base, class Culprit, class E>
auto FillError(const Builder<Base>& b, const Culprit& culprit, E&& error, int)
    -> decltype(b.derived().FillMError(culprit, std::forward<E>(error))) {
  return b.derived().FillMError(culprit, std::forward<E>(error));
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

template <class Base>
struct Stub : Base {
  void MVerify() const;
  void MTry() const;
  void MakeMError() const;
  void FillMError() const;
};

template <class Base>
constexpr bool HasStub(Stub<Base>*) {
  return true;
}

constexpr bool HasStub(void*) { return false; }

}  // namespace internal_method_hooks

// Implements methods `Verify()` and `Try()` on the policy and 'MakeError()` and
// `FillError()` on the builder.
//
// `policy.Verify(val)` resolves to `policy.MVerify(val)` if it'll accept the
// argument, and falls through to `Verify(val)` from the base class otherwise.
// Other methods are implemented similarly.
//
//    Implements | Calls
// --------------+---------------
//        Verify | MVerify
//           Try | MTry
//     MakeError | MakeMError
//     FillError | FillMError
// --------------+---------------
using MethodHooks =
    Domain<internal_method_hooks::Policy, internal_method_hooks::Builder>;

// Policy extensions that define `MVerify()` or `MTry()`, and builder extensions
// that define `MakeMError()` or `FillMError()` should inherit from `Hook<Base>`
// and bring all methods from `Hook<Base>` into the extension scope for which
// they introduce overloads.
//
//   // Classifies `false` arguments of `MVERIFY()` as errors.
//   template <class Base>
//   struct AcceptBool : Hook<Base> {
//     using Hook<Base>::MVerify;
//     struct Acceptor {
//       bool IsError() const { return !val; }
//       bool GetCulprit() const { return false; }
//       bool val;
//     };
//     bool MVerify(bool val) const { return {val}; }
//   };
//
//   // Instructs the merror macros to return `false` on error.
//   template <class Base>
//   struct ReturnBool : Hook<Base> {
//     using Hook<Base>::MakeMError;
//     template <class Culprit>
//     bool MakeMError(ResultType<bool>, const Culprit&) const {
//       return false;
//     }
//   };
//
//   bool DoStuff(int n) {
//     static constexpr auto MErrorDomain = EmptyDomain()
//         .With(Return(), MethodHooks(), Domain<AcceptBool, ReturnBool>());
//     MVERIFY(n > 0);
//     ...
//     return true;
//   }
//
// The error domain must have `MethodHooks()` in it or the hooks won't be
// called.
template <class Base>
using Hook =
    typename std::conditional<internal_method_hooks::HasStub(
                                  static_cast<Base*>(nullptr)),
                              Base, internal_method_hooks::Stub<Base>>::type;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_METHOD_HOOKS_H_
