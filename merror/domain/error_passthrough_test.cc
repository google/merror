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

#include "merror/domain/error_passthrough.h"

#include <utility>

#include "gtest/gtest.h"
#include "merror/domain/base.h"
#include "merror/domain/defer.h"
#include "merror/domain/return.h"
#include "merror/macros.h"

namespace merror {
namespace {

enum class MyError {
  kOk,
  kFail,
  kUnknown,
};

struct ErrorAcceptor {
  bool IsError() const { return error != MyError::kOk; }
  MyError GetCulprit() const { return error; }
  MyError error;
};

struct BoolAcceptor {
  bool IsError() const { return !val; }
  bool GetCulprit() const { return false; }
  bool val;
};

template <class Base>
struct Accept : Base {
  template <class R>
  ErrorAcceptor Verify(Ref<R, MyError> error) const {
    return {error.Get()};
  }
  template <class R>
  BoolAcceptor Verify(Ref<R, bool> val) const {
    return {val.Get()};
  }
};

template <class Base>
struct ProduceError : Base {
  MyError MakeError(ResultType<MyError>, bool) const {
    return MyError::kUnknown;
  }

  // Fallbacks for FillError.
  void FillError(bool, MyError* error) const { *error = MyError::kUnknown; }

  template <class T, class F>
  void FillError(T, F f) const {
    f(MyError::kUnknown);
  }
};

template <class Base>
struct SideError : Base {
  template <class Error>
  auto Fill(
      Error&& error) && -> decltype(std::move(Defer<Error>(this)->derived())) {
    this->derived().FillError(this->context().culprit,
                              std::forward<Error>(error));
    return std::move(this->derived());
  }
};

constexpr auto MErrorDomain =
    EmptyDomain()
        .With(Return(), Policy<Accept>(), Builder<SideError>(),
              Builder<ProduceError>(), ErrorPassthrough())
        .Return<MyError>();

TEST(ErrorPassthrough, Passthrough) {
  bool passed;
  MyError side_error;
  auto F = [&](MyError e) {
    passed = false;
    side_error = MyError::kOk;
    MVERIFY(e).Fill(&side_error);
    passed = true;
    return MyError::kOk;
  };
  EXPECT_EQ(MyError::kOk, F(MyError::kOk));
  EXPECT_EQ(MyError::kOk, side_error);
  EXPECT_TRUE(passed);
  EXPECT_EQ(MyError::kFail, F(MyError::kFail));
  EXPECT_EQ(MyError::kFail, side_error);
  EXPECT_FALSE(passed);
}

TEST(ErrorPassthrough, Fallback) {
  bool passed;
  MyError side_error;
  auto F = [&](bool val) {
    passed = false;
    side_error = MyError::kOk;
    MVERIFY(val).Fill(&side_error);
    passed = true;
    return MyError::kOk;
  };
  EXPECT_EQ(MyError::kOk, F(true));
  EXPECT_EQ(MyError::kOk, side_error);
  EXPECT_TRUE(passed);
  EXPECT_EQ(MyError::kUnknown, F(false));
  EXPECT_EQ(MyError::kUnknown, side_error);
  EXPECT_FALSE(passed);
}

TEST(ErrorPassthrough, FallbackWithLambda) {
  static bool called;
  bool passed;
  auto F = [&](bool val) {
    passed = false;
    MVERIFY(val).Fill([](MyError error) {
      called = true;
      EXPECT_EQ(MyError::kUnknown, error);
    });
    passed = true;
    return MyError::kOk;
  };
  EXPECT_EQ(MyError::kOk, F(true));
  EXPECT_FALSE(called);
  EXPECT_TRUE(passed);
  EXPECT_EQ(MyError::kUnknown, F(false));
  EXPECT_TRUE(called);
  EXPECT_FALSE(passed);
}

}  // namespace
}  // namespace merror
