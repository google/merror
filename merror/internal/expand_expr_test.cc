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

#include "merror/internal/expand_expr.h"

#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "merror/types.h"

namespace merror {
namespace {

using std::nullopt;
using std::optional;
using ::testing::Eq;

template <class Left, class Right, class Result>
struct Expression {
  Expression(Left left, RelationalOperator op, Right right, Result res)
      : left(left), op(op), right(right), res(res) {}
  Left left;
  RelationalOperator op;
  Right right;
  Result res;
};

template <class Left, class Right, class Result>
Expression(Left left, RelationalOperator op, Right right, Result res)
    -> Expression<Left, Right, Result>;

template <class Left, class Right, class Result>
bool operator==(const Expression<Left, Right, Result>& a,
                const Expression<Left, Right, Result>& b) {
  return a.left == b.left && a.op == b.op && a.right == b.right &&
         a.res == b.res;
}

struct ForwardingVisitor {
  template <class Expr>
  typename std::decay<Expr>::type operator()(Expr&& expr) const {
    return std::forward<Expr>(expr);
  }
};

struct ExpandingVisitor : ForwardingVisitor {
  using ForwardingVisitor::operator();

  template <class Left, class Op, class Right, class Res>
  Expression<typename std::decay<Left>::type, typename std::decay<Right>::type,
             typename std::decay<Res>::type>
  operator()(Left&& left, Op op, Right&& right, Res&& res) const {
    return {std::forward<Left>(left), Op::value, std::forward<Right>(right),
            std::forward<Res>(res)};
  }
};

TEST(ExpandExpr, RelationalOps) {
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 == 1),
              Expression(2, RelationalOperator::kEq, 1, false));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 == 2),
              Expression(2, RelationalOperator::kEq, 2, true));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 == 3),
              Expression(2, RelationalOperator::kEq, 3, false));

  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 != 1),
              Expression(2, RelationalOperator::kNe, 1, true));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 != 2),
              Expression(2, RelationalOperator::kNe, 2, false));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 != 3),
              Expression(2, RelationalOperator::kNe, 3, true));

  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 < 1),
              Expression(2, RelationalOperator::kLt, 1, false));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 < 2),
              Expression(2, RelationalOperator::kLt, 2, false));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 < 3),
              Expression(2, RelationalOperator::kLt, 3, true));

  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 > 1),
              Expression(2, RelationalOperator::kGt, 1, true));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 > 2),
              Expression(2, RelationalOperator::kGt, 2, false));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 > 3),
              Expression(2, RelationalOperator::kGt, 3, false));

  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 <= 1),
              Expression(2, RelationalOperator::kLe, 1, false));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 <= 2),
              Expression(2, RelationalOperator::kLe, 2, true));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 <= 3),
              Expression(2, RelationalOperator::kLe, 3, true));

  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 >= 1),
              Expression(2, RelationalOperator::kGe, 1, true));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 >= 2),
              Expression(2, RelationalOperator::kGe, 2, true));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 >= 3),
              Expression(2, RelationalOperator::kGe, 3, false));
}

TEST(ExpandExpr, RelationalOpsWithExtraStuff) {
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 + 3 == 5),
              Expression(5, RelationalOperator::kEq, 5, true));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 1 == (2 > 3)),
              Expression(1, RelationalOperator::kEq, false, false));
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), (1 > 2) == 3),
              Expression(false, RelationalOperator::kEq, 3, false));
}

TEST(ExpandExpr, RelationalOpsWithMixedTypes) {
  auto expanded = INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 42 == 'A');
  // The original argument types are preserved.
  EXPECT_TRUE((std::is_same<decltype(expanded.left), int>()));
  EXPECT_TRUE((std::is_same<decltype(expanded.right), char>()));
  EXPECT_THAT(expanded, Expression(42, RelationalOperator::kEq, 'A', false));
}

TEST(ExpandExpr, RelationalOpsNotExpanded) {
  // If the passed visitor doesn't accept expanded expressions, then they aren't
  // expanded.
  EXPECT_TRUE(INTERNAL_MERROR_EXPAND_EXPR(ForwardingVisitor(), 42 == 42));
  EXPECT_FALSE(INTERNAL_MERROR_EXPAND_EXPR(ForwardingVisitor(), 42 == 1337));
  EXPECT_TRUE(INTERNAL_MERROR_EXPAND_EXPR(ForwardingVisitor(), 2 + 3 == 5));
  EXPECT_FALSE(INTERNAL_MERROR_EXPAND_EXPR(ForwardingVisitor(), 1 == (2 > 3)));
  EXPECT_FALSE(INTERNAL_MERROR_EXPAND_EXPR(ForwardingVisitor(), (1 > 2) == 3));
}

