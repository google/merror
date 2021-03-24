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
//                  ====[ Introduction ]====
//
// This file defines error handling macros MERROR, MVERIFY and MTRY.
//
//   * `MERROR()` creates an error, which can be returned or passed around.
//   * `MVERIFY(expr)` returns an error if `expr` is an error.
//   NOTE: MTRY is a macro that uses non-standard compiler extensions.
//   * `MTRY(expr)` returns an error if `expr` is an error; otherwise it
//     evaluates to the value extracted from `expr`. `MTRY(expr)` behaves as
//     `expr` in all non-error contexts.
//
// The macros use the Error Domain to determine if a given expression produced
// an error, build an error to return, or extract the value from the expression.
//
//                  ====[ Error Domain ]====
//
// The merror macros find Error Domain via special name `MErrorDomain`. The
// expression `MErrorDomain()`, when evaluated in the lexical scope of the
// macro, must return an object of class type with the following public methods:
//
//   // Called by `MVERIFY(expr)` to determine whether `expr` is an error.
//   // `R` is `decltype((expr))&&`. `T` is `R` minus the
//   //  reference qualifier, `const` and `volatile`.
//   //
//   // The result can be a reference. It can alias `expr`.
//   VerifyAcceptor Verify(Ref<R, T> expr) const;
//
//   // Called by `MTRY(expr)`  to determine whether `expr` is an error.
//   //
//   // `R` is `decltype((expr))&&`. `T` is `R` minus the
//   // reference qualifier, `const` and `volatile`.
//   //
//   // `R` is `merror::Void&&` if the argument of `MTRY` is `void`.
//   //
//   // The result can be a reference. It can alias `expr`.
//   TryAcceptor Try(Ref<R, T> expr) const;
//
//   // Called by `MVERIFY(expr)` if `expr` is of the form `lhs relop rhs`,
//   // where `relop` is a relational operator.
//   //
//   // Should either do nothing and return `false` or produce human-readable
//   // representation of the arguments and return `true`.
//   bool PrintOperands(const Lhs& lhs, const Rhs& rhs,
//                      string* lhs_str, string* rhs_str) const;
//
//   // Called by the macros when an error needs to be built.
//   //
//   // When called by `MERROR(culprit)`, `Culprit` is `decltype((culprit))` or
//   // `merror::Void&&` if the culprit is empty.
//   //
//   // When called by `MVERIFY()` and `MTRY()`, `Culprit` is
//   // `decltype(Verify(...).GetCulprit())&&` and
//   // `decltype(Try(...).GetCulprit())&&` respectively.
//   //
//   // The result can be a reference. It can alias `ctx`.
//   ErrorBuilder GetErrorBuilder(merror::Context<Macro, Culprit>&& ctx) const;
//
// `VerifyAcceptor`, the result type of `Verify()`, must have the following
// methods:
//
//   // Called by `MVERIFY(expr)` to determine whether `expr` is an error.
//   // Called exactly once.
//   bool IsError();
//
//   // Returns the object to which one can point a finger and say,
//   // "*That* error has happened." It may be a reference. It may alias the
//   // expression passed to `MVERIFY()`. `std::decay_t<Culprit>` must be
//   // constructible from `Culprit&&`. Called exactly once if `IsError()` has
//   // returned true. Otherwise not called.
//   Culprit GetCulprit() &&;
//
// `TryAcceptor`, the result type of `Try()`, must have the same methods as
// `VerifyAcceptor` plus one more:
//
//   // Called by `MTRY(expr)`  to extract value from
//   // `expr` after establishing that `IsError()` is `false`.
//   //
//   // `Value` may be a reference type. It may alias `expr`. It may be `void`.
//   // Called at most once if `IsError()` has returned true. Otherwise not
//   // called.
//   Value GetValue() &&;
//
// The expansions of `MERROR() and `MVERIFY()` end with the call to
// `GetErrorBuilder()`, which allows the users of these macros to interact with
// the builder: call methods, stream additional diagnostics, etc. `MTRY()`
// allows this via Builder Patches described below. The result type
// of these manipulations should be a class type with the following method:
//
//   // Called by the macros to finalize building of the error right before
//   // returning it.
//   //
//   // The method may have const and/or reference qualifiers as long as they
//   // are compatible with the builder expression.
//   Error BuildError() [const][&|&&];
//
// If `BuildError()` isn't public, or if its result type's move-constructor
// isn't public, `MErrorAccess<T>` must be declared a friend. See documentation
// for `MErrorAccess` in //util/merror/types.h for details.
//
// If `MErrorDomain()` is an rvalue, it must be move-constructible. If it's an
// lvalue, it must NOT be invalidated at the end of the `MErrorDomain()`
// expression; it must stay valid until the macro is fully evaluated.
//
// `MErrorDomain` may be a type, a function, or a functor.
//
//                  ====[ Builder Patch ]====
//
// A builder patch is a chain of method and operator calls intended to provide
// extra error information. Builder patches can be specified to the right of
// `MERROR()` and `MVERIFY()`, as an optional second argument of `MTRY()`.
//
//   MVERIFY(2 + 2 == 4).ErrorCode(INTERNAL) << "Armageddon";
//   return MERROR().ErrorCode(INTERNAL) << "Armageddon";
//
// When used with `MTRY()` a builder patch must start either
// with a method name or underscore. The latter is a shorthand for the current
// builder. There are several syntactic ways to pass a builder patch:
//
//   // No builder patch.
//   MTRY(Stuff());
//   // The same as above.
//   MTRY(Stuff(), _);
//   // Non-trivial builder patch.
//   MTRY(Stuff(), ErrorCode(INTERNAL) << "Armageddon");
//   // The same as above.
//   MTRY(Stuff(), _.ErrorCode(INTERNAL) << "Armageddon");
//   // We have to use `_` here because builder patches can't start with `<<`.
//   MTRY(Stuff(), _ << "Armageddon");
//
// Builder patches are evaluated only in case of error.
//
//                     ====[ Example ]====
//
// Here's a simple Error Domain that can be used in functions that return
// `false` on error. It supports `bool` `MVERIFY()` and `optional<T>` `MTRY()`
// arguments.
//
//   struct MyErrorBuilder {
//     bool BuildError() const { return false; }
//   };
//
//   struct MyVerifyAcceptor {
//     bool IsError() const { return !value; }
//     bool GetCulprit() const { return false; }
//     bool value;
//   };
//
//   template <class Opt>
//   struct MyTryAcceptor {
//     // An rvalue or lvalue reference to `[const] [volatile] optional<T>`.
//     Opt opt;
//     bool IsError() const { return !opt; }
//     nullopt_t GetCulprit() const { return nullopt; }
//     auto GetValue() const -> decltype(*std::forward<Opt>(opt)) {
//       return *std::forward<Opt>(opt);
//     }
//   };
//
//   struct MyErrorDomain {
//     MyVerifyAcceptor Verify(bool expr) const { return {expr}; }
//
//     template <class R, class T>
//     MyTryAcceptor<R> Try(Ref<R, optional<T>> expr) const {
//       return {expr.Forward()};
//     }
//
//     bool PrintOperands(const Lhs& lhs, const Rhs& rhs,
//                        string* lhs_str, string* rhs_str) const {
//       return false;
//     }
//
//     template <class Context>
//     MyErrorBuilder GetErrorBuilder(Context&&) const {
//       return {};
//     }
//   };
//
// If can be used as follows:
//
//   optional<string> UserName();
//
//   bool Greet(int n) {
//     using MErrorDomain = MyErrorDomain;
//     // Returns `false` unless `n >= 0`.
//     MVERIFY(n >= 0);
//     // Returns `false` if `UserName()` is `nullopt`.
//     string user_name = MTRY(UserName());
//     while (n--) {
//       std::cout << "Hello, " << user_name << std::endl;
//     }
//     return true;
//   }

