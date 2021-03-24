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

#include "merror/domain/adl_hooks.h"

#include <utility>

#include "merror/domain/base.h"
#include "merror/domain/defer.h"
#include "merror/domain/return.h"
#include "merror/macros.h"
#include "gtest/gtest.h"

namespace merror {
namespace {

namespace hidden {

enum class ErrorCode {
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

struct VerifyAcceptor {
  bool IsError() { return e != ErrorCode::kOk; }
  const ErrorCode& GetCulprit() && { return e; }
  const ErrorCode& e;
};

template <class Policy, class R>
VerifyAcceptor MVerify(const Policy&, Ref<R, ErrorCode> e) {
  return {e.Get()};
}

template <class E>
struct TryAcceptor {
  E&& e;
  bool IsError() { return e.error != ErrorCode::kOk; }
  auto GetValue() && -> decltype((std::forward<E>(e).value)) {
    return std::forward<E>(e).value;
  }
  const ErrorCode& GetCulprit() && { return e.error; }
};

template <class Policy, class R, class T>
TryAcceptor<R> MTry(const Policy&, Ref<R, ErrorOr<T>> e) {
  return {e.Forward()};
}

template <class Builder>
ErrorCode MakeMError(const Builder&, ResultType<ErrorCode>, Void) {
  return ErrorCode::kUnknown;
}

template <class Builder>
ErrorCode MakeMError(const Builder&, ResultType<ErrorCode>, ErrorCode e) {
  return e;
}

template <class Builder, class T>
ErrorCode MakeMError(const Builder&, ResultType<ErrorCode>,
                     const ErrorOr<T>& e) {
  return e.error;
}

template <class Builder>
void FillMError(const Builder&, Void, ErrorCode* error) {
  *error = ErrorCode::kUnknown;
}

template <class Builder>
void FillMError(const Builder&, ErrorCode culprit, ErrorCode* error) {
  *error = culprit;
}

template <class Builder, class T>
void FillMError(const Builder&, const ErrorOr<T>& culprit, ErrorCode* error) {
  *error = culprit.error;
}

}  // namespace hidden

using hidden::ErrorCode;
using hidden::ErrorOr;

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
      return ErrorCode::kFallback;
    }

    void FillError(int culprit, ErrorCode* error) const {
      EXPECT_EQ(666, culprit);
      *error = ErrorCode::kFallback;
    }

    int MakeError(ResultType<int>, Void) const { return -1; }

    void FillError(Void, int* error) const { *error = -1; }
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
              AdlHooks(), Builder<SideError>())
        .Return<ErrorCode>();

TEST(AdlHooks, Throw) {
  ErrorCode error;
  auto F = [&]() {
    error = ErrorCode::kOk;
    return MERROR().Fill(&error);
  };

  EXPECT_EQ(ErrorCode::kUnknown, F());
  EXPECT_EQ(ErrorCode::kUnknown, error);
}

TEST(AdlHooks, ThrowFallback) {
  int error;
  auto F = [&]() {
    error = 0;
    return MERROR().Return<int>().Fill(&error);
  };

  EXPECT_EQ(-1, F());
  EXPECT_EQ(-1, error);
}

TEST(AdlHooks, Verify) {
  bool passed;
  ErrorCode error;
  auto F = [&](ErrorCode in) {
    passed = false;
    error = ErrorCode::kOk;
    MVERIFY(in).Fill(&error);
    EXPECT_EQ(ErrorCode::kOk, in);
    passed = true;
    return ErrorCode::kOk;
  };

  EXPECT_EQ(ErrorCode::kOk, F(ErrorCode::kOk));
  EXPECT_TRUE(passed);
  EXPECT_EQ(ErrorCode::kOk, error);

  EXPECT_EQ(ErrorCode::kFail, F(ErrorCode::kFail));
  EXPECT_TRUE(!passed);
  EXPECT_EQ(ErrorCode::kFail, error);
}

TEST(AdlHooks, VerifyFallback) {
  bool passed;
  ErrorCode error;
  auto F = [&](const char* s) {
    passed = false;
    error = ErrorCode::kOk;
    MVERIFY(s).Fill(&error);
    EXPECT_TRUE(s);
    passed = true;
    return ErrorCode::kOk;
  };

  EXPECT_EQ(ErrorCode::kOk, F("hello"));
  EXPECT_TRUE(passed);
  EXPECT_EQ(ErrorCode::kOk, error);

  EXPECT_EQ(ErrorCode::kFallback, F(nullptr));
  EXPECT_TRUE(!passed);
  EXPECT_EQ(ErrorCode::kFallback, error);
}

TEST(AdlHooks, Try) {
  bool passed;
  ErrorCode error;
  auto F = [&](ErrorOr<int> e) {
    passed = false;
    error = ErrorCode::kOk;
    int n = MTRY(e, Fill(&error));
    EXPECT_EQ(ErrorCode::kOk, e.error);
    EXPECT_EQ(42, n);
    passed = true;
    return ErrorCode::kOk;
  };

  EXPECT_EQ(ErrorCode::kOk, F({ErrorCode::kOk, 42}));
  EXPECT_TRUE(passed);
  EXPECT_EQ(ErrorCode::kOk, error);

  EXPECT_EQ(ErrorCode::kFail, F({ErrorCode::kFail}));
  EXPECT_TRUE(!passed);
  EXPECT_EQ(ErrorCode::kFail, error);
}

TEST(AdlHooks, TryFallback) {
  bool passed;
  ErrorCode error;
  auto F = [&](const char* s) {
    passed = false;
    error = ErrorCode::kOk;
    char c = MTRY(s, Fill(&error));
    EXPECT_TRUE(s);
    EXPECT_EQ('X', c);
    passed = true;
    return ErrorCode::kOk;
  };

  EXPECT_EQ(ErrorCode::kOk, F("X"));
  EXPECT_TRUE(passed);
  EXPECT_EQ(ErrorCode::kOk, error);

  EXPECT_EQ(ErrorCode::kFallback, F(nullptr));
  EXPECT_TRUE(!passed);
  EXPECT_EQ(ErrorCode::kFallback, error);
}

TEST(AdlHooks, TryPerfectForwarding) {
  auto F = [] {
    ErrorOr<int> e = {ErrorCode::kOk, 42};
    const ErrorOr<int>& c = e;
    ExpectSame(MTRY(e), e.value);
    ExpectSame(MTRY(c), c.value);
    ExpectSame(MTRY(std::move(e)), std::move(e.value));
    ExpectSame(MTRY(std::move(c)), std::move(c.value));
    return ErrorCode::kOk;
  };
  EXPECT_EQ(ErrorCode::kOk, F());
}

}  // namespace
}  // namespace merror
