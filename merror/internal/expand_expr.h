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
// This file is internal to merror. Don't include it directly and don't use
// anything that it defines.

#ifndef MERROR_5EDA97_INTERNAL_EXPAND_EXPR_H_
#define MERROR_5EDA97_INTERNAL_EXPAND_EXPR_H_

#include <type_traits>
#include <utility>

#include "merror/types.h"

// The INTERNAL_MERROR_EXPAND_EXPR() macro can be used for extracting individual
// arguments from relational expressions. For example, given the expression
// `x == y`, this macro can tell you the values of `x` and `y`.
//
// __VA_ARGS__ should be a non-void expression.
//
// Before we get into the semantics of this macro, let's introduce a helper
// function (C is short for Const).
//
//   template <class T>
//   const T& C(const T& val) { return val; }
//
// If the expression is of the form `lhs relop rhs` where `relop` is a
// relational operator (==, !=, <, >, <= or >=), and `C(lhs) relop C(rhs)` is
// a valid expression, the macro returns
// `visitor(lhs, relop_enum, rhs, C(lhs) relop C(rhs))`, where `relop_enum` is
// `std::integral_constant<merror::RelationalOperator, X>`. Otherwise it returns
// `visitor(lhs relop rhs)`.
//
// Example:
//
//   struct Print {
//     template <class L, RelationalOperator Op, class R, class Expr>
//     string operator(const L& lhs,
//                     std::integral_constant<RelationalOperator, Op> op,
//                     const R& rhs,
//                     const Expr& expr) const {
//       std::ostringstream strm;
//       strm << lhs << " " << op << " " << rhs << " => " << expr;
//       return strm.str();
//     }
//
//     template <class Expr>
//     string operator(const Expr& expr) const {
//       std::ostringstream strm;
//       strm << expr;
//       return strm.str();
//     }
//   };
//
//   // 42
//   std::cout << EXPAND_EXPR(42, Print()) << std::endl;
//
//   // 42
//   std::cout << EXPAND_EXPR(40 + 2, Print()) << std::endl;
//
//   // 42 > 41 => true
//   std::cout << EXPAND_EXPR(40 + 2 > 41, Print()) << std::endl;
//
//   // true == 0 => false
//   std::cout << EXPAND_EXPR(2 > 1 == 0, Print()) << std::endl;
//
// The reason INTERNAL_MERROR_EXPAND_EXPR() passes the results to the visitor
// instead of returning them is to have all temporary variables still alive when
// the expression result is passed to the visitor. This avoids lifetime issues.
//
// The `C()` calls are meant to avoid use-after-move bugs when relational
// operators consume their rvalue inputs.
#define INTERNAL_MERROR_EXPAND_EXPR(visitor, expr) \
  INTERNAL_MERROR_EXPAND_EXPR_I(visitor, expr)

///////////////////////////////////////////////////////////////////////////////
//                   IMPLEMENTATION DETAILS FOLLOW                           //
//                    DO NOT USE FROM OTHER FILES                            //
///////////////////////////////////////////////////////////////////////////////

// `INTERNAL_MERROR_EXPAND_EXPR` delegates to `INTERNAL_MERROR_EXPAND_EXPR_I` in
// order to macro-expand `expr` and ensure it has no unparenthesized commas.
//
// If we accepted expressions with commas (for example, by making
// `INTERNAL_MERROR_EXPAND_EXPR` variadic), it would lead to the following
// problem.
//
//   INTERNAL_MERROR_EXPAND_EXPR(visitor, F() == G(), H())
//
// If the comma between `F() == G()` and `H()` is the builtin (the only option
// if `H()` returns `void`), then `operator==` doesn't get evaluated, which is
// bad. If we attempt to fix this issue by providing an overloaded `operator,`
// for `Expr` left-hand-side, we run into another problem: `H()` may get
// evaluated before `F()` and `G()`.
#define INTERNAL_MERROR_EXPAND_EXPR_I(visitor, expr)  \
  ::merror::internal_expand_expr::ExprVisitor::Visit( \
      (::merror::internal_expand_expr::ExprBuilder()->*expr), (visitor))

