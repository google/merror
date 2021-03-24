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
#include "merror/macros.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace merror {
namespace {

using std::nullopt;
using std::optional;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::StrEq;

#define MERROR_TEST_ID(...) __VA_ARGS__

template <class T>
std::unique_ptr<T> WrapUnique(T* t) {
  return std::unique_ptr<T>(t);
}

struct Any {
  Any() {}
  template <class T>
  Any(T&&) {}
};

template <class Culprit>
struct ErrorContext {
  Macro macro;
  uintptr_t location_id;
  const char* function;
  const char* file;
  int line;
  const char* macro_str;
  const char* args_str;
  Culprit culprit;
  optional<RelationalExpression> rel_expr;
};

template <class Context>
struct ReflectingBuilder {
  Context&& ctx;

 private:
  ErrorContext<typename std::decay<typename Context::Culprit>::type>
  BuildError() && {
    return {ctx.macro,
            ctx.location_id,
            ctx.function,
            ctx.file,
            ctx.line,
            ctx.macro_str,
            ctx.args_str,
            std::move(ctx.culprit),
            ctx.rel_expr ? *ctx.rel_expr : optional<RelationalExpression>()};
  }

  template <class T>
  friend struct ::merror::MErrorAccess;
};

struct ReflectingDomain {
  template <class Context>
  ReflectingBuilder<Context> GetErrorBuilder(Context&& ctx) const {
    static_assert(!std::is_reference<Context>(), "");
    return {std::move(ctx)};
  }
};

TEST(MErrorDomain, Works) {
  struct Domain {
    const Domain& operator()() const { return *this; }
    const Domain& Expect(int expected) const {
      EXPECT_EQ(expected, value);
      return *this;
    }
    Domain Value(int override) const { return {override}; }
    int value;
  };
  {
    MERROR_DOMAIN(Domain);
    static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
    EXPECT_EQ(0, MErrorDomain.value);
  }
  {
    constexpr MERROR_DOMAIN(Domain);
    constexpr auto x = MErrorDomain;
    static_cast<void>(x);
    static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
    EXPECT_EQ(0, MErrorDomain.value);
  }
  {
    static MERROR_DOMAIN(Domain);
    [] {
      static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
      EXPECT_EQ(0, MErrorDomain.value);
    }();
  }
  {
    static constexpr MERROR_DOMAIN(Domain);
    [] {
      constexpr auto x = MErrorDomain;
      static_cast<void>(x);
      static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
      EXPECT_EQ(0, MErrorDomain.value);
    }();
  }
  {
    MERROR_DOMAIN(Domain).Expect(0);
    static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
    EXPECT_EQ(0, MErrorDomain.value);
  }
  {
    MERROR_DOMAIN(Domain{42});
    static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
    EXPECT_EQ(42, MErrorDomain.value);
  }
  {
    MERROR_DOMAIN(Domain{42}).Expect(42);
    static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
    EXPECT_EQ(42, MErrorDomain.value);
  }
  {
    MERROR_DOMAIN(Domain{42});
    {
      MERROR_DOMAIN(Domain{1337});
      static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
      EXPECT_EQ(1337, MErrorDomain.value);
    }
    EXPECT_EQ(42, MErrorDomain.value);
  }
  {
    MERROR_DOMAIN(Domain{42});
    {
      MERROR_DOMAIN(Domain{1337}).Expect(1337);
      static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
      EXPECT_EQ(1337, MErrorDomain.value);
    }
    EXPECT_EQ(42, MErrorDomain.value);
  }
  {
    MERROR_DOMAIN(Domain{42});
    {
      MERROR_DOMAIN(Domain).Value(1337);
      static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
      EXPECT_EQ(1337, MErrorDomain.value);
    }
    EXPECT_EQ(42, MErrorDomain.value);
  }
  {
    MERROR_DOMAIN(Domain{42});
    {
      MERROR_DOMAIN(Domain).Expect(0).Value(1337);
      static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
      EXPECT_EQ(1337, MErrorDomain.value);
    }
    EXPECT_EQ(42, MErrorDomain.value);
  }
  {
    MERROR_DOMAIN(Domain{42});
    {
      MERROR_DOMAIN();
      static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
      EXPECT_EQ(42, MErrorDomain.value);
    }
    EXPECT_EQ(42, MErrorDomain.value);
  }
  {
    MERROR_DOMAIN(Domain{42});
    {
      MERROR_DOMAIN().Expect(42);
      static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
      EXPECT_EQ(42, MErrorDomain.value);
    }
    EXPECT_EQ(42, MErrorDomain.value);
  }
  {
    MERROR_DOMAIN(Domain{42});
    {
      MERROR_DOMAIN().Value(1337);
      static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
      EXPECT_EQ(1337, MErrorDomain.value);
    }
    EXPECT_EQ(42, MErrorDomain.value);
  }
  {
    MERROR_DOMAIN(Domain{42});
    {
      MERROR_DOMAIN().Expect(42).Value(1337);
      static_assert(std::is_same<decltype(MErrorDomain), const Domain>(), "");
      EXPECT_EQ(1337, MErrorDomain.value);
    }
    EXPECT_EQ(42, MErrorDomain.value);
  }
}

TEST(MError, ControlFlow) {
  using MErrorDomain = ReflectingDomain;
  auto F = [] { return MERROR(); };
  F();
}

TEST(MError, Context) {
  using MErrorDomain = ReflectingDomain;
  {
    const int line = __LINE__;
    ErrorContext<Void> ctx = [] { return MERROR(); }();
    EXPECT_THAT(ctx.macro, Macro::kError);
    EXPECT_THAT(ctx.location_id, Not(0));
    EXPECT_THAT(ctx.function, HasSubstr("MError_Context_Test::TestBody()"));
    EXPECT_THAT(ctx.file, EndsWith("macros_test.cc"));
    EXPECT_THAT(ctx.line, line + 1);
    EXPECT_THAT(ctx.macro_str, StrEq("MERROR"));
    EXPECT_THAT(ctx.args_str, StrEq(""));
    EXPECT_TRUE(ctx.rel_expr == nullopt);
  }
  {
    const int line = __LINE__;
    ErrorContext<int> ctx = [] { return MERROR(42); }();
    EXPECT_THAT(ctx.macro, Macro::kError);
    EXPECT_THAT(ctx.culprit, 42);
    EXPECT_THAT(ctx.location_id, Not(0));
    EXPECT_THAT(ctx.function, HasSubstr("MError_Context_Test::TestBody()"));
    EXPECT_THAT(ctx.file, EndsWith("macros_test.cc"));
    EXPECT_THAT(ctx.line, line + 1);
    EXPECT_THAT(ctx.macro_str, StrEq("MERROR"));
    EXPECT_THAT(ctx.args_str, StrEq("42"));
    EXPECT_TRUE(ctx.rel_expr == nullopt);
  }
}

TEST(MError, LocationId) {
  using MErrorDomain = ReflectingDomain;
  auto F = [] { return MERROR(); };
  auto G = [](bool x) { return x ? MERROR() : MERROR(); };

  uintptr_t f = F().location_id;
  EXPECT_NE(f, 0);

  uintptr_t g0 = G(false).location_id;
  EXPECT_NE(g0, 0);
  EXPECT_NE(g0, f);

  uintptr_t g1 = G(true).location_id;
  EXPECT_NE(g1, 0);
  EXPECT_NE(g1, f);
  EXPECT_NE(g1, g0);

  EXPECT_EQ(f, F().location_id);
  EXPECT_EQ(g0, G(false).location_id);
  EXPECT_EQ(g1, G(true).location_id);
}

TEST(MError, DomainFunction) {
  auto MErrorDomain = [] { return ReflectingDomain(); };
  auto F = [&] { return MERROR(); };
  ErrorContext<Void> ctx = F();
}

TEST(MError, LvalueDomain) {
  struct Domain : ReflectingDomain {
    Domain() {}
    Domain(const Domain&) = delete;
    Domain& operator=(const Domain&) = delete;
  } domain;
  auto MErrorDomain = [&]() -> ReflectingDomain& { return domain; };
  auto F = [&] { return MERROR(); };
  ErrorContext<Void> ctx = F();
}

TEST(MError, BuilderOperators) {
  struct Builder {
    int BuildError() { return error; }
    int error;
  };
  struct Proxy {
    Builder B() { return {42}; }
  };
  struct MErrorDomain {
    Proxy GetErrorBuilder(Any) const { return {}; }
  };
  {
    int error = [] { return MERROR().B(); }();
    EXPECT_EQ(42, error);
  }
  {
    int error = [] { return MERROR().B() = {666}; }();
    EXPECT_EQ(666, error);
  }
}

TEST(MVerify, ControlFlow) {
  struct MErrorDomain : ReflectingDomain {
    struct Acceptor {
      bool IsError() { return !value; }
      const bool& GetCulprit() && { return value; }
      const bool& value;
    };
    Acceptor Verify(Ref<bool&> val) const { return {val.Get()}; }
  };
  bool passed = false;
  auto F = [&](bool val) -> Any {
    passed = false;
    MVERIFY(val);
    passed = true;
    return {};
  };
  F(false);
  EXPECT_FALSE(passed);
  F(true);
  EXPECT_TRUE(passed);
}

TEST(MVerify, Context) {
  struct MErrorDomain : ReflectingDomain {
    struct Acceptor {
      bool IsError() { return true; }
      const int& GetCulprit() && { return value; }
      const int& value;
    };
    Acceptor Verify(Ref<int&&> val) const { return {val.Get()}; }
  };
  const int line = __LINE__;
  auto F = [] {
    MVERIFY(MERROR_TEST_ID(2 + 3));
    static_cast<void>(1);
  };
  ErrorContext<int> ctx = F();
  EXPECT_THAT(ctx.macro, Macro::kVerify);
  EXPECT_THAT(ctx.location_id, Not(0));
  EXPECT_THAT(ctx.function, HasSubstr("MVerify_Context_Test::TestBody()"));
  EXPECT_THAT(ctx.file, EndsWith("macros_test.cc"));
  EXPECT_THAT(ctx.line, line + 2);
  EXPECT_THAT(ctx.macro_str, StrEq("MVERIFY"));
  EXPECT_THAT(ctx.args_str, StrEq("MERROR_TEST_ID(2 + 3)"));
  EXPECT_THAT(ctx.culprit, 5);
  EXPECT_FALSE(ctx.rel_expr);
}

TEST(MVerify, CustomCulprit) {
  struct MErrorDomain : ReflectingDomain {
    struct Acceptor {
      bool IsError() { return true; }
      std::string GetCulprit() && { return std::to_string(value); }
      const int& value;
    };
    Acceptor Verify(Ref<int&&> val) const { return {val.Get()}; }
  };
  auto F = [] {
    MVERIFY(42);
    static_cast<void>(1);
  };
  ErrorContext<std::string> ctx = F();
  EXPECT_EQ("42", ctx.culprit);
}

TEST(MVerify, DomainFunction) {
  struct Domain : ReflectingDomain {
    struct Acceptor {
      bool IsError() { return true; }
      const int& GetCulprit() && { return value; }
      const int& value;
    };
    Acceptor Verify(Ref<int&&> val) const { return {val.Get()}; }
  };
  auto MErrorDomain = [] { return Domain(); };
  auto F = [&] {
    MVERIFY(42);
    static_cast<void>(1);
  };
  ErrorContext<int> ctx = F();
}

TEST(MVerify, LvalueDomain) {
  struct Domain : ReflectingDomain {
    Domain() {}
    Domain(const Domain&) = delete;
    Domain& operator=(const Domain&) = delete;
    struct Acceptor {
      bool IsError() { return true; }
      const int& GetCulprit() && { return value; }
      const int& value;
    };
    Acceptor Verify(Ref<int&&> val) const { return {val.Get()}; }
  } domain;
  {
    auto MErrorDomain = [&]() -> Domain& { return domain; };
    auto F = [&] {
      MVERIFY(42);
      static_cast<void>(1);
    };
    ErrorContext<int> ctx = F();
  }
  {
    auto MErrorDomain = [&]() -> const Domain& { return domain; };
    auto F = [&] {
      MVERIFY(42);
      static_cast<void>(1);
    };
    ErrorContext<int> ctx = F();
  }
}

/*
TEST(MVerify, RvalueDomain) {
  struct Domain : ReflectingDomain {
    struct Acceptor {
      bool IsError() { return true; }
      const int& GetCulprit() && { return value; }
      const int& value;
    };
    Acceptor Verify(Ref<int&&> val) const {
      EXPECT_EQ(std::string(100, 'x'), lifeline);
      return {val.Get()};
    }
    std::string lifeline = std::string(100, 'x');
  };
  {
    auto MErrorDomain = [](Domain d = {}) -> Domain&& { return std::move(d); };
    auto F = [&] {
      MVERIFY(42);
      static_cast<void>(1);
    };
    ErrorContext<int> ctx = F();
  }
  {
    auto MErrorDomain = [](Domain d = {}) -> const Domain&& {
      return std::move(d);
    };
    auto F = [&] {
      MVERIFY(42);
      static_cast<void>(1);
    };
    ErrorContext<int> ctx = F();
  }
}
*/


TEST(MVerify, BuilderOperators) {
  struct Builder {
    int BuildError() { return error; }
    int error;
  };
  struct Proxy {
    Builder B() { return {42}; }
  };
  struct MErrorDomain {
    struct Acceptor {
      bool IsError() { return true; }
      const int& GetCulprit() && { return value; }
      const int& value;
    };
    Acceptor Verify(Ref<int&&> val) const { return {val.Get()}; }
    Proxy GetErrorBuilder(Any) const { return {}; }
  };
  {
    auto F = [&] {
      MVERIFY(1337).B();
      static_cast<void>(1);
    };
    EXPECT_EQ(42, F());
  }
  {
    auto F = [&] {
      MVERIFY(1337).B() = {666};
      static_cast<void>(1);
    };
    EXPECT_EQ(666, F());
  }
}

TEST(MVerify, RelExpr) {
  struct Domain : ReflectingDomain {
    explicit Domain(bool print_res) : print_res_(print_res) {}
    struct Acceptor {
      bool IsError() { return !value; }
      const bool& GetCulprit() && { return value; }
      const bool& value;
    };
    Acceptor Verify(Ref<bool&&> val) const { return {val.Get()}; }
    bool PrintOperands(int lhs, int rhs, std::string* lhs_str,
                       std::string* rhs_str) const {
      *lhs_str = std::to_string(lhs);
      *rhs_str = std::to_string(rhs);
      return print_res_;
    }

   private:
    bool print_res_;
  };
  {
    auto MErrorDomain = [] { return Domain(true); };
    auto F = [&] {
      MVERIFY(MERROR_TEST_ID(2 + 3 < 4));
      static_cast<void>(1);
    };
    ErrorContext<bool> ctx = F();
    ASSERT_TRUE(ctx.rel_expr);
    EXPECT_EQ("5", ctx.rel_expr->left);
    EXPECT_EQ("4", ctx.rel_expr->right);
    EXPECT_EQ(RelationalOperator::kLt, ctx.rel_expr->op);
  }
  {
    auto MErrorDomain = [] { return Domain(false); };
    auto F = [&] {
      MVERIFY(MERROR_TEST_ID(2 + 3 < 4));
      static_cast<void>(1);
    };
    ErrorContext<bool> ctx = F();
    EXPECT_FALSE(ctx.rel_expr);
  }
}

TEST(MVerify, IfElse) {
  struct MErrorDomain : ReflectingDomain {
    struct Acceptor {
      bool IsError() { return false; }
      Any GetCulprit() && { return {}; }
    };
    Acceptor Verify(Any) const { return {}; }
  };
  auto F = [&](bool val) -> Any {
    // Check that `MVERIFY()` behaves like a statement when put under `if` and
    // `else`.
    if (val) MVERIFY(val);
    if (val)
      MVERIFY(val);
    else
      MVERIFY(val);
    return {};
  };
  F(false);
}

TEST(MVerify, UninitializedVariableWarning) {
  struct MErrorDomain : ReflectingDomain {
    struct Acceptor {
      bool IsError() { return false; }
      Any GetCulprit() && { return {}; }
    };
    Acceptor Verify(Any) const { return {}; }
  };
  []() -> Any {
    int* p;
    MVERIFY(p = nullptr);
    delete p;  // must not generate "uninitialize variable" warning
    return {};
  }();
}

struct VoidBuilder {
  void BuildError() const {}
};

struct PtrDomain {
  template <class Ptr>
  struct Acceptor {
    Ptr&& ptr;
    bool IsError() { return ptr == nullptr; }
    auto GetValue() && -> decltype(*std::forward<Ptr>(ptr)) {
      return *std::forward<Ptr>(ptr);
    }
    std::nullptr_t GetCulprit() && { return nullptr; }
  };
  template <class Ptr>
  Acceptor<Ptr> Try(Ref<Ptr> ptr) const {
    return {ptr.Forward()};
  }
  template <class Context>
  VoidBuilder GetErrorBuilder(const Context&) const {
    return {};
  }
};

struct BoolDomain {
  struct Acceptor {
    bool b;
    bool IsError() { return !b; }
    bool GetCulprit() && { return false; }
  };
  template <class Bool>
  Acceptor Verify(Ref<Bool, bool> b) const {
    return {b.Forward()};
  }
  template <class Context>
  VoidBuilder GetErrorBuilder(const Context&) const {
    return {};
  }
};

TEST(MTry, Lifetime) {
  using MErrorDomain = PtrDomain;
  bool passed = false;
  auto F = [&](int* ptr) {
    passed = false;
    EXPECT_EQ(42, MTRY(WrapUnique(ptr).get()));
    passed = true;
  };
  F(new int(42));
  EXPECT_TRUE(passed);
  F(nullptr);
  EXPECT_FALSE(passed);
}

TEST(MTry, NoCopy) {
  using MErrorDomain = PtrDomain;
  bool passed = false;
  auto F = [&](const char* ptr) {
    passed = false;
    EXPECT_EQ(ptr, &MTRY(ptr));
    passed = true;
  };
  F("");
  EXPECT_TRUE(passed);
  F(nullptr);
  EXPECT_FALSE(passed);
}

TEST(MTry, NotMovable) {
  using MErrorDomain = PtrDomain;
  bool passed = false;
  auto F = [&]() {
    // Neither `p` nor `*p` are movable.
    const std::unique_ptr<const std::unique_ptr<int>> p(
        new std::unique_ptr<int>());
    passed = false;
    EXPECT_EQ(p.get(), &MTRY(p));
    passed = true;
  };
  F();
  EXPECT_TRUE(passed);
}

TEST(MTry, ComplexExpression) {
  using MErrorDomain = PtrDomain;
  auto Addr = [](const int& a) { return &a; };
  auto Plus = [](const int* a, const int* b, int* res) {
    MTRY(res) = MTRY(a) + MTRY(b);
  };
  {
    int n = -1;
    Plus(Addr(2), Addr(3), &n);
    EXPECT_EQ(5, n);
  }
  {
    int n = -1;
    Plus(nullptr, Addr(3), &n);
    EXPECT_EQ(-1, n);
  }
  {
    int n = -1;
    Plus(Addr(2), nullptr, &n);
    EXPECT_EQ(-1, n);
  }
  {
    int n = -1;
    Plus(Addr(2), Addr(3), nullptr);
    EXPECT_EQ(-1, n);
  }
  {
    int n = -1;
    Plus(nullptr, nullptr, nullptr);
    EXPECT_EQ(-1, n);
  }
}

TEST(MTry, NestedMTry) {
  using MErrorDomain = PtrDomain;
  int n;
  auto SetN = [&](const int* const* from) {
    n = -1;
    n = MTRY(MTRY(from));
  };
  {
    int x = 42;
    auto p = &x;
    SetN(&p);
    EXPECT_EQ(42, n);
  }
  {
    int* p = nullptr;
    SetN(&p);
    EXPECT_EQ(-1, n);
  }
  {
    SetN(nullptr);
    EXPECT_EQ(-1, n);
  }
}

TEST(MTry, Lambda) {
  using MErrorDomain = PtrDomain;

  int n;
  auto SetN = [&](const int* p) {
    n = -1;
    n = MTRY([&]() { return p; }());
  };

  SetN(nullptr);
  EXPECT_EQ(-1, n);

  const int x = 42;
  SetN(&x);
  EXPECT_EQ(42, n);
}

TEST(MTry, Context) {
  struct MErrorDomain : ReflectingDomain {
    struct Acceptor {
      bool IsError() { return true; }
      void GetValue() && { ADD_FAILURE(); }
      const int& GetCulprit() && { return value; }
      const int& value;
    };
    Acceptor Try(Ref<int&&> value) const { return {value.Get()}; }
  };
  const int line = __LINE__;
  auto F = [] {
    MTRY(MERROR_TEST_ID(2 + 3), _);
    static_cast<void>(1);
  };
  ErrorContext<int> ctx = F();
  EXPECT_THAT(ctx.macro, Macro::kTry);
  EXPECT_THAT(ctx.location_id, Not(0));
  EXPECT_THAT(ctx.function, HasSubstr("MTry_Context_Test::TestBody()"));
  EXPECT_THAT(ctx.file, EndsWith("macros_test.cc"));
  EXPECT_THAT(ctx.line, line + 2);
  EXPECT_THAT(ctx.macro_str, StrEq("MTRY"));
  EXPECT_THAT(ctx.args_str, StrEq("MERROR_TEST_ID(2 + 3), _"));
  EXPECT_THAT(ctx.culprit, 5);
  EXPECT_FALSE(ctx.rel_expr);
}

TEST(MTry, CustomCulprit) {
  struct MErrorDomain : ReflectingDomain {
    struct Acceptor {
      bool IsError() { return true; }
      void GetValue() && { ADD_FAILURE(); }
      std::string GetCulprit() && { return std::to_string(value); }
      const int& value;
    };
    Acceptor Try(Ref<int&&> value) const { return {value.Get()}; }
  };
  auto F = [] {
    MTRY(42);
    static_cast<void>(1);
  };
  ErrorContext<std::string> ctx = F();
  EXPECT_EQ("42", ctx.culprit);
}

TEST(MTry, DomainFunction) {
  struct Domain : ReflectingDomain {
    struct Acceptor {
      bool IsError() { return true; }
      void GetValue() && { ADD_FAILURE(); }
      const int& GetCulprit() && { return value; }
      const int& value;
    };
    Acceptor Try(Ref<int&&> value) const { return {value.Get()}; }
  };
  auto MErrorDomain = [] { return Domain(); };
  auto F = [&] {
    MTRY(42);
    static_cast<void>(1);
  };
  ErrorContext<int> ctx = F();
}

TEST(MTry, LvalueDomain) {
  struct Domain : ReflectingDomain {
    Domain() {}
    Domain(const Domain&) = delete;
    Domain& operator=(const Domain&) = delete;
    struct Acceptor {
      bool IsError() { return true; }
      void GetValue() && { ADD_FAILURE(); }
      const int& GetCulprit() && { return value; }
      const int& value;
    };
    Acceptor Try(Ref<int&&> value) const { return {value.Get()}; }
  } domain;
  {
    auto MErrorDomain = [&]() -> Domain& { return domain; };
    auto F = [&] {
      MTRY(42);
      static_cast<void>(1);
    };
    ErrorContext<int> ctx = F();
  }
  {
    auto MErrorDomain = [&]() -> const Domain& { return domain; };
    auto F = [&] {
      MTRY(42);
      static_cast<void>(1);
    };
    ErrorContext<int> ctx = F();
  }
}

/* TODO(iserna): Verify why it breaks.
TEST(MTry, RvalueDomain) {
  struct Domain : ReflectingDomain {
    struct Acceptor {
      bool IsError() { return true; }
      void GetValue() && { ADD_FAILURE(); }
      const int& GetCulprit() && { return value; }
      const int& value;
    };
    Acceptor Try(Ref<int&&> value) const {
      EXPECT_EQ(std::string(100, 'x'), lifeline);
      return {value.Get()};
    }
    std::string lifeline = std::string(100, 'x');
  };
  {
    auto MErrorDomain = [](Domain d = {}) -> Domain&& { return std::move(d); };
    auto F = [&] {
      MTRY(42);
      static_cast<void>(1);
    };
    ErrorContext<int> ctx = F();
  }
  {
    auto MErrorDomain = [](Domain d = {}) -> const Domain&& {
      return std::move(d);
    };
    auto F = [&] {
      MTRY(42);
      static_cast<void>(1);
    };
    ErrorContext<int> ctx = F();
  }
}
*/

TEST(MTry, BuilderPatch) {
  struct Builder {
    int BuildError() { return error; }
    Builder Code(int e) { return {e}; }
    int error;
  };
  struct MErrorDomain {
    struct Acceptor {
      bool IsError() { return value < min; }
      void GetValue() && {}
      int& GetCulprit() && { return value; }
      int min;
      int& value;
    };
    Acceptor Try(Ref<int&> value) const { return {min, value.Get()}; }
    MErrorDomain Min(int m) const { return {m}; }
    Builder GetErrorBuilder(Any) const { return {1}; }
    int min;
  };
  {
    auto F = [](int n) {
      MTRY(n);
      return 0;
    };
    EXPECT_EQ(1, F(-1));
    EXPECT_EQ(0, F(0));
  }
  {
    auto F = [](int n) {
      MTRY(n, _);
      return 0;
    };
    EXPECT_EQ(1, F(-1));
    EXPECT_EQ(0, F(0));
  }
  {
    auto F = [](int n) {
      MTRY(n, Code(42));
      return 0;
    };
    EXPECT_EQ(42, F(-1));
    EXPECT_EQ(0, F(0));
  }
  {
    auto F = [](int n) {
      MTRY(n, _.Code(42));
      return 0;
    };
    EXPECT_EQ(42, F(-1));
    EXPECT_EQ(0, F(0));
  }
  {
    auto F = [](int n) {
      MTRY(n, Code(1337) = {42});
      return 0;
    };
    EXPECT_EQ(42, F(-1));
    EXPECT_EQ(0, F(0));
  }
  {
    auto F = [](int n) {
      MTRY(n, _ = {42});
      return 0;
    };
    EXPECT_EQ(42, F(-1));
    EXPECT_EQ(0, F(0));
  }
}

TEST(MTry, VoidExpr) {
  struct MErrorDomain : ReflectingDomain {
    struct Acceptor {
      bool IsError() { return false; }
      int GetValue() && { return 42; }
      Void GetCulprit() && { return {}; }
    };
    Acceptor Try(Ref<Void&&>) const { return {}; }
  };
  bool passed = false;
  auto F = [&]() -> Any {
    int n = MTRY((void)0);
    passed = true;
    EXPECT_EQ(42, n);
    return {};
  };
  F();
  EXPECT_TRUE(passed);
}

}  // namespace
}  // namespace merror