#ifndef MERROR_5EDA97_MACROS_H_
#define MERROR_5EDA97_MACROS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "merror/internal/expand_expr.h"
#include "merror/internal/preprocessor.h"
#include "merror/internal/tls_map.h"
#include "merror/types.h"

// Defines an error domain in the current scope. Other merror macros such as
// `MVERIFY` and `MTRY` rely on the error domain to perform their duties.
//
// `MERROR_DOMAIN(Base)` expands to `const auto MErrorDomain = (Base())`, which
// allows you to apply a patch by pasting it after the macro. It can be
// optionally preceded by `constexpr`.
//
//   constexpr MERROR_DOMAIN(merror::Default).Log(ERROR);
//
// `MERROR_DOMAIN()` without arguments expands to two declarations:
//
//   const auto& _unique_variable_ = MErrorDomain;
//   const auto MErrorDomain = _unique_variable_
//
// You can use this form to patch an error domain that you already have in
// scope. For example, suppose you have `MErrorDomain` in the file scope and
// want to use `MErrorDomain.Log(ERROR)` as your error domain in one of the
// functions.
//
//   // At the top of foo.cc.
//   //
//   // Within foo.cc, the error domain is
//   // `merror::Default().DefaultErrorCode(UNKNOWN)`.
//   constexpr MERROR_DOMAIN(merror::Default).DefaultErrorCode(UNKNOWN);
//
//   Status Foo(int n) {
//     // Within `Foo()`, the error domain is
//     // `merror::Default().DefaultErrorCode(UNKNOWN).Log(ERROR)`.
//     MERROR_DOMAIN().Log(ERROR);
//     ...
//   }
#define MERROR_DOMAIN(...) \
  MERROR_INTERNAL_MERROR_DOMAIN(MErrorDomain, __VA_ARGS__)

