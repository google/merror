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

#include "merror/domain/bool.h"

#include <type_traits>

#include "merror/domain/base.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/observer.h"
#include "merror/domain/return.h"
#include "merror/macros.h"
#include "gtest/gtest.h"

namespace merror {
namespace {

template <class Base>
struct CulpritChecker : Base {
  void ObserveRetVal(bool val) {
    static_assert(
        std::is_same<typename Base::ContextType::Culprit, std::false_type&&>(),
        "");
    Base::ObserveRetVal(val);
  }
};

constexpr auto MErrorDomain = EmptyDomain()
                                  .With(MethodHooks(), Return(), AcceptBool(),
                                        MakeBool(), Builder<CulpritChecker>())
                                  .Return<bool>();

TEST(Bool, Works) {
  bool passed;
  auto F = [&](bool val) {
    passed = false;
    MVERIFY(val);
    passed = true;
    return true;
  };
  EXPECT_TRUE(F(true));
  EXPECT_TRUE(passed);
  EXPECT_FALSE(F(false));
  EXPECT_FALSE(passed);
}

}  // namespace
}  // namespace merror
