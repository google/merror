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

#include "merror/domain/method_hooks.h"

#include <utility>

#include "gtest/gtest.h"
#include "merror/domain/base.h"
#include "merror/domain/defer.h"
#include "merror/domain/return.h"
#include "merror/macros.h"

namespace merror {
namespace {

enum ErrorCode {
  kOk,
  kFail,
  kUnknown,
  kFallback,
};

template <class T>
struct ErrorOr {
  ErrorCode error;
  T value;
};

template <class Base>
struct AcceptError : Hook<Base> {
  using Hook<Base>::MVerify;
  using Hook<Base>::MTry;

  struct VerifyAcceptor {
    bool IsError() { return e != kOk; }
    const ErrorCode& GetCulprit() && { return e; }
    const ErrorCode& e;
  };

  template <class R>
  VerifyAcceptor MVerify(Ref<R, ErrorCode> e) const {
    return {e.Get()};
  }

  template <class E>
  struct TryAcceptor {
    E&& e;
    bool IsError() { return e.error != kOk; }
    auto GetValue() && -> decltype((std::forward<E>(e).value)) {
      return std::forward<E>(e).value;
    }
    const ErrorCode& GetCulprit() && { return e.error; }
  };

  template <class R, class T>
  TryAcceptor<R> MTry(Ref<R, ErrorOr<T>> e) const {
    return {e.Forward()};
  }
};

template <class Base>
struct ProduceError : Hook<Base> {
  using Hook<Base>::MakeMError;
  using Hook<Base>::FillMError;

  ErrorCode MakeMError(ResultType<ErrorCode>, Void) const { return kUnknown; }

  ErrorCode MakeMError(ResultType<ErrorCode>, ErrorCode e) const { return e; }

  template <class T>
  ErrorCode MakeMError(ResultType<ErrorCode>, const ErrorOr<T>& e) const {
    return e.error;
  }

  void FillMError(Void culprit, ErrorCode* error) const { *error = kUnknown; }

  void FillMError(ErrorCode culprit, ErrorCode* error) const {
    *error = culprit;
  }

  template <class T>
  void FillMError(const ErrorOr<T>& culprit, ErrorCode* error) const {
    *error = culprit.error;
  }
};

template <class Base>
struct SideError : Base {
  template <class Error, class X = void>
  auto Fill(Error* error) && -> decltype(std::move(Defer<X>(this)->derived())) {
    this->derived().FillError(this->context().culprit, error);
    return std::move(this->derived());
  }
};

struct Fallback {
  template <class Base>
  struct Policy : Base {
    struct Acceptor {
      bool IsError() { return p == nullptr; }
      const char& GetValue() && { return *p; }
      int GetCulprit() && { return 666; }
      const char* p;
    };
    template <class R>
    Acceptor Verify(Ref<R, const char*> p) const {
      return {p.Get()};
    }
    template <class R>
    Acceptor Try(Ref<R, const char*> p) const {
      return {p.Get()};
    }
  };

  template <class Base>
  struct Builder : Base {
    ErrorCode MakeError(ResultType<ErrorCode>, int culprit) const {
      EXPECT_EQ(666, culprit);
      return kFallback;
    }

    void FillError(int culprit, ErrorCode* error) const {
      EXPECT_EQ(666, culprit);
      *error = kFallback;
    }

    int MakeError(ResultType<int>, Void culprit) const { return -1; }

