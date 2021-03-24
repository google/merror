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

#include "merror/domain/tee.h"

#include <type_traits>
#include <utility>

#include "merror/domain/base.h"
#include "merror/domain/bool.h"
#include "merror/domain/fill_error.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/return.h"
#include "merror/macros.h"
#include "gtest/gtest.h"

namespace merror {
namespace {

constexpr auto MyErrorDomain =
    EmptyDomain()
        .With(Tee(), FillError(), Return(), MethodHooks(), AcceptBool(),
              MakeBool())
        .Return();
constexpr auto MErrorDomain = MyErrorDomain;

TEST(Tee, NoSinks) {
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

TEST(Tee, MultipleSinks) {
  bool passed;
  bool side_error1;
  bool side_error2;
  bool side_error3;
  auto F = [&](bool val) {
    passed = false;
    side_error1 = true;
    side_error2 = true;
    side_error3 = true;
    MVERIFY(val)
        .Tee([&side_error1](bool val) { EXPECT_FALSE((side_error1 = val)); })
        .Tee([&side_error2](bool val) { EXPECT_FALSE((side_error2 = val)); })
        .Tee(&side_error3);
    passed = true;
  };
  F(true);
  EXPECT_TRUE(passed);
  EXPECT_TRUE(side_error1);
  EXPECT_TRUE(side_error2);
  EXPECT_TRUE(side_error3);
  F(false);
  EXPECT_FALSE(passed);
  EXPECT_FALSE(side_error1);
  EXPECT_FALSE(side_error2);
  EXPECT_FALSE(side_error3);
}

TEST(Tee, Policy) {
  bool passed;
  bool side_error;
  auto F = [&](bool val) {
    passed = false;
    side_error = true;
    const auto MErrorDomain = MyErrorDomain.Tee([&](bool val) {
      EXPECT_FALSE(val);
      side_error = val;
    });
    MVERIFY(val);
    passed = true;
  };
  F(true);
  EXPECT_TRUE(passed);
  EXPECT_TRUE(side_error);
  F(false);
  EXPECT_FALSE(passed);
  EXPECT_FALSE(side_error);
}

TEST(Tee, NonVoidReturnType) {
  bool passed;
  bool side_error;
  auto F = [&](bool val) {
    passed = false;
    side_error = true;
    MVERIFY(val).Tee(&side_error).Return<bool>();
    passed = true;
    return true;
  };
  EXPECT_TRUE(F(true));
  EXPECT_TRUE(passed);
  EXPECT_TRUE(side_error);
  EXPECT_FALSE(F(false));
  EXPECT_FALSE(passed);
  EXPECT_FALSE(side_error);
}

TEST(Tee, PolicyNoTee) {
  bool passed;
  bool side_error;
  auto F = [&](bool val) {
    passed = false;
    side_error = true;
    const auto MErrorDomain = MyErrorDomain.Tee([&](bool val) {
      EXPECT_FALSE(val);
      side_error = val;
    }).NoTee();
    MVERIFY(val);
    passed = true;
  };
  F(true);
  EXPECT_TRUE(passed);
  EXPECT_TRUE(side_error);
  F(false);
  EXPECT_FALSE(passed);
  EXPECT_TRUE(side_error);
}


TEST(Tee, NonVoidReturnTypeNoTee) {
  bool passed;
  bool side_error;
  auto F = [&](bool val) {
    passed = false;
    side_error = true;
    MVERIFY(val).Tee(&side_error).NoTee().Return<bool>();
    passed = true;
    return true;
  };
  EXPECT_TRUE(F(true));
  EXPECT_TRUE(passed);
  EXPECT_TRUE(side_error);
  EXPECT_FALSE(F(false));
  EXPECT_FALSE(passed);
  EXPECT_TRUE(side_error);
}

}  // namespace
}  // namespace merror