namespace merror {
namespace internal_expand_expr {

template <class T>
const T& MakeConst(const T& val) {
  return val;
}

#define INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(name, symbol)  \
  struct name {                                                                \
    template <class L, class R, class F>                                       \
    static auto Visit(L&& left, R&& right, F&& f)                              \
        -> decltype(std::forward<F>(f)((std::forward<L>(left)                  \
                                            symbol std::forward<R>(right)))) { \
      return std::forward<F>(f)(                                               \
          (std::forward<L>(left) symbol std::forward<R>(right)));              \
    }                                                                          \
  }

INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(ArrowStar, ->*);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(Mul, *);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(Div, /);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(Mod, %);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(Plus, +);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(Minus, -);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(LShift, <<);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(RShift, >>);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(BitAnd, &);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(BitXor, ^);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(BitOr, |);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(Assign, =);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(PlusAssign, +=);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(MinusAssign, -=);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(MulAssign, *=);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(DivAssign, /=);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(ModAssign, %=);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(LShiftAssign, <<=);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(RShiftAssign, >>=);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(AndAssign, &=);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(XorAssign, ^=);
INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR(OrAssign, |=);

#undef INTERNAL_MERROR_EXPAND_EXPR_FORWARDING_OPERATOR_VISITOR

#define INTERNAL_MERROR_EXPAND_EXPR_INTROSPECTING_OPERATOR_VISITOR(name,       \
                                                                   symbol)     \
  struct name {                                                                \
    template <class L, class R, class F>                                       \
    static auto Visit(L&& left, R&& right, F&& f, int)                         \
        -> decltype(std::forward<F>(f)(                                        \
            std::forward<L>(left),                                             \
            std::integral_constant<RelationalOperator,                         \
                                   RelationalOperator::k##name>(),             \
            std::forward<R>(right),                                            \
            (internal_expand_expr::MakeConst(left)                             \
                 symbol internal_expand_expr::MakeConst(right)))) {            \
      return std::forward<F>(f)(                                               \
          std::forward<L>(left),                                               \
          std::integral_constant<RelationalOperator,                           \
                                 RelationalOperator::k##name>(),               \
          std::forward<R>(right),                                              \
          (internal_expand_expr::MakeConst(left)                               \
               symbol internal_expand_expr::MakeConst(right)));                \
    }                                                                          \
                                                                               \
    template <class L, class R, class F>                                       \
    static auto Visit(L&& left, R&& right, F&& f, unsigned)                    \
        -> decltype(std::forward<F>(f)(std::forward<L>(left)                   \
                                           symbol std::forward<R>(right))) {   \
      return std::forward<F>(f)(std::forward<L>(left)                          \
                                    symbol std::forward<R>(right));            \
    }                                                                          \
                                                                               \
    template <class L, class R, class F, class Self = name>                    \
    static auto Visit(L&& left, R&& right, F&& f)                              \
        -> decltype(Self::Visit(std::forward<L>(left), std::forward<R>(right), \
                                std::forward<F>(f), 0 /*rank*/)) {             \
      return Self::Visit(std::forward<L>(left), std::forward<R>(right),        \
                         std::forward<F>(f), 0 /*rank*/);                      \
    }                                                                          \
  }

INTERNAL_MERROR_EXPAND_EXPR_INTROSPECTING_OPERATOR_VISITOR(Lt, <);
INTERNAL_MERROR_EXPAND_EXPR_INTROSPECTING_OPERATOR_VISITOR(Gt, >);
INTERNAL_MERROR_EXPAND_EXPR_INTROSPECTING_OPERATOR_VISITOR(Le, <=);
INTERNAL_MERROR_EXPAND_EXPR_INTROSPECTING_OPERATOR_VISITOR(Ge, >=);
INTERNAL_MERROR_EXPAND_EXPR_INTROSPECTING_OPERATOR_VISITOR(Eq, ==);
INTERNAL_MERROR_EXPAND_EXPR_INTROSPECTING_OPERATOR_VISITOR(Ne, !=);

#undef INTERNAL_MERROR_EXPAND_EXPR_INTROSPECTING_OPERATOR_VISITOR

// Op is one of the classes defined above: BitAnd, BitXor, etc.
// L and R can be references.
template <class L, class R, class Op>
struct Expr {
  L&& left;
  R&& right;

  // This operator is triggered when Expr is the left argument of a builtin
  // operator `&&`, `||` or `?:`. We handle these operators specially because
  // they have non-standard argument evaluation rules that can't be emulated
  // in user-defined operators.
  explicit operator bool();

#define INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(name, symbol)   \
  template <class T>                                         \
  Expr<Expr<L, R, Op>, T, name> operator symbol(T&& value) { \
    return {std::move(*this), std::forward<T>(value)};       \
  }                                                          \
  static_assert(true, "")

