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

#include "merror/domain/return.h"

#include <type_traits>
#include <utility>

#include "merror/domain/base.h"
#include "merror/macros.h"
#include "gtest/gtest.h"

namespace merror {
namespace {

template <class Base>
struct MakeInt : Base {
  int MakeError(ResultType<int>, const Void&) const { return 42; }
};

struct ExpectedRetVal {};

template <class Expected>
class ExpectedValue {
 public:
  explicit ExpectedValue(Expected expected) : expected_(std::move(expected)) {}
  ~ExpectedValue() { EXPECT_TRUE(done_); }

  template <class Actual>
  void Verify(const Actual& actual) {
    EXPECT_EQ(expected_, actual);
    done_ = true;
  }

 private:
  Expected expected_;
  bool done_ = false;
};

struct AcceptAll {
  template <class Actual>
  void Verify(const Actual& actual) {}
};

template <class Base>
struct Sniffer : Observer<Base> {
  template <class R>
  auto ExpectRetVal(R r) && -> decltype(AddAnnotation<ExpectedRetVal>(
      std::move(*this), std::make_shared<ExpectedValue<R>>(std::move(r)))) {
    return AddAnnotation<ExpectedRetVal>(
        std::move(*this), std::make_shared<ExpectedValue<R>>(std::move(r)));
  }

  template <class R>
  void ObserveRetVal(const R& ret_val) {
    AcceptAll accept_all;
    GetAnnotationOr<ExpectedRetVal>(*this, &accept_all)->Verify(ret_val);
  }
};

TEST(Return, Auto) {
  static constexpr auto MErrorDomain =
      Return().With(Builder<MakeInt>(), Builder<Sniffer>());
  auto F = []() -> int { return MERROR().ExpectRetVal(42); };
  EXPECT_EQ(42, F());
}

TEST(Return, Value) {
  static constexpr auto MErrorDomain =
      Return().With(Builder<Sniffer>()).Return(666);
  {
    auto F = [] { return MERROR().ExpectRetVal(666); };
    EXPECT_EQ(666, F());
  }
  {
    auto F = [] { return MERROR().Return(1337).ExpectRetVal(1337); };
    EXPECT_EQ(1337, F());
  }
}

struct RetVal {
  RetVal(int a) : a(a) {}
  RetVal(const RetVal&) = default;
  RetVal(RetVal&& other) : a(other.a) { other.a = 0; }
  operator int() const { return a; }
  int a;
};
RetVal ret_val = 69;

TEST(Return, RetValue1) {
  static auto MErrorDomain = Return().With(Builder<Sniffer>());
  {
    auto F = []() -> int { return MERROR().Return(ret_val).ExpectRetVal(69); };
    EXPECT_EQ(69, F());
  }
}

TEST(Return, RetValue2) {
  static auto MErrorDomain = Return().With(Builder<Sniffer>());
  {
    auto F = []() -> int { return MERROR().Return(ret_val).ExpectRetVal(69); };
    EXPECT_EQ(69, F());
  }
}

TEST(Return, DeferReturn) {
  {
    auto MErrorDomain =
        Return().With(Builder<Sniffer>()).DeferReturn([] { return 1999; });
    auto F = [&] { return MERROR().ExpectRetVal(1999); };
    EXPECT_EQ(1999, F());
  }
  {
    auto MErrorDomain = Return()
                            .With(Builder<MakeInt>(), Builder<Sniffer>())
                            .DeferReturn([](int error) {
                              EXPECT_EQ(error, 42);
                              return 1999;
                            });
    auto F = [&] { return MERROR().ExpectRetVal(1999); };
    EXPECT_EQ(1999, F());
  }
}

TEST(Return, Void) {
  {
    static constexpr auto MErrorDomain =
        Return().With(Builder<Sniffer>()).Return();
    auto F = [] { return MERROR().ExpectRetVal(Void()); };
    F();
    static_assert(std::is_void<decltype(F())>(), "");
  }
  {
    static constexpr auto MErrorDomain =
        Return().With(Builder<Sniffer>()).Return<void>();
    auto F = [] { return MERROR().ExpectRetVal(Void()); };
    F();
    static_assert(std::is_void<decltype(F())>(), "");
  }
}

TEST(Return, Type) {
  static constexpr auto MErrorDomain =
      Return().With(Builder<MakeInt>(), Builder<Sniffer>()).Return<int>();
  auto F = [] { return MERROR().ExpectRetVal(42); };
  EXPECT_EQ(42, F());
}

}  // namespace
}  // namespace merror