TEST(ExpandExpr, NonRelationalOps) {
  // In these tests we either have no operators or all of them have higher
  // precedence than `<=` and therefore don't get instrumented by
  // INTERNAL_MERROR_EXPAND_EXPR. We check only a few because they go through
  // the same code path.
  EXPECT_EQ(42, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 42));
  EXPECT_EQ(true, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), (42 == 42)));
  EXPECT_EQ(false, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), !42));
  EXPECT_EQ(5, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 2 + 3));

  // These operators have lower precedence than `<=` and therefore get
  // instrumented by INTERNAL_MERROR_EXPAND_EXPR. We check all of them because
  // they go through different code paths.
  EXPECT_EQ(2, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 3 & 6));
  EXPECT_EQ(5, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 3 ^ 6));
  EXPECT_EQ(7, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 3 | 6));
  int n = 0;
  EXPECT_EQ(42, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), n = 42));
  EXPECT_EQ(42, n);
  EXPECT_EQ(44, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), n += 2));
  EXPECT_EQ(44, n);
  EXPECT_EQ(42, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), n -= 2));
  EXPECT_EQ(42, n);
  EXPECT_EQ(84, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), n *= 2));
  EXPECT_EQ(84, n);
  EXPECT_EQ(42, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), n /= 2));
  EXPECT_EQ(42, n);
  EXPECT_EQ(3, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), n %= 39));
  EXPECT_EQ(3, n);
  EXPECT_EQ(12, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), n <<= 2));
  EXPECT_EQ(12, n);
  EXPECT_EQ(3, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), n >>= 2));
  EXPECT_EQ(3, n);
  EXPECT_EQ(2, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), n &= 6));
  EXPECT_EQ(2, n);
  n = 3;
  EXPECT_EQ(5, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), n ^= 6));
  EXPECT_EQ(5, n);
  n = 3;
  EXPECT_EQ(7, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), n |= 6));
  EXPECT_EQ(7, n);
}

TEST(ExpandExpr, ShortCircuitingOps) {
  // Operators `&&`, `||` and `?:` have short-circuiting behavior that must
  // not be broken by INTERNAL_MERROR_EXPAND_EXPR.
  struct Falsy {
    explicit operator bool() const { return false; }
  };
  struct Truthy {
    explicit operator bool() const { return true; }
  };
  auto Fail = [] {
    ADD_FAILURE();
    return 0;
  };
  EXPECT_EQ(true,
            INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), true || Fail()));
  EXPECT_EQ(true,
            INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 42 || Fail()));
  EXPECT_EQ(true, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(),
                                              Truthy() || Fail()));
  EXPECT_EQ(false,
            INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), false && Fail()));
  EXPECT_EQ(false,
            INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 0 && Fail()));
  EXPECT_EQ(false,
            INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), nullptr && Fail()));
  EXPECT_EQ(false,
            INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), Falsy() && Fail()));
  EXPECT_EQ(
      42, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), true ? 42 : Fail()));
  EXPECT_EQ(42,
            INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 666 ? 42 : Fail()));
  EXPECT_EQ(42, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(),
                                            Truthy() ? 42 : Fail()));
  EXPECT_EQ(
      42, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), false ? Fail() : 42));
  EXPECT_EQ(42,
            INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), 0 ? Fail() : 42));
  EXPECT_EQ(42, INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(),
                                            Falsy() ? Fail() : 42));
}

TEST(ExpandExpr, ShortCircuitingOpsLifetime) {
  // This tests shows that in one corner case INTERNAL_MERROR_EXPAND_EXPR
  // changes the lifetime semantics of temporary objects.
  //
  // If you have a short-circuiting expression involving operators `&&`, `||` or
  // `?:`, the temporaries created during the evaluation of the left operand
  // normally stay alive until the end of the whole expression. But with
  // INTERNAL_MERROR_EXPAND_EXPR they get destroyed as soon as the
  // left operand is evaluated.
  class Weird {
   public:
    Weird(bool* dead) : dead_(dead) {}
    Weird(const Weird&) { ADD_FAILURE(); }
    ~Weird() {
      EXPECT_FALSE(*dead_);
      *dead_ = true;
    }
    Weird operator==(bool* dead) const { return Weird(dead); }
    explicit operator bool() const { return true; }

   private:
    bool* dead_;
  };

  {
    bool one = false;
    bool two = false;
    // Here's what happens in this test.
    // 1. `Weird{&one} == &two` evaluates to `Weird{&two}`.
    // 2. `Weird{&two}` is converted to `true`.
    // 3. `Weird{&two}` is destroyed.
    // 4. The lambda is called with `42` as an argument.
    // 5. `Weird{&one}` is destroyed.
    //
    // Note that `Weird{&two}` got destroyed before the lambda is called but
    // `Weird{&one}` is still alive. Only temporaries that get created as a
    // result of some operator evaluation get destroyed too early.
    INTERNAL_MERROR_EXPAND_EXPR(
        [&](int n) {
          EXPECT_EQ(42, n);
          EXPECT_FALSE(one);
          EXPECT_TRUE(two);
        },
        Weird{&one} == &two ? 42 : 1337);
  }

  {
    bool one = false;
    bool two = false;
    // The same expression without INTERNAL_MERROR_EXPAND_EXPR.
    [&](int n) {
      EXPECT_EQ(42, n);
      EXPECT_FALSE(one);
      // `Weird{&two}` is still alive.
      EXPECT_FALSE(two);
    }(Weird{&one} == &two ? 42 : 1337);
  }
}

