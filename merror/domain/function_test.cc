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
#include "merror/domain/function.h"

#include <cstddef>
#include <type_traits>

#include "gtest/gtest.h"
#include "merror/domain/base.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/observer.h"
#include "merror/domain/return.h"
#include "merror/macros.h"

namespace merror {
namespace {

template <class Base>
struct CulpritChecker : Base {
  template <class Sig>
  void ObserveRetVal(const std::function<Sig>& fn) {
    static_assert(
        std::is_same<typename Base::ContextType::Culprit, std::nullptr_t&&>(),
        "");
    Base::ObserveRetVal(fn);
  }
};

constexpr auto MErrorDomain =
    EmptyDomain()
        .With(MethodHooks(), Return(), AcceptFunction(), MakeFunction(),
              Builder<CulpritChecker>())
        .Return<std::function<void()>>();

TEST(Function, Works) {
  bool passed;
  auto F = [&](std::function<void()> val) {
    passed = false;
    auto f = MTRY(val);
    passed = true;
    return f;
  };

  EXPECT_NE(F([] {}), nullptr);
  EXPECT_TRUE(passed);
  EXPECT_EQ(F(nullptr), nullptr);
  EXPECT_FALSE(passed);
}

}  // namespace
}  // namespace merror
