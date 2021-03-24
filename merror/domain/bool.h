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
// This library defines error domain extensions for dealing with `bool` as an
// error type (`false` is an error). They are a part of
// `merror::MErrorDomain()`.
//
//   bool DoStuff();
//
//   bool Foo(int x) {
//     MVERIFY(x > 0);
//     MVERIFY(DoStuff());
//     return true;
//   }

#ifndef MERROR_5EDA97_DOMAIN_BOOL_H_
#define MERROR_5EDA97_DOMAIN_BOOL_H_

#include <type_traits>

#include "merror/domain/base.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/return.h"

namespace merror {

namespace internal_bool {

struct Acceptor {
  bool IsError() const { return !value; }
  std::false_type GetCulprit() const { return {}; }
  bool value;
};

template <class Base>
struct AcceptBool : Hook<Base> {
  using Hook<Base>::MVerify;

  // Doesn't accept types convertible to bool. Otherwise the following would
  // compile and work unexpectedly.
  //
  //   MVERIFY(util::error::CANCELLED);  // passes because CANCELLED = 1
  //   MVERIFY(util::error::OK);         // fails because OK = 0
  //   MVERIFY(nice(19));                // the opposite of the intended check
  template <class R>
  Acceptor MVerify(Ref<R, bool> val) const {
    return {val.Get()};
  }
};

template <class Base>
struct MakeBool : Hook<Base> {
  using Hook<Base>::MakeMError;

  template <class Culprit>
  bool MakeMError(ResultType<bool>, const Culprit& culprit) const {
    return false;
  }
};

}  // namespace internal_bool

// Error domain extension that enables MVERIFY() to accept bool as an argument.
using AcceptBool = Policy<internal_bool::AcceptBool>;

// Error domain extension that enables merror macros to return bool on error.
using MakeBool = Builder<internal_bool::MakeBool>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_BOOL_H_
