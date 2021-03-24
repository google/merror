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
// Defines `FillError()` builder extension.

#ifndef MERROR_5EDA97_DOMAIN_FILL_ERROR_H_
#define MERROR_5EDA97_DOMAIN_FILL_ERROR_H_

#include <cassert>

#include "merror/domain/base.h"
#include "merror/domain/return.h"

namespace merror {

namespace internal_fill_error {

namespace impl {

template <class Builder, class Culprit>
struct AnyError {
  template <class Error>
  operator Error() const {
    return builder.MakeError(ResultType<Error>(), culprit);
  }
  const Builder& builder;
  const Culprit& culprit;
};

template <class Builder, class Culprit, class F>
auto Invoke(const Builder& builder, const Culprit& culprit, F&& f, int)
    -> decltype(
        std::forward<F>(f)(std::declval<AnyError<Builder, Culprit>>())) {
  return std::forward<F>(f)(AnyError<Builder, Culprit>{builder, culprit});
}

template <class Builder, class Culprit, class F>
auto Invoke(const Builder& builder, const Culprit& culprit, F&& f, unsigned)
    -> decltype(std::forward<F>(f)()) {
  return std::forward<F>(f)();
}

template <class Builder, class Culprit, class F, class R, class T>
void FillPtr(const Builder& b, const Culprit& culprit, F* p, R (*f)(T)) {
  f(b.MakeError(ResultType<typename std::decay<T>::type>(), culprit));
}

template <class Builder, class Culprit, class F, class R>
void FillPtr(const Builder&, const Culprit&, F*, R (*f)()) {
  f();
}

template <class Builder, class Culprit, class T>
void FillPtr(const Builder& b, const Culprit& culprit, T* p, void*) {
  *p = b.MakeError(ResultType<T>(), culprit);
}

template <class Builder, class Culprit, class Error>
void FillError(const Builder& builder, const Culprit& culprit, Error* error) {
  assert(error);
  FillPtr(builder, culprit, error, error);
}

template <class Builder, class Culprit, class Error>
void FillError(const Builder& builder, const Culprit& culprit, Error&& error) {
  Invoke(builder, culprit, std::forward<Error>(error), 0);
}

}  // namespace impl

template <class Base>
struct Builder : Base {
  template <class Culprit, class Error>
  void FillError(const Culprit& culprit, Error&& error) const {
    impl::FillError(this->derived(), culprit, std::forward<Error>(error));
  }
};

}  // namespace internal_fill_error

// A builder extension that implements `FillError()` on top of `MakeError()`.
using FillError = Builder<internal_fill_error::Builder>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_FILL_ERROR_H_
