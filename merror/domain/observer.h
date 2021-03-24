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

#ifndef MERROR_5EDA97_DOMAIN_OBSERVER_H_
#define MERROR_5EDA97_DOMAIN_OBSERVER_H_

#include <type_traits>

namespace merror {

namespace internal_observer {

template <class Base>
struct Stub : Base {
  template <class RetVal>
  void ObserveRetVal(const RetVal& ret_val) {}
};

template <class Base>
constexpr bool HasStub(Stub<Base>*) {
  return true;
}

constexpr bool HasStub(void*) { return false; }

}  // namespace internal_observer

// Builder extensions with side effects on errors should inherit from
// `Observer<Base>` and define `ObserveRetVal()` that calls
// `ObserveRetVal()` from the base class at the very end.
//
//   // Builder extension that logs the function's return value whenever an
//   // error is detected.
//   template <class Base>
//   struct RetValPrinter : Observer<Base> {
//     template <class RetVal>
//     void ObserveRetVal(const RetVal& ret_val) {
//       LOG(ERROR) << "Error detected. Return value: " << ret_val;
//       Observer<Base>::ObserveRetVal(ret_val);
//     }
//   };
//
//   constexpr auto PrintRetVal = Builder<RetValPrinter>();
//
//   Status Foo();
//
//   Status Bar() {
//     static constexpr auto MErrorDomain =
//         merror::MErrorDomain().With(PrintRetVal);
//     // Logs the return value on error.
//     MVERIFY(Foo());
//     ...
//     return Status::OK;
//   }
//
// The error domain must call `ObserveRetVal(ret_val)` when an error is detected
// and right before `ret_val` is returned from the current function. The
// `merror::Return()` extension -- a part of `merror::MErrorDomain()` --
// provides this guarantee.
template <class Base>
using Observer =
    typename std::conditional<internal_observer::HasStub(
                                  static_cast<Base*>(nullptr)),
                              Base, internal_observer::Stub<Base>>::type;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_OBSERVER_H_