// Creates an error, which can be returned or passed around.
//
//   Status MakeSandwich() {
//     constexpr MERROR_DOMAIN(merror::Default);
//     return MERROR(PERMISSION_DENIED) << "Eat vegetables";
//   }
//
// Can be called either with no arguments or with the culprit as the argument.
//
// See comments at the top of the file for more info.
#define MERROR(...) \
  MERROR_INTERNAL_MERROR("MERROR", #__VA_ARGS__, (MErrorDomain()), __VA_ARGS__)

// If the specified expression is an error, returns an error (possibly of
// different type than the passed expression).
//
//   Status CookBreakfast(int hunger_level) {
//     constexpr MERROR_DOMAIN(merror::Default);
//     MVERIFY(hunger_level >= 0).ErrorCode(INVALID_ARGUMENT) << "Huh?";
//     while (hunger_level--) {
//       MVERIFY(MakeSandwich());
//     }
//     return Status::OK;
//   }
//
// See comments at the top of the file for more info.
#define MVERIFY(...)                                                      \
  MERROR_INTERNAL_MVERIFY_IMPL("MVERIFY", #__VA_ARGS__, (MErrorDomain()), \
                               __VA_ARGS__)

// If the specified expression is an error, returns an error (possibly of
// different type than the passed expression). Otherwise evaluates to the value
// extracted from the expression.
//
//   StatusOr<string> ReadFile(StringPiece filepath);
//
//   Status PrintFile(StringPiece filepath) {
//     constexpr MERROR_DOMAIN(merror::Default);
//     string content = MTRY(ReadFile(filepath));
//     std::cout << content << std::endl;
//     return Status::OK;
//   }
//
// A builder patch can be passed as an optional second argument.
//
//   StatusOr<string> ReadDb(StringPiece row, StringPiece column);
//   optional<UserProfile> ParseUserProfile(StringPiece serialized);
//
//   StatusOr<string> GetEmailAddress(string username) {
//     constexpr MERROR_DOMAIN(merror::Default);
//     string serialized_profile =
//         MTRY(ReadDb(username, "profile"), _ << "Username: " << username);
//     UserProfile profile =
//         MTRY(ParseUserProfile(serialized_profile), ErrorCode(INTERNAL));
//     return profile.email_address();
//   }
//
// See comments at the top of the file for more info.
#define MTRY(...)                                                             \
  MERROR_INTERNAL_APPLY_VARIADIC(MERROR_INTERNAL_MTRY_, "MTRY", #__VA_ARGS__, \
                                 (MErrorDomain()), __VA_ARGS__)

///////////////////////////////////////////////////////////////////////////////
//                   IMPLEMENTATION DETAILS FOLLOW                           //
//                    DO NOT USE FROM OTHER FILES                            //
///////////////////////////////////////////////////////////////////////////////

#define MERROR_INTERNAL_MERROR(MACRO, ARGS, DOMAIN, ...)                       \
  ::merror::MErrorAccess<::merror::internal_macros::ErrorBuilderFinalizer>() = \
      ::merror::internal_macros::Const((DOMAIN)).GetErrorBuilder(              \
          ::merror::internal::MakeContext<::merror::Macro::kError>(            \
              ::merror::internal_macros::TypeId([] {}), __PRETTY_FUNCTION__,   \
              __FILE__, __LINE__, MACRO, ARGS,                                 \
              MERROR_INTERNAL_IF(MERROR_INTERNAL_IS_EMPTY(__VA_ARGS__),        \
                                 ::merror::Void(), (__VA_ARGS__)),             \
              nullptr))

// `TMP` is a unique variable name.
// `DOMAIN` is `MErrorDomain` when expanded from `MERROR_DOMAIN`.
// The pragmas are disabling the ClangTidy's clang-diagnostic-shadow check.
#ifdef __clang__
#define MERROR_INTERNAL_MERROR_DOMAIN(DOMAIN, ...)                \
  _Pragma("clang diagnostic push")                                \
      _Pragma("clang diagnostic ignored \"-Wshadow\"")            \
          MERROR_INTERNAL_MERROR_DOMAIN_IMPL(DOMAIN, __VA_ARGS__) \
              _Pragma("clang diagnostic pop")
#else
#define MERROR_INTERNAL_MERROR_DOMAIN(DOMAIN, ...) \
  MERROR_INTERNAL_MERROR_DOMAIN_IMPL(DOMAIN, __VA_ARGS__)
#endif

#define MERROR_INTERNAL_MERROR_DOMAIN_IMPL(DOMAIN, ...)                       \
  MERROR_INTERNAL_IF(MERROR_INTERNAL_IS_EMPTY(__VA_ARGS__),                   \
                     static_assert(1, "");                                    \
                     auto&& merror__##DOMAIN##__patch__tmp__var = DOMAIN();   \
                     const auto DOMAIN = merror__##DOMAIN##__patch__tmp__var, \
                     const auto DOMAIN = (__VA_ARGS__()))

// Some compilers emit a warning for code like:
//
//   if (foo)
//     if (bar) { } else baz;
//
// because they think you might want the `else` to bind to the first `if`. This
// leads to problems with code like:
//
//   if (condition) MVERIFY(expr);
//
// The `switch (convertible-to-zero) case 0:` idiom is used to suppress this.
#define MERROR_INTERNAL_MVERIFY_IMPL(MACRO, ARGS, DOMAIN, EXPR)                \
  switch (const auto _gverify_domain_ =                                        \
              ::merror::internal_macros::WrapDomain((DOMAIN)))                 \
  case 0:                                                                      \
  default:                                                                     \
    if (auto _gverify_val_ = INTERNAL_MERROR_EXPAND_EXPR(                      \
            ::merror::internal_macros::MakeVerifier(&_gverify_domain_.value),  \
            EXPR)) {                                                           \
    } else /* NOLINT */                                                        \
      return ::merror::MErrorAccess<                                           \
                 ::merror::internal_macros::ErrorBuilderFinalizer>() =         \
                 _gverify_domain_.value.GetErrorBuilder(                       \
                     ::merror::internal::MakeContext<                          \
                         ::merror::Macro::kVerify>(                            \
                         ::merror::internal_macros::TypeId([] {}),             \
                         __PRETTY_FUNCTION__, __FILE__, __LINE__, MACRO, ARGS, \
                         ::std::move(*_gverify_val_.culprit),                  \
                         _gverify_val_.rel_expr.get()))

#define MERROR_INTERNAL_APPLY_VARIADIC(F, MACRO, ARGS, DOMAIN, ...) \
  MERROR_INTERNAL_VCAT(F, MERROR_INTERNAL_NARG(__VA_ARGS__))        \
  (MACRO, ARGS, DOMAIN, __VA_ARGS__)

// If `__VA_ARGS__` starts with an underscore token, removes it. Otherwise
// expands to `.__VA_ARGS__`.
#define MERROR_INTERNAL_HANDLE_UNDERSCORE(...)                   \
  MERROR_INTERNAL_VCAT(                                          \
      MERROR_INTERNAL_HANDLE_UNDERSCORE_,                        \
      MERROR_INTERNAL_STARTS_WITH_UNDERSCORE_TOKEN(__VA_ARGS__)) \
  (__VA_ARGS__)

#define MERROR_INTERNAL_HANDLE_UNDERSCORE_0(...) .__VA_ARGS__
#define MERROR_INTERNAL_HANDLE_UNDERSCORE_1(...) \
  MERROR_INTERNAL_VCAT(MERROR_INTERNAL_EAT_UNDERSCORE, __VA_ARGS__)
#define MERROR_INTERNAL_EAT_UNDERSCORE_

namespace merror {

namespace internal_macros {

template <class T>
const T& Const(const T& val) {
  return val;
}

// Never called in evaluated context.
template <class T>
T* Ptr(T&&) {
  abort();
}

// `T` is either const or a reference to const.
template <class T>
struct DomainWrapper {
  T value;
  operator int() const { return 0; }
};

template <class T>
DomainWrapper<const T> WrapDomain(T&& value) {
  static_assert(!std::is_reference<T>(), "");
  return {std::move(value)};
}
template <class T>
DomainWrapper<const T&> WrapDomain(const T& value) {
  return {value};
}
template <class T>
DomainWrapper<const T&> WrapDomain(T& value) {
  return {value};
}

// Similar to std::optional<T>. We don't use std::optional<T> to avoid
// the dependency and also to be able to store absl::nullopt_t as the value.
template <class T>
class Optional {
 public:
  Optional() : has_value_(false) {}
  Optional(Optional&& other) : has_value_(other.has_value_) {
    if (other.has_value_) new (&value_) T(std::move(other.value_));
  }
  ~Optional() {
    if (has_value_) value_.~T();
  }
  explicit operator bool() const { return has_value_; }
  T& operator*() {
    assert(has_value_);
    return value_;
  }
  T* operator->() {
    assert(has_value_);
    return &value_;
  }
  T* get() { return has_value_ ? &value_ : nullptr; }
  template <class... Args>
  void emplace(Args&&... args) {
    assert(!has_value_);
    new (&value_) T(std::forward<Args>(args)...);
    has_value_ = true;
  }
  void clear() {
    assert(has_value_);
    value_.~T();
    has_value_ = false;
  }

 private:
  union {
    T value_;
  };
  bool has_value_;
};

template <class Culprit>
struct VerificationResult {
  Optional<Culprit> culprit;
  Optional<RelationalExpression> rel_expr;

  // This conversion is used directly by `MVERIFY()`. It determines whether
  // `MVERIFY()` succeeds or fails.
  explicit operator bool() const { return !culprit; }
};

template <class T>
Ref<T&&> MakeRef(T&& ref) {
  return std::forward<T>(ref);
}

template <class T>
struct VoidT {
  using type = void;
};

template <class Domain, class Arg>
using VerificationCulprit = decltype(std::declval<const Domain&>()
                                         .Verify(std::declval<Ref<Arg&&>>())
                                         .GetCulprit());

template <class Domain, class Arg, class = void>
struct IsValidVerifyArg : std::false_type {};

template <class Domain, class Arg>
struct IsValidVerifyArg<Domain, Arg,
                        typename VoidT<VerificationCulprit<Domain, Arg>>::type>
    : std::true_type {};

template <class Domain>
struct Verifier {
  // This overload is called when the argument of `MVERIFY()` is a relational
  // expression.
  template <class L, class Op, class R, class Expr,
            class Culprit = VerificationCulprit<Domain, Expr>>
  VerificationResult<typename std::decay<Culprit>::type> operator()(
      const L& left, Op op, const R& right, Expr&& expr) const {
    // Assume that the acceptor doesn't alias any temporaries that might be
    // created (and destroyed) during the call to `Verify()`.
    auto&& acceptor =
        domain.Verify(internal_macros::MakeRef(std::forward<Expr>(expr)));
    VerificationResult<typename std::decay<Culprit>::type> res;
    if (MERROR_PREDICT_FALSE(acceptor.IsError())) {
      res.culprit.emplace(
          std::forward<decltype(acceptor)>(acceptor).GetCulprit());
      res.rel_expr.emplace();
      if (domain.PrintOperands(left, right, &res.rel_expr->left,
                               &res.rel_expr->right)) {
        res.rel_expr->op = op;
      } else {
        res.rel_expr.clear();
      }
    }
    return res;
  }

  // This overload is called when the argument of `MVERIFY()` isn't a relational
  // expression.
  template <class Expr, class Culprit = VerificationCulprit<Domain, Expr>>
  VerificationResult<typename std::decay<Culprit>::type> operator()(
      Expr&& expr) const {
    // Assume that the acceptor doesn't alias any temporaries that might be
    // created (and destroyed) during the call to `Verify()`.
    auto&& acceptor =
        domain.Verify(internal_macros::MakeRef(std::forward<Expr>(expr)));
    VerificationResult<typename std::decay<Culprit>::type> res;
    if (MERROR_PREDICT_FALSE(acceptor.IsError())) {
      res.culprit.emplace(
          std::forward<decltype(acceptor)>(acceptor).GetCulprit());
    }
    return res;
  }

  // The sole purpose of this overload is to generate a better error message
  // when `MVERIFY()` is passed an argument of unsupported type.
  template <class Expr, class = typename std::enable_if<
                            !IsValidVerifyArg<Domain, Expr>::value>::type>
  VerificationResult<int> operator()(Expr&& expr) const {
    // This assertion fails if you pass an expression of unsupported type to
    // MVERIFY().
    //
    //   MVERIFY(42);
    //
    // Type parameter `Expr` is the type of the expression. In this example it's
    // `int`.
    static_assert(sizeof(Expr) == 0,
                  "MVERIFY() can't handle arguments of type Expr");
    // The following expression always generates a compile error (we ended up in
    // this overload of `operator()` precisely because this expression is
    // malformed). The diagnostics can be helpful for understanding the reasons
    // why `MVERIFY(expr)` fails to compile.
    domain.Verify(internal_macros::MakeRef(std::forward<Expr>(expr)))
        .GetCulprit();
    return {};
  }

  const Domain& domain;
};

struct ErrorBuilderFinalizer {};

template <class T>
struct Expr {
  T&& get() const { return std::forward<T>(expr); }
  T&& expr;
};

// We overload `operator,` here in order to support `void` arguments in
// `MTRY()`.
//
// `MTRY(x)` expands to something like `((x), Expr<Void>()).get()`. If `x` is
// `void`, the comma in the expression is the built-in `operator,`. Otherwise
// it's our overloaded `operator,`.
template <class U, class T>
Expr<U> operator,(U&& expr, Expr<T>) {
  return {std::forward<U>(expr)};
}

template <class Domain>
Verifier<Domain> MakeVerifier(const Domain* domain) {
  return {*domain};
}

// The macros pass a lambda as an argument:
//
//   TypeId([] {})
//
// Since lambdas have unique types, this gives us unique integers for macro
// expansions.
template <class T>
uintptr_t TypeId(const T&) {
  static constexpr char c = 0;
  return reinterpret_cast<uintptr_t>(&c);
}

}  // namespace internal_macros

template <>
struct MErrorAccess<internal_macros::ErrorBuilderFinalizer> {
  // We use `operator=` here because it has the lowest priority among all
  // operators (except `operator,`) and right-to-left associativity. This frees
  // all other operators other than `operator,` to be used by the user to
  // interact with the builder.
  //
  // The following snippet works because `operator<<` has higher priority than
  // `operator=`:
  //
  //   MERROR() << "OMG";
  //
  // It expands roughly to this:
  //
  //   MErrorAccess<ErrorBuilderFinalizer>() =
  //       MErrorPolicy().GetErrorBuilder() << "OMG";
  //
  // There are two overloads of `operator=` because we can't use
  // ABSL_MUST_USE_RESULT if the result type happens to be `void`.
  template <class Builder,
            class R = decltype(std::declval<Builder>().BuildError()),
            typename std::enable_if<std::is_void<R>::value, int>::type = 0>
  R operator=(Builder&& builder) const {
    return std::forward<Builder>(builder).BuildError();
  }

  template <class Builder,
            class R = decltype(std::declval<Builder>().BuildError()),
            typename std::enable_if<!std::is_void<R>::value, int>::type = 0>
  [[nodiscard]] R operator=(Builder&& builder) const {
    return std::forward<Builder>(builder).BuildError();
  }
};

}  // namespace merror

///////////////////////////////////////////////////////////////////////////////
//                   IMPLEMENTATION DETAILS FOLLOW                           //
//                    DO NOT USE FROM OTHER FILES                            //
///////////////////////////////////////////////////////////////////////////////

// The static_assert() will trigger if the type of EXPR differs from expansion
// to expansion. For example, it will trigger in the following case:
//
//   // The type of one instance of `[] {}` isn't the same as the type of
//   // another instance of the same expression. `static_assert` triggers.
//   MTRY([] {});
//
// EXPR can have lambdas in it as long as the type of the expression doesn't
// depend on the types of the lambdas. Thus, the following is legal:
//
//   // The type of the expression is `int*`. `static_assert` doesn't trigger.
//   MTRY([]() -> int* { return nullptr; }());
//
#define MERROR_INTERNAL_MTRY_IMPL(MACRO, ARGS, KEY, DOMAIN, EXPR,              \
                                  BUILDER_PATCH)                               \
  (MERROR_INTERNAL_MTRY_STATE(DOMAIN, EXPR, KEY).GetSelfOrNull() ?: ({         \
    constexpr auto _gtry_state1_ =                                             \
        true ? nullptr                                                         \
             : ::merror::internal_macros::Ptr(                                 \
                   MERROR_INTERNAL_MTRY_STATE(DOMAIN, EXPR, KEY));             \
    constexpr auto _gtry_state2_ =                                             \
        true ? nullptr                                                         \
             : ::merror::internal_macros::Ptr(                                 \
                   MERROR_INTERNAL_MTRY_STATE(DOMAIN, EXPR, KEY));             \
    static_assert(                                                             \
        ::std::is_same<decltype(_gtry_state1_),                                \
                       decltype(_gtry_state2_)>::value,                        \
        "Sorry. MTRY() can't be called with this argument, since the "         \
        "argument results in different types when evaluated more than once."); \
    auto* _gtry_stash_ = ::merror::internal::tls_map::Get<                     \
        typename std::remove_pointer<decltype(_gtry_state1_)>::type::Stash>(   \
        KEY);                                                                  \
    return ::merror::MErrorAccess<                                             \
               ::merror::internal_macros::ErrorBuilderFinalizer>() =           \
               _gtry_stash_->GetDomain().GetErrorBuilder(                      \
                   ::merror::internal::MakeContext<::merror::Macro::kTry>(     \
                       ::merror::internal_macros::TypeId([] {}),               \
                       __PRETTY_FUNCTION__, __FILE__, __LINE__, MACRO, ARGS,   \
                       _gtry_stash_->GetCulprit(), nullptr)) BUILDER_PATCH;    \
    nullptr;                                                                   \
  }))->GetValue()

