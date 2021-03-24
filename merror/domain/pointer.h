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
// This library defines error domain extensions for dealing with pointers, both
// dumb and smart, as errors (`nullptr` is an error). They are a part of
// `merror::MErrorDomain()`.
//
// `MTRY(ptr)` returns an error from the current function if `ptr` is null.
// Otherwise it evaluates to `ptr`. It doesn't dereference the pointer for you.
//
//   std::unique_ptr<int> DoStuff();
//
//   std::unique_ptr<string> Foo(int x) {
//     MVERIFY(x > 0);
//     int n = *MTRY(DoStuff());
//     return gtl::MakeUnique<string>(StrCat(n));
//   }

#ifndef MERROR_5EDA97_DOMAIN_POINTER_H_
#define MERROR_5EDA97_DOMAIN_POINTER_H_

#include <assert.h>

#include <type_traits>

#include "merror/domain/base.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/return.h"

namespace merror {

namespace internal_pointer {

template <bool C>
using EnableIf = typename std::enable_if<C, int>::type;

// T is considered a smart pointer if it models the following concept:
//
//   class T {
//    public:
//     using element_type = <unspecified>;
//     element_type* operator->();
//     element_type* operator*();
//   };
//
//   bool operator==(const T&, std::nullptr_t);
//
// The existence of element_type is necessary to exclude optional<U*> and other
// value wrappers.
template <class T>
using SmartPtrEnabler =
    EnableIf<std::is_same<typename T::element_type*,
                          decltype(std::declval<T&>().operator->())>::value &&
             std::is_same<typename T::element_type&,
                          decltype(*std::declval<T&>())>::value &&
             std::is_convertible<decltype(std::declval<const T&>() == nullptr),
                                 bool>::value>;

struct VerifyAcceptor {
  template <class Ptr>
  explicit VerifyAcceptor(const Ptr& ptr) : is_null(ptr == nullptr) {}
  bool IsError() const { return is_null; }
  std::nullptr_t GetCulprit() && { return nullptr; }
  bool is_null;
};

template <class Ptr>
struct TryAcceptor {
  bool IsError() const {
    const auto& p = ptr;
    return p == nullptr;
  }
  std::nullptr_t GetCulprit() && { return nullptr; }
  decltype(*std::declval<Ptr>()) GetValue() && {
    return *std::forward<Ptr>(ptr);
  }
  Ptr&& ptr;
};

template <class Base>
struct AcceptPointer : Hook<Base> {
  using Hook<Base>::MVerify;
  using Hook<Base>::MTry;

  template <class R, class T>
  VerifyAcceptor MVerify(Ref<R, T*> ptr) const {
    return VerifyAcceptor(ptr.Get());
  }

  template <class R, class V, class C>
  VerifyAcceptor MVerify(Ref<R, V C::*> ptr) const {
    return VerifyAcceptor(ptr.Get());
  }

  template <class R, class T>
  TryAcceptor<R> MTry(Ref<R, T*> ptr) const {
    return TryAcceptor<R>{ptr.Forward()};
  }

  template <class R, class T, SmartPtrEnabler<T> = 0>
  TryAcceptor<R> MTry(Ref<R, T> ptr) const {
    return TryAcceptor<R>{ptr.Forward()};
  }
};

template <class Base>
struct MakePointer : Hook<Base> {
  using Hook<Base>::MakeMError;

  template <class T, class Culprit>
  T* MakeMError(ResultType<T*>, const Culprit&) const {
    return nullptr;
  }
  template <class V, class C, class Culprit>
  V C::*MakeMError(ResultType<V C::*>, const Culprit&) const {
    return nullptr;
  }
  template <class T, class Culprit, SmartPtrEnabler<T> = 0>
  T MakeMError(ResultType<T>, const Culprit&) const {
    auto res = T();
    assert(res == nullptr);
    return res;
  }
  template <class Culprit>
  std::nullptr_t MakeMError(ResultType<std::nullptr_t>, const Culprit&) const {
    return nullptr;
  }
};

}  // namespace internal_pointer

// Error domain extension that enables MTRY() to accept pointers and smart
// pointer arguments.
using AcceptPointer = Policy<internal_pointer::AcceptPointer>;

// Error domain extension that enables merror macros to return pointers and
// smart pointers on error.
using MakePointer = Builder<internal_pointer::MakePointer>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_POINTER_H_
