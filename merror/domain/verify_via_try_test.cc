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

#include "merror/domain/verify_via_try.h"

#include <utility>

#include "gtest/gtest.h"
#include "merror/macros.h"

namespace merror {
namespace {

// Treats `true` as error. And takes any convertible to bool.
template <class Base>
struct AcceptCrazyBool : Base {
  struct Acceptor {
    bool IsError() const { return val; }
    bool GetCulprit() const { return true; }
    bool val;
  };
  template <class R, class T>
  Acceptor Verify(Ref<R, T> val) const {
    return {static_cast<bool>(val.Get())};
  }
};

// Treats `false` as error. Only takes bool.
template <class Base>
struct AcceptBool : Base {
  struct Acceptor {
    bool IsError() const { return !val; }
    bool GetCulprit() const { return false; }
    bool val;
  };
  template <
      class R, class T,
      typename std::enable_if<std::is_same<T, bool>::value, int>::type = 0>
  Acceptor Verify(Ref<R, T> val) const {
    return {val.Get()};
  }
};

template <class Base>
struct AcceptPointer : Base {
  template <class T>
  struct Acceptor {
    bool IsError() const { return ptr == nullptr; }
    std::nullptr_t GetCulprit() const { return nullptr; }
    T& GetValue() { return *ptr; }
    T* ptr;
  };
  template <class R, class T>
  Acceptor<T> Try(Ref<R, T*> val) const {
    return {val.Get()};
  }
};

template <class Base>
struct ReturnVoid : Base {
  void BuildError() && {
    using Culprit = decltype(this->context().culprit);
    static_assert(std::is_same<Culprit, std::nullptr_t&&>() ||
                      std::is_same<Culprit, bool&&>(),
                  "");
  }
};

constexpr auto MErrorDomain =
    EmptyDomain().With(Policy<AcceptCrazyBool>(), Policy<AcceptPointer>(),
                       Builder<ReturnVoid>(), VerifyViaTry());

TEST(VerifyViaTry, LvaluePointer) {
  bool passed;
  auto F = [&](const char* s) {
    passed = false;
    MVERIFY(s);
    passed = true;
  };
  F("");
  EXPECT_TRUE(passed);
  F(nullptr);
  EXPECT_FALSE(passed);
}

TEST(VerifyViaTry, RvaluePointer) {
  bool passed;
  auto F = [&](const char* s) {
    passed = false;
    MVERIFY(std::move(s));
    passed = true;
  };
  F("");
  EXPECT_FALSE(passed);
  F(nullptr);
  EXPECT_TRUE(passed);
}

TEST(VerifyViaTry, LvalueBool) {
  bool passed;
  auto F = [&](bool val) {
    passed = false;
    MVERIFY(val);
    passed = true;
  };
  F(true);
  EXPECT_FALSE(passed);
  F(false);
  EXPECT_TRUE(passed);
}

TEST(VerifyViaTry, LvalueBooNormal) {
  constexpr auto MErrorDomain =
      EmptyDomain().With(Policy<AcceptBool>(), Policy<AcceptPointer>(),
                         Builder<ReturnVoid>(), VerifyViaTry());
  bool passed;
  auto F = [&](bool val) {
    passed = false;
    MVERIFY(val);
    passed = true;
  };
  F(true);
  EXPECT_TRUE(passed);
  F(false);
  EXPECT_FALSE(passed);
}

TEST(VerifyViaTry, LvaluePointerNormal) {
  constexpr auto MErrorDomain =
      EmptyDomain().With(Policy<AcceptBool>(), Policy<AcceptPointer>(),
                         Builder<ReturnVoid>(), VerifyViaTry());
  bool passed;
  auto F = [&](const char* s) {
    passed = false;
    MVERIFY(s);
    passed = true;
  };
  F("");
  EXPECT_TRUE(passed);
  F(nullptr);
  EXPECT_FALSE(passed);
}

}  // namespace
}  // namespace merror