#define MERROR_INTERNAL_MTRY_1(MACRO, ARGS, DOMAIN, EXPR) \
  MERROR_INTERNAL_MTRY_IMPL(MACRO, ARGS, __COUNTER__, DOMAIN, EXPR, )
#define MERROR_INTERNAL_MTRY_2(MACRO, ARGS, DOMAIN, EXPR, builder_patch) \
  MERROR_INTERNAL_MTRY_IMPL(MACRO, ARGS, __COUNTER__, DOMAIN, EXPR,      \
                            MERROR_INTERNAL_HANDLE_UNDERSCORE(builder_patch))
#define MERROR_INTERNAL_MTRY_3(MACRO, ARGS, DOMAIN, A1, A2, A3) \
  _Pragma("GCC error \"MTRY() can't be called with 3 arguments\"")
#define MERROR_INTERNAL_MTRY_4(MACRO, ARGS, DOMAIN, A1, A2, A3, A4) \
  _Pragma("GCC error \"MTRY() can't be called with 4 arguments\"")
#define MERROR_INTERNAL_MTRY_5(MACRO, ARGS, DOMAIN, A1, A2, A3, A4, A5) \
  _Pragma("GCC error \"MTRY() can't be called with 5 arguments\"")
#define MERROR_INTERNAL_MTRY_6(MACRO, ARGS, DOMAIN, A1, A2, A3, A4, A5, A6) \
  _Pragma("GCC error \"MTRY() can't be called with 6 arguments\"")