  // Overload all operators with precedence no greater than `->*`
  // except for `&&`, `||` which are handled by the `operator bool()`
  // declared above. Operators with precedence higher than `->*` can be safely
  // bypassed.
  //
  // Since we can't overload `.*`, `INTERNAL_MERROR_EXPAND_EXPR(x.*m)` won't
  // compile even if `x.*m` is a valid expression. One solution would be to
  // pick `<=` as our starting operator but unfortunately it triggers
  // -Wparentheses on GCC.
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(ArrowStar, ->*);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(Mul, *);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(Div, /);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(Mod, %);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(Plus, +);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(Minus, -);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(LShift, <<);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(RShift, >>);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(Lt, <);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(Gt, >);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(Le, <=);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(Ge, >=);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(Eq, ==);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(Ne, !=);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(BitAnd, &);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(BitXor, ^);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(BitOr, |);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(Assign, =);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(PlusAssign, +=);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(MinusAssign, -=);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(MulAssign, *=);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(DivAssign, /=);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(ModAssign, %=);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(LShiftAssign, <<=);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(RShiftAssign, >>=);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(AndAssign, &=);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(XorAssign, ^=);
  INTERNAL_MERROR_EXPAND_EXPR_OPERATOR(OrAssign, |=);

#undef INTERNAL_MERROR_EXPAND_EXPR_OPERATOR
};

template <class T>
struct IsExpr : std::false_type {};

template <class L, class R, class Op>
struct IsExpr<Expr<L, R, Op>> : std::true_type {};

struct ExprVisitor {
  // Helper functor for Visit() defined below.
  template <class Op, class R, class F>
  struct BoundRhs {
    static_assert(!IsExpr<typename std::decay<R>::type>(), "");

    // The right operand of a binary expression or an instance of Expr.
    R&& right_expr;
    // Binary expression visitor.
    F&& f;

    template <class LVal>
    auto operator()(LVal&& left_val) const
        -> decltype(Op::Visit(std::forward<LVal>(left_val),
                              std::forward<R>(right_expr),
                              std::forward<F>(f))) {
      return Op::Visit(std::forward<LVal>(left_val),
                       std::forward<R>(right_expr), std::forward<F>(f));
    }
  };

  template <class T, class F>
  static decltype(std::declval<F>()(std::declval<T>())) Visit(T&& val, F&& f) {
    // This assertion fails when the expression under MVERIFY or a similar
    // macro is invalid.
    //
    //   MVERIFY("hello" == 42);
    //   // error: static_assert failed
    //
    // To get a more helpful error message, parenthesize the expression and
    // recompile:
    //
    //  MVERIFY(("hello" == 42));
    //  // error: comparison between pointer and integer
    static_assert(
        !IsExpr<typename std::decay<T>::type>(),
        "Invalid expression passed to MVERIFY() or a similar macro. "
        "To find the culprit, parenthesize the expression and recompile.");
    return std::forward<F>(f)(std::forward<T>(val));
  }

  template <class L, class R, class Op, class F, class Self = ExprVisitor>
  static auto Visit(Expr<L, R, Op> node, F&& f)
      -> decltype(Self::Visit(std::forward<L>(node.left),
                              BoundRhs<Op, R, F>{std::forward<R>(node.right),
                                                 std::forward<F>(f)})) {
    return Self::Visit(
        std::forward<L>(node.left),
        BoundRhs<Op, R, F>{std::forward<R>(node.right), std::forward<F>(f)});
  }
};

struct CastToBool {
  template <class T>
  bool operator()(T&& val) const {
    // This will call even explicit conversion operators (just what we need).
    return std::forward<T>(val) ? true : false;
  }
};

template <class L, class R, class Op>
Expr<L, R, Op>::operator bool() {
  return ExprVisitor::Visit(std::move(*this), CastToBool());
}

struct Leaf {
  template <class T, class F>
  static decltype(std::declval<F>()(std::declval<T>())) Visit(T&& left,
                                                              const Void& right,
                                                              F&& f) {
    return std::forward<F>(f)(std::forward<T>(left));
  }
};

struct ExprBuilder {};

// We start unwrapping the expression from the left with our catch-all
// overloaded operator->*. We could use any other operator with precedence not
// higher than relational operators. The upside would be a simpler and more
// robust implementation; the downside is that it triggers -Wparentheses on GCC.
template <class T>
Expr<T, const Void&, Leaf> operator->*(ExprBuilder&&, T&& value) {
  static constexpr Void v = {};
  return {std::forward<T>(value), v};
}

}  // namespace internal_expand_expr
}  // namespace merror

#endif  // MERROR_5EDA97_INTERNAL_EXPAND_EXPR_H_