    void FillError(Void culprit, int* error) const { *error = -1; }
  };
};

template <class Expected, class Actual>
void ExpectSame(Expected&& expected, Actual&& actual) {
  static_assert(std::is_same<Expected, Actual>(), "");
  EXPECT_EQ(&expected, &actual);
}

constexpr auto MErrorDomain =
    EmptyDomain()
        .With(Return(), Domain<Fallback::Policy, Fallback::Builder>(),
              MethodHooks(), Policy<AcceptError>(), Builder<ProduceError>(),
              Builder<SideError>())
        .Return<ErrorCode>();

TEST(MethodHooks, Throw) {
  ErrorCode error;
  auto F = [&]() {
    error = kOk;
    return MERROR().Fill(&error);
  };

  EXPECT_EQ(kUnknown, F());
  EXPECT_EQ(kUnknown, error);
}

TEST(MethodHooks, ThrowFallback) {
  int error;
  auto F = [&]() {
    error = 0;
    return MERROR().Return<int>().Fill(&error);
  };

  EXPECT_EQ(-1, F());
  EXPECT_EQ(-1, error);
}

TEST(MethodHooks, Verify) {
  bool passed;
  ErrorCode error;
  auto F = [&](ErrorCode in) {
    passed = false;
    error = kOk;
    MVERIFY(in).Fill(&error);
    EXPECT_EQ(kOk, in);
    passed = true;
    return kOk;
  };

  EXPECT_EQ(kOk, F(kOk));
  EXPECT_TRUE(passed);
  EXPECT_EQ(kOk, error);

  EXPECT_EQ(kFail, F(kFail));
  EXPECT_TRUE(!passed);
  EXPECT_EQ(kFail, error);
}

TEST(MethodHooks, VerifyFallback) {
  bool passed;
  ErrorCode error;
  auto F = [&](const char* s) {
    passed = false;
    error = kOk;
    MVERIFY(s).Fill(&error);
    EXPECT_TRUE(s);
    passed = true;
    return kOk;
  };

  EXPECT_EQ(kOk, F("hello"));
  EXPECT_TRUE(passed);
  EXPECT_EQ(kOk, error);

  EXPECT_EQ(kFallback, F(nullptr));
  EXPECT_TRUE(!passed);
  EXPECT_EQ(kFallback, error);
}

TEST(MethodHooks, Try) {
  bool passed;
  ErrorCode error;
  auto F = [&](ErrorOr<int> e) {
    passed = false;
    error = kOk;
    int n = MTRY(e, Fill(&error));
    EXPECT_EQ(kOk, e.error);
    EXPECT_EQ(42, n);
    passed = true;
    return kOk;
  };

  EXPECT_EQ(kOk, F({kOk, 42}));
  EXPECT_TRUE(passed);
  EXPECT_EQ(kOk, error);

  EXPECT_EQ(kFail, F({kFail}));
  EXPECT_TRUE(!passed);
  EXPECT_EQ(kFail, error);
}

TEST(MethodHooks, TryFallback) {
  bool passed;
  ErrorCode error;
  auto F = [&](const char* s) {
    passed = false;
    error = kOk;
    char c = MTRY(s, Fill(&error));
    EXPECT_TRUE(s);
    EXPECT_EQ('X', c);
    passed = true;
    return kOk;
  };

  EXPECT_EQ(kOk, F("X"));
  EXPECT_TRUE(passed);
  EXPECT_EQ(kOk, error);

  EXPECT_EQ(kFallback, F(nullptr));
  EXPECT_TRUE(!passed);
  EXPECT_EQ(kFallback, error);
}

TEST(MethodHooks, TryPerfectForwarding) {
  auto F = [] {
    ErrorOr<int> e = {kOk, 42};
    const ErrorOr<int>& c = e;
    ExpectSame(MTRY(e), e.value);
    ExpectSame(MTRY(c), c.value);
    ExpectSame(MTRY(std::move(e)), std::move(e.value));
    ExpectSame(MTRY(std::move(c)), std::move(c.value));
    return kOk;
  };
  EXPECT_EQ(kOk, F());
}

TEST(MethodHooks, Hook) {
  struct A {};
  struct B : Hook<A> {
    using Hook<A>::MVerify;
    using Hook<A>::MTry;
    using Hook<A>::MakeMError;
    using Hook<A>::FillMError;
  };
  static_assert(std::is_same<Hook<B>, B>(), "");
}

}  // namespace
}  // namespace merror