// If `decltype(EXPR)` is `void`, the second argument of `MakeMTryState()` is
// `merror::Void()`. Otherwise it's `EXPR`.
#define MERROR_INTERNAL_MTRY_STATE(DOMAIN, EXPR, KEY)                     \
  ::merror::internal_macros::MakeMTryState<KEY>(                          \
      (DOMAIN),                                                           \
      ((EXPR),                                                            \
       ::merror::internal_macros::Expr<::merror::Void>{::merror::Void()}) \
          .get())

namespace merror {
namespace internal_macros {

template <int Key, class Domain, class Expr>
class MTryState {
  using Acceptor =
      decltype(std::declval<const Domain&>().Try(std::declval<Ref<Expr&&>>()));

 public:
  // When MTRY() fails, an instance of Stash is passed through tls_map.
  // Since tls_map isn't free, MTRY() failures have overhead. In order to
  // achieve zero overhead on the successful path of MTRY(), it's crucial to
  // avoid pinning the error domain to memory and allow the compiler to
  // optimize it away. That's why Stash is copying the domain (unless it's
  // an lvalue reference) instead of storing a reference.
  struct Stash {
    Stash(Domain&& domain, Acceptor&& acceptor)
        : domain(std::forward<Domain>(domain)),
          acceptor(std::forward<Acceptor>(acceptor)) {}