TEST(ExpandExpr, ArrowStar) {
  struct S {
    int value;
  } s = {42};
  EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), &s->*&S::value),
              Eq(42));
  EXPECT_THAT(
      INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), &s->*&S::value == 24),
      Expression(42, RelationalOperator::kEq, 24, false));
  // The following doesn't work, unfortunately. It's a limitation of the
  // implementation.
  // EXPECT_THAT(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), s.*&S::value),
  //             Eq(42));
}

// IntExpr is either an int or a function that produces an int.
class IntExpr {
 public:
  explicit IntExpr(int value) : value_(value) {}
  IntExpr(const IntExpr&) { ADD_FAILURE(); }

  ~IntExpr() {
    // Ensure an error if Get() is called on a dead instance.
    compute_ = nullptr;
    value_ = nullopt;
  }

  explicit IntExpr(std::function<int()> compute)
      : compute_(std::move(compute)) {
    assert(compute_);
  }

  int Get() const {
    assert(value_ || compute_);
    return value_ ? *value_ : compute_();
  }

 private:
  // Exactly one of these is not empty.
  std::function<int()> compute_;
  optional<int> value_;
};

// Note that Get() on the results of these operators will call Get() on the
// arguments. The arguments must be still alive at that point.

// Higher precedence than relational operators.
IntExpr operator+(const IntExpr& a, const IntExpr& b) {
  return IntExpr{[&]() { return a.Get() + b.Get(); }};
}

// Relational operator.
IntExpr operator==(const IntExpr& a, const IntExpr& b) {
  return IntExpr{[&]() { return a.Get() == b.Get(); }};
}

// Lower precedence than relational operators.
IntExpr operator|(const IntExpr& a, const IntExpr& b) {
  return IntExpr{[&]() { return a.Get() | b.Get(); }};
}

struct IntExprVerifier {
  IntExprVerifier() = default;
  void operator()(const IntExpr& expr) {
    ASSERT_TRUE(expected);
    EXPECT_EQ(expected, expr.Get());
    expected = nullopt;
  }
  optional<int> expected;
};

TEST(ExpandExpr, Lifetime) {
  // Verify that all temporary objects are still alive when the visitor is
  // called.
  INTERNAL_MERROR_EXPAND_EXPR(IntExprVerifier{42}, IntExpr(42));
  INTERNAL_MERROR_EXPAND_EXPR(IntExprVerifier{5}, IntExpr(2) + IntExpr(3));
  INTERNAL_MERROR_EXPAND_EXPR(IntExprVerifier{1}, IntExpr(42) == IntExpr(42));
  INTERNAL_MERROR_EXPAND_EXPR(IntExprVerifier{7}, IntExpr(3) | IntExpr(6));
  INTERNAL_MERROR_EXPAND_EXPR(IntExprVerifier{1},
                              IntExpr(2) + IntExpr(3) == IntExpr(5));
  INTERNAL_MERROR_EXPAND_EXPR(IntExprVerifier{1},
                              IntExpr(5) == IntExpr(2) + IntExpr(3));
  INTERNAL_MERROR_EXPAND_EXPR(IntExprVerifier{3},
                              IntExpr(2) | (IntExpr(5) == IntExpr(5)));
  INTERNAL_MERROR_EXPAND_EXPR(IntExprVerifier{3},
                              (IntExpr(5) == IntExpr(5)) | IntExpr(2));
  INTERNAL_MERROR_EXPAND_EXPR(IntExprVerifier{0},
                              (IntExpr(42) == IntExpr(42)) == IntExpr(42));
}

TEST(ExpandExpr, NonConstRelationalOps) {
  struct S {
    // Note: not const.
    bool operator==(const S&) { return true; }
  };
  // Expression not being expanded because operator== isn't const.
  EXPECT_TRUE(INTERNAL_MERROR_EXPAND_EXPR(ExpandingVisitor(), S() == S()));
}

}  // namespace
}  // namespace merror
