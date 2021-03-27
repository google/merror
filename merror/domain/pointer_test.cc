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

#include "merror/domain/pointer.h"

#include <functional>
#include <memory>
#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "merror/domain/return.h"
#include "merror/macros.h"

namespace merror {
namespace {

using ::testing::IsNull;
using ::testing::Pointee;

constexpr auto MErrorDomain =
    EmptyDomain().With(MethodHooks(), Return(), AcceptPointer(), MakePointer());

template <template <class...> class P>
void TestTry() {
  bool passed;
  auto F = [&](P<int> p) -> P<std::string> {
    passed = false;
    std::string res = std::to_string(MTRY(p));
    passed = true;
    return P<std::string>(new std::string(res));
  };
  EXPECT_THAT(F(P<int>(new int(42))), Pointee(std::string("42")));
  EXPECT_TRUE(passed);
  EXPECT_THAT(F(nullptr), IsNull());
  EXPECT_FALSE(passed);
}

TEST(Try, SmartPointer) {
  TestTry<std::unique_ptr>();
  TestTry<std::shared_ptr>();
}

TEST(Try, DumbPointer) {
  bool passed;
  auto F = [&](int* p) -> const char* {
    passed = false;
    EXPECT_EQ(42, MTRY(p));
    passed = true;
    return "42";
  };

  int n = 42;
  EXPECT_STREQ("42", F(&n));
  EXPECT_TRUE(passed);

  EXPECT_THAT(F(nullptr), IsNull());
  EXPECT_FALSE(passed);
}

TEST(Make, NullptrT) {
  []() -> std::nullptr_t { return MERROR(); }();
}

template <class T>
void TestVerify(const T& ptr) {
  bool passed;
  auto F = [&](const T& p) {
    passed = false;
    MVERIFY(p).Return();
    passed = true;
  };
  F(ptr);
  EXPECT_TRUE(passed);
  F(nullptr);
  EXPECT_FALSE(passed);
}

TEST(Verify, DumbPointer) {
  struct S {
    static void F() {}
    void M() {}
    int d;
  } s;
  TestVerify(&s);
  TestVerify(&S::F);
  TestVerify(&S::M);
  TestVerify(&S::d);
}

}  // namespace
}  // namespace merror