    const Domain& GetDomain() { return domain; }
    decltype(std::declval<Acceptor>().GetCulprit()) GetCulprit() {
      return std::forward<Acceptor>(acceptor).GetCulprit();
    }

    Domain domain;      // either lvalue ref or value; never rvalue ref
    Acceptor acceptor;  // can be anything: value, lvalue ref, rvalue ref
  };

  // Implementation note: assume that the acceptor doesn't alias any temporaries
  // that might be created (and destroyed) during the call to `Try()`.
  MTryState(Domain&& domain, Expr&& expr)
      : domain_(std::forward<Domain>(domain)),
        acceptor_(Const(domain_).Try(
            internal_macros::MakeRef(std::forward<Expr>(expr)))),
        stash_(nullptr) {}

  // Non-copyable and non-movable.
  MTryState(const MTryState&) = delete;
  MTryState& operator=(const MTryState&) = delete;

  ~MTryState() {
    if (MERROR_PREDICT_FALSE(stash_ != nullptr))
      ::merror::internal::tls_map::Remove<Stash>(Key);
  }

  MTryState* GetSelfOrNull() {
    if (MERROR_PREDICT_FALSE(acceptor_.IsError())) {
      stash_ = ::merror::internal::tls_map::Put<Stash>(
          Key, std::forward<Domain>(domain_),
          std::forward<Acceptor>(acceptor_));
      return nullptr;
    }
    return this;
  }

  decltype(std::declval<Acceptor>().GetValue()) GetValue() {
    return std::forward<Acceptor>(acceptor_).GetValue();
  }

 private:
  static_assert(!std::is_rvalue_reference<Domain>(), "");
  Domain&& domain_;
  Acceptor acceptor_;
  Stash* stash_;
};

template <int Key, class Domain, class Expr>
MTryState<Key, Domain, Expr> MakeMTryState(Domain&& domain, Expr&& expr) {
  return {std::forward<Domain>(domain), std::forward<Expr>(expr)};
}

}  // namespace internal_macros
}  // namespace merror

#endif  // MERROR_5EDA97_MACROS_H_
