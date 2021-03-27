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

#include "merror/domain/forward.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "merror/domain/base.h"
#include "merror/macros.h"

namespace merror {
namespace {

template <class Ptr>
struct PtrAcceptor {
  Ptr&& ptr;
  bool IsError() const { return ptr == nullptr; }
  std::nullptr_t GetCulprit() const { return nullptr; }
  auto GetValue() && -> decltype(*std::forward<Ptr>(ptr)) {
    return *std::forward<Ptr>(ptr);
  }
};

struct VoidAcceptor {
  bool IsError() const { return false; }
  std::nullptr_t GetCulprit() const { return nullptr; }
  int GetValue() { return 42; }
};

template <class Base>
struct Accept : Base {
  template <class R, class Ptr>
  PtrAcceptor<R> Try(Ref<R, Ptr> val) const {
    return {val.Forward()};
  }
  VoidAcceptor Try(Ref<Void&&, Void>) const { return {}; }
};

template <class Base>
struct ReturnVoid : Base {
  void BuildError() && {
    using Culprit = decltype(this->context().culprit);
    static_assert(std::is_same<Culprit, std::nullptr_t&&>(), "");
  }
};

constexpr auto MErrorDomain =
    EmptyDomain().With(Forward(), Policy<Accept>(), Builder<ReturnVoid>());

TEST(Forward, Regular) {
  bool passed;
  auto F = [&](const char* s) {
    passed = false;
    const char& q = MTRY(s);
    EXPECT_EQ(s, &q);
    passed = true;
  };
  F("");
  EXPECT_TRUE(passed);
  F(nullptr);
  EXPECT_FALSE(passed);
}

TEST(Forward, Override) {
  bool passed;
  auto F = [&](const char* s) {
    MERROR_DOMAIN().Forward();
    passed = false;
    const char* q = MTRY(s);
    EXPECT_EQ(s, q);
    passed = true;
  };
  F("");
  EXPECT_TRUE(passed);
  F(nullptr);
  EXPECT_FALSE(passed);
}

TEST(Forward, Void) {
  auto F = []() {
    MERROR_DOMAIN().Forward();
    return MTRY(static_cast<void>(0));
  };
  F();
  static_assert(std::is_same<decltype(F()), void>(), "");
}

template <class Expected, class Actual>
void ExpectSame(Expected&& expected, Actual&& actual) {
  static_assert(std::is_same<Expected, Actual>(), "");
  EXPECT_EQ(&expected, &actual);
}

TEST(Forward, PerfectForwarding) {
  bool passed = false;
  auto F = [&]() {
    MERROR_DOMAIN().Forward();
    std::unique_ptr<int> x(new int);
    const std::unique_ptr<int> y(new int);
    ExpectSame(x, MTRY(x));
    ExpectSame(y, MTRY(y));
    ExpectSame(std::move(x), MTRY(std::move(x)));
    ExpectSame(std::move(y), MTRY(std::move(y)));
    passed = true;
  };
  F();
  EXPECT_TRUE(passed);
}

}  // namespace
}  // namespace merror
