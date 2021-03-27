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

#include "merror/domain/fill_error.h"

#include <string>

#include "gtest/gtest.h"
#include "merror/domain/base.h"
#include "merror/domain/bool.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/return.h"
#include "merror/macros.h"

namespace merror {
namespace {

template <class Base>
struct SideError : Base {
  template <class Error>
  typename Base::BuilderType&& Fill(Error&& error) && {
    this->derived().FillError(this->context().culprit,
                              std::forward<Error>(error));
    return std::move(this->derived());
  }
};

template <class Base>
struct GenerateError : Base {
  template <class T>
  int MakeError(ResultType<int>, T&&) const {
    return 42;
  }

  template <class T>
  bool MakeError(ResultType<bool>, T&&) const {
    return false;
  }
};

constexpr auto MErrorDomain =
    EmptyDomain().With(Return(), FillError(), MethodHooks(), AcceptBool(),
                       Builder<SideError>(), Builder<GenerateError>());

TEST(FillError, Works) {
  int error = 0;
  auto F = [&]() { return MERROR().Fill(&error).Return(); };
  F();
  EXPECT_EQ(42, error);
}

class Functor {
 public:
  explicit Functor(bool* out) : out_(out) {}
  void operator()() && { *out_ = false; }

 private:
  bool* out_;
};

class Functor1 {
 public:
  explicit Functor1(bool* out) : out_(out) {}
  void operator()(const bool& val) && {
    EXPECT_FALSE(val);
    *out_ = val;
  }

 private:
  bool* out_;
};

class Functor01 : public Functor, public Functor1 {
 public:
  Functor01(bool* out0, bool* out1) : Functor(out0), Functor1(out1) {}
  using Functor::operator();
  using Functor1::operator();
};

TEST(FillError, Function) {
  bool passed;
  static bool side_error;
  auto F = [&](bool arg) {
    static bool val;
    val = arg;
    passed = false;
    side_error = true;
    MVERIFY(val)
        .Fill(*[] {
          EXPECT_FALSE(val);
          side_error = val;
        })
        .Return();
    passed = true;
  };
  F(true);
  EXPECT_TRUE(passed);
  EXPECT_TRUE(side_error);
  F(false);
  EXPECT_FALSE(passed);
  EXPECT_FALSE(side_error);
}

TEST(FillError, Function1) {
  bool passed;
  static bool side_error;
  auto F = [&](bool val) {
    passed = false;
    side_error = true;
    MVERIFY(val)
        .Fill(*+[](const bool& val) {
          EXPECT_FALSE(val);
          side_error = val;
        })
        .Return();
    passed = true;
  };
  F(true);
  EXPECT_TRUE(passed);
  EXPECT_TRUE(side_error);
  F(false);
  EXPECT_FALSE(passed);
  EXPECT_FALSE(side_error);
}

TEST(FillError, FunctionPointer) {
  bool passed;
  static bool side_error;
  auto F = [&](bool arg) {
    static bool val;
    val = arg;
    passed = false;
    side_error = true;
    MVERIFY(val)
        .Fill(+[]() {
          EXPECT_FALSE(val);
          side_error = val;
        })
        .Return();
    passed = true;
  };
  F(true);
  EXPECT_TRUE(passed);
  EXPECT_TRUE(side_error);
  F(false);
  EXPECT_FALSE(passed);
  EXPECT_FALSE(side_error);
}

TEST(FillError, FunctionPointer1) {
  bool passed;
  static bool side_error;
  auto F = [&](bool val) {
    passed = false;
    side_error = true;
    MVERIFY(val)
        .Fill(+[](const bool& val) {
          EXPECT_FALSE(val);
          side_error = val;
        })
        .Return();
    passed = true;
  };
  F(true);
  EXPECT_TRUE(passed);
  EXPECT_TRUE(side_error);
  F(false);
  EXPECT_FALSE(passed);
  EXPECT_FALSE(side_error);
}

TEST(FillError, Functor) {
  bool passed;
  bool side_error;
  auto F = [&](bool val) {
    passed = false;
    side_error = true;
    MVERIFY(val).Fill(Functor(&side_error)).Return();
    passed = true;
  };
  F(true);
  EXPECT_TRUE(passed);
  EXPECT_TRUE(side_error);
  F(false);
  EXPECT_FALSE(passed);
  EXPECT_FALSE(side_error);
}

TEST(FillError, Functor1) {
  bool passed;
  bool side_error;
  auto F = [&](bool val) {
    passed = false;
    side_error = true;
    MVERIFY(val).Fill(Functor1(&side_error)).Return();
    passed = true;
  };
  F(true);
  EXPECT_TRUE(passed);
  EXPECT_TRUE(side_error);
  F(false);
  EXPECT_FALSE(passed);
  EXPECT_FALSE(side_error);
}

TEST(FillError, Functor01) {
  bool passed;
  bool side_error0;
  bool side_error1;
  auto F = [&](bool val) {
    passed = false;
    side_error0 = true;
    side_error1 = true;
    MVERIFY(val).Fill(Functor01(&side_error0, &side_error1)).Return();
    passed = true;
  };
  F(true);
  EXPECT_TRUE(passed);
  EXPECT_TRUE(side_error0);
  EXPECT_TRUE(side_error1);
  F(false);
  EXPECT_FALSE(passed);
  EXPECT_TRUE(side_error0);
  EXPECT_FALSE(side_error1);
}

TEST(FillError, Pointer) {
  bool passed;
  int side_error;
  auto F = [&](bool val) {
    passed = false;
    side_error = 0;
    MVERIFY(val).Fill(&side_error).Return();
    passed = true;
  };
  F(true);
  EXPECT_TRUE(passed);
  EXPECT_EQ(side_error, 0);
  F(false);
  EXPECT_FALSE(passed);
  EXPECT_EQ(side_error, 42);
}

}  // namespace
}  // namespace merror
