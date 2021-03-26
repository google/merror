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
// Defines `VerifyViaTry`. This error domain extension enables `MVERIFY()` to
// accept lvalue arguments that are valid `MTRY()` arguments.

#ifndef MERROR_5EDA97_DOMAIN_VERIFY_VIA_TRY_H_
#define MERROR_5EDA97_DOMAIN_VERIFY_VIA_TRY_H_

#include <utility>

#include "merror/domain/base.h"

namespace merror {

namespace internal_verify_via_try {

template <class Base>
struct Policy;

namespace impl {

template <class Base, class R, class T>
auto Verify(const Policy<Base>& p, Ref<R&, T> ref, int*)
    -> decltype(p.derived().Try(std::move(ref))) {
  return p.derived().Try(std::move(ref));
}

template <class Base, class Ref>
auto Verify(const Policy<Base>& p, Ref&& ref, const int*)
    -> decltype(p.Base::Verify(std::forward<Ref>(ref))) {
  return p.Base::Verify(std::forward<Ref>(ref));
}

template <class Base, class R, class T>
auto Verify(const Policy<Base>& p, Ref<R&&, T> ref, volatile const int*)
    -> decltype(p.derived().Try(std::move(ref))) {
  static_assert(sizeof(T) == 0, "Cannot verify via try rvalues.");
  return p.derived().Try(std::move(ref));
}

}  // namespace impl

template <class Base>
struct Policy : public Base {
  template <class Ref>
  auto Verify(Ref&& ref) const
      -> decltype(impl::Verify(*this, std::forward<Ref>(ref),
                               static_cast<int*>(nullptr))) {
    return impl::Verify(*this, std::forward<Ref>(ref),
                        static_cast<int*>(nullptr));
  }
};

}  // namespace internal_verify_via_try

// Error domain extension that enables `MVERIFY()` to accept lvalue arguments
// that are valid `MTRY()` arguments.
using VerifyViaTry = Policy<internal_verify_via_try::Policy>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_VERIFY_VIA_TRY_H_
