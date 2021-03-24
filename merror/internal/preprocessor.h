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

#ifndef MERROR_5EDA97_INTERNAL_PREPROCESSOR_H_
#define MERROR_5EDA97_INTERNAL_PREPROCESSOR_H_

// Evaluates the arguments and then stringizes them.
#define MERROR_INTERNAL_STRINGIZE(...) MERROR_INTERNAL_STRINGIZE_I(__VA_ARGS__)

// Evaluates to the number of arguments after expansion.
//
//   #define PAIR x, y
//
//   MERROR_INTERNAL_NARG() => 1
//   MERROR_INTERNAL_NARG(x) => 1
//   MERROR_INTERNAL_NARG(x, y) => 2
//   MERROR_INTERNAL_NARG(PAIR) => 2
//
// Requires: the number of arguments after expansion is at most 9.
#define MERROR_INTERNAL_NARG(...) \
  MERROR_INTERNAL_10TH(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

// Evaluates the arguments and then concatenates the first with the rest.
//
//   #define PAIR x, y
//
//   MERROR_INTERNAL_VCAT() =>
//   MERROR_INTERNAL_VCAT(x) => x
//   MERROR_INTERNAL_VCAT(x, y, z) => xy, z
//   MERROR_INTERNAL_VCAT(PAIR) => xy
#define MERROR_INTERNAL_VCAT(...) MERROR_INTERNAL_VCAT_I(__VA_ARGS__)

// Expands to 1 if the first token of the first argument is an underscore,
// otherwise to 0.
// Requires: the first argument starts with [a-zA-Z0-9_] or is empty.
#define MERROR_INTERNAL_STARTS_WITH_UNDERSCORE_TOKEN(...) \
  MERROR_INTERNAL_IS_BEGIN_PARENS(                        \
      MERROR_INTERNAL_VCAT(MERROR_INTERNAL_DETECT_UNDERSCORE, __VA_ARGS__))

// If the arguments after expansion have no tokens, evaluates to `1`. Otherwise
// evaluates to `0`.
//
// There is one case when it generates a compile error: if the argument is a
// non-variadic macro with more than one parameter.
//
//   #define M(a, b)  // it doesn't matter what it expands to
//
//   // Expected: expands to `0`.
//   // Actual: compile error.
//   MERROR_INTERNAL_IS_EMPTY(M)
#define MERROR_INTERNAL_IS_EMPTY(...)                               \
  MERROR_INTERNAL_IS_EMPTY_I(                                       \
      MERROR_INTERNAL_HAS_COMMA(__VA_ARGS__),                       \
      MERROR_INTERNAL_HAS_COMMA(MERROR_INTERNAL_COMMA __VA_ARGS__), \
      MERROR_INTERNAL_HAS_COMMA(__VA_ARGS__()),                     \
      MERROR_INTERNAL_HAS_COMMA(MERROR_INTERNAL_COMMA __VA_ARGS__()))

// If `COND` is `1` after expansion, evaluates to `THEN`. If `COND` is `0` after
// expansion, evaluates to `ELSE`. Otherwise undefined behavior.
//
// `THEN` and `ELSE` must not contain unparenthesized commas.
#define MERROR_INTERNAL_IF(COND, THEN, ELSE) \
  MERROR_INTERNAL_VCAT(MERROR_INTERNAL_IF_, COND)(THEN, ELSE)

///////////////////////////////////////////////////////////////////////////////
//                   IMPLEMENTATION DETAILS FOLLOW                           //
//                    DO NOT USE FROM OTHER FILES                            //
///////////////////////////////////////////////////////////////////////////////

// Expands to 1 if the first argument starts with something in parentheses,
// otherwise to 0.
// Requires: the first argument starts with something in parentheses,
// [a-zA-Z0-9_], or is empty.
#define MERROR_INTERNAL_IS_BEGIN_PARENS(...)               \
  MERROR_INTERNAL_FIRST(                                   \
      MERROR_INTERNAL_VCAT(MERROR_INTERNAL_IS_VARIADIC_R_, \
                           MERROR_INTERNAL_IS_VARIADIC_C __VA_ARGS__))

// Helpers for the macros defined above.
#define MERROR_INTERNAL_STRINGIZE_I(...) #__VA_ARGS__
#define MERROR_INTERNAL_10TH(_1, _2, _3, _4, _5, _6, _7, _8, _9, X, ...) X
#define MERROR_INTERNAL_DETECT_UNDERSCORE_ ()
#define MERROR_INTERNAL_FIRST(...) MERROR_INTERNAL_FIRST_I(__VA_ARGS__)
#define MERROR_INTERNAL_FIRST_I(a, ...) a
#define MERROR_INTERNAL_VCAT_I(a, ...) a##__VA_ARGS__
#define MERROR_INTERNAL_IS_VARIADIC_C(...) 1
#define MERROR_INTERNAL_IS_VARIADIC_R_1 1,
#define MERROR_INTERNAL_IS_VARIADIC_R_MERROR_INTERNAL_IS_VARIADIC_C 0,
#define MERROR_INTERNAL_IF_1(THEN, ELSE) THEN
#define MERROR_INTERNAL_IF_0(THEN, ELSE) ELSE
#define MERROR_INTERNAL_COMMA(...) ,
#define MERROR_INTERNAL_IS_EMPTY_CASE_0001 ,
#define MERROR_INTERNAL_CAT5(A, B, C, D, E) A##B##C##D##E
#define MERROR_INTERNAL_HAS_COMMA(...) \
  MERROR_INTERNAL_10TH(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 0)
#define MERROR_INTERNAL_IS_EMPTY_I(A, B, C, D) \
  MERROR_INTERNAL_HAS_COMMA(                   \
      MERROR_INTERNAL_CAT5(MERROR_INTERNAL_IS_EMPTY_CASE_, A, B, C, D))

#endif  // MERROR_5EDA97_INTERNAL_PREPROCESSOR_H_
