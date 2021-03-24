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
// Defines `ErrorPassthrough`. This error domain extension implements
// `MakeError()` and `FillError()` by copying the culprit. If the result error
// type isn't the same as the culprit type, it delegates to the base class.
//
// Thanks to `ErrorPassthrough`, you don't have to define trivial `MakeMError()`
// and `FillMError()` functions for your custom error types.
//
//   enum MyError {
//     kSuccess,
//     kBadKarma,
//     kArmaggeddon,
//   };
//
//   struct Acceptor {
//     bool IsError() const { return e != kSuccess; }
//     MyError GetCulprit() const { return e; }
//     MyError e;
//   };
//
//   // This hook is enough to make `MVERIFY()` work with `MyError` arguments
//   // and to use all merror macros in functions returning `MyError`.
//   template <class Policy, class R>
//   Acceptor MVerify(const Policy&, merror::Ref<R, MyError> e) {
//     return {e.Get()};
//   }
//
//   // You don't need to define this trivial function.
//   //
//   //   template <class Builder>
//   //   MyError MakeMError(const Builder&, merror::ResultType<MyError>,
//   //                      MyError culprit) {
//   //     return culprit;
//   //   }
//
//   // You don't need to define this trivial function.
//   //
//   //   template <class Builder>
//   //   void FillMError(const Builder&, MyError culprit, MyError* error) {
//   //     *error = culprit;
//   //   }
//
//   MyError Foo();
//
//   MyError Bar() {
//     MVERIFY(Foo());
//     MVERIFY(Foo());
//     return kSuccess;
//   }

#ifndef MERROR_5EDA97_DOMAIN_ERROR_PASSTHROUGH_H_
#define MERROR_5EDA97_DOMAIN_ERROR_PASSTHROUGH_H_

#include "merror/domain/base.h"
#include "merror/domain/return.h"

namespace merror {

namespace internal_error_passthrough {

template <class Base>
struct Builder : Base {
  template <class R>
  R MakeError(ResultType<R>, const R& culprit) const {
    return culprit;
  }

  template <class R, class Culprit>
  R MakeError(ResultType<R> r, const Culprit& culprit) const {
    return Base::MakeError(r, culprit);
  }

  template <class E>
  void FillError(const E& culprit, E* error) const {
    *error = culprit;
  }

  template <class Culprit, class E>
  void FillError(const Culprit& culprit, E&& error) const {
    Base::FillError(culprit, std::forward<E>(error));
  }
};

}  // namespace internal_error_passthrough

// Implements `MakeError()` and `FillError()` by copying the culprit.
using ErrorPassthrough = Builder<internal_error_passthrough::Builder>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_ERROR_PASSTHROUGH_H_
