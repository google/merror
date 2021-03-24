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
// Basic types used by the merror macros.

#ifndef MERROR_5EDA97_TYPES_H_
#define MERROR_5EDA97_TYPES_H_

#include <cstdint>
#include <iosfwd>
#include <string>
#include <type_traits>

namespace merror {

// An empty type that represents void in contexts where the real void is hard
// to use.
struct Void {};

inline bool operator==(Void, Void) { return true; }
inline bool operator!=(Void, Void) { return false; }

std::ostream& operator<<(std::ostream& strm, Void v);

// Macro kind. There is a one-to-many relationship between kinds and the actual
// error handling macros (there may be more than one macro with the kind kTry).
enum class Macro {
  // Error is created unconditionally. The prototypical example is MERROR().
  kError,
  // Depending on the expression value, an error may be raised. The prototypical
  // example is MVERIFY().
  kVerify,
  // Depending on the expression value, either an error is raised or a value is
  // produced. The prototypical example is MTRY().
  kTry,
};

// C++ relational operator.
enum class RelationalOperator {
  kEq,  // ==
  kNe,  // !=
  kLt,  // <
  kGt,  // >
  kLe,  // <=
  kGe,  // >=
};

std::ostream& operator<<(std::ostream& strm, RelationalOperator op);

// C++ expression involving a relational operator.
struct RelationalExpression {
  // Left operand.
  std::string left;
  RelationalOperator op;
  // Right operand.
  std::string right;
};

std::ostream& operator<<(std::ostream& strm, const RelationalExpression& expr);

// Reference wrapper similar to std::reference_wrapper.
//
// `T` is not a reference and not cv-qualified.
// `R` is is an lvalue or rvalue reference to `[const] [volatile] T`.
//
// `MVERIFY(expr)` and `MTRY(expr)` wrap `expr` in `Ref` and pass it to
// `Verify()` or `Try()` on error domain respectively. Here's how `Verify()`
// can handle expressions of type `MyError`.
//
//   // Handles lvalue and rvalue references to `[const] [volatile] MyError`.
//   template <class R>
//   Acceptor Verify(Ref<R, MyError> val);
//
//   // Handles only lvalue references to `[const] [volatile] MyError`.
//   template <class R>
//   Acceptor Verify(Ref<R&, MyError> val);
//
//   // Handles only rvalue references to `[const] [volatile] MyError`.
//   template <class R>
//   Acceptor Verify(Ref<R&&, MyError> val);
template <class R,
          class T = typename std::remove_cv<
              typename std::remove_reference<R>::type>::type>
class Ref {
 public:
  static_assert(std::is_reference<R>::value, "");
  static_assert(std::is_same<Ref, Ref<R>>::value, "");

  using reference = R;
  using value_type = T;

  Ref(R&& ref) : ref_(std::forward<R>(ref)) {}  // NOLINT
  R& Get() const { return ref_; }
  R&& Forward() const { return std::forward<R>(ref_); }

 private:
  R&& ref_;
};

// If certain methods of an error domain aren't public but have to be called by
// merror macros, the domain must declare `MErrorAccess<T>` a friend. Currently
// only `BuildError()` is invoked by the macros with the privileges of
// MErrorAccess.
//
// Example:
//
//   class MyErrorBuilder {
//    private:
//     // The merror macros need to call this method but it's private.
//     Status BuildError();
//     // Grant merror macros friend access.
//     template <class T>
//     friend struct merror::MErrorAccess;
//   };
template <class T>
struct MErrorAccess;

namespace internal {

template <Macro M, class Culprit>
struct Context;

// Trivial generator for `Context` defined below. Its purpose is deduce
// `Culprit` and to allow macros to construct contexts.
template <Macro M, class Culprit>
Context<M, Culprit&&> MakeContext(uintptr_t location_id, const char* function,
                                  const char* file, int line,
                                  const char* macro_str, const char* args_str,
                                  Culprit&& culprit,
                                  RelationalExpression* rel_expr) {
  return Context<M, Culprit&&>(location_id, function, file, line, macro_str,
                               args_str, std::forward<Culprit>(culprit),
                               rel_expr);
}

// Error context constructed by merror macros and passed to `GetErrorBuilder()`.
// The name of this type is internal and shouldn't be spelled by the users. The
// list of template parameters and data members may be extended in the future.
template <Macro M, class CulpritT>
struct Context {
  static_assert(std::is_reference<CulpritT>::value, "");

  using Culprit = CulpritT&&;

  // Kind of the macro that detected an error. The actual macro name is
  // specified in macro_str.
  static constexpr Macro macro = M;

  // Unique non-zero identifier of the macro expansion. Unlike the {file, line}
  // pair, location ID correctly distinguishes between merror macros expanded on
  // the same line. It is stable across translation units for macros used in
  // header files.
  //
  // Location ID isn't stable across binaries or even multiple runs of the same
  // binary.
  uintptr_t location_id;

  // __PRETTY_FUNCTION__. Not null, not empty, infinite lifetime.
  const char* function;
  // __FILE__. Not null, not empty, infinite lifetime.
  const char* file;
  // __LINE__. Positive.
  int line;
  // The name of the macro as it is spelled in the source code. For example,
  // `MVERIFY` or `MTRY`. Not null, not empty, infinite lifetime.
  const char* macro_str;
  // Arguments of the macro as they are spelled in the source code. For example,
  // `ReadFileToString(file, &content, DefaultOptions())`. Not null, may be
  // empty (e.g., for `MERROR`), infinite lifetime.
  const char* args_str;

  Culprit&& culprit;

  // In `Context<kMVerify>` with a relational expression (e.g., `x > 0`) and an
  // error domain that supports expression stringification, `rel_expr` points to
  // a temporary object that gets destroyed after the macro is fully evaluated.
  // In all other cases rel_expr is null.
  RelationalExpression* rel_expr;

 private:
  explicit Context(uintptr_t location_id, const char* function,
                   const char* file, int line, const char* macro_str,
                   const char* args_str, Culprit&& culprit,
                   RelationalExpression* rel_expr)
      : location_id(location_id),
        function(function),
        file(file),
        line(line),
        macro_str(macro_str),
        args_str(args_str),
        culprit(std::forward<Culprit>(culprit)),
        rel_expr(rel_expr) {}

  template <Macro X, class Y>
  friend Context<X, Y&&> MakeContext(uintptr_t location_id,
                                     const char* function, const char* file,
                                     int line, const char* macro_str,
                                     const char* args_str, Y&& culprit,
                                     RelationalExpression* rel_expr);
};

template <Macro M, class Culprit>
constexpr Macro Context<M, Culprit>::macro;

}  // namespace internal

}  // namespace merror

#endif  // MERROR_5EDA97_TYPES_H_
