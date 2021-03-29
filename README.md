MError
C++ Macro Error Handling Library

# Introduction
MError is a library for error handling in C++ without exceptions. It requires C++17 and only works for gcc or clang compilers.

Before MError, I was adamant to the opinion of avoiding macros for error handling. My TL at the time, would counter indicating that those macros served as a signal in the code to indicate error-handling code as opposed to normal expected flow.

MError was conceived by Roman Perepelitsa to tackle the explosion of error handling macros in our codebase. MError provides an alternative that replaces all of macros with 2 control-flow and 2 auxiliary macros. Furthermore, it provides a declarative, scoped mechanism to specify error-handling logic and great interoperability between 'error' types such as bool, std::optional and absl::Status.

I reluctantly started to use MError after I already contributed to its codebase. By now, I am convinced MError greatly simplifies error-handling and status propagation code in C++.

# Motivational Example

```
constexpr MERROR_DOMAIN(merror::Default).CerrLog();  

StatusOr<int> Div(int a, int b) {
  MVERIFY(b != 0).ErrorCode(absl::StatusCode::kInvalidArgument) << "Cannot divide by zero";
  return a / b;
}

StatusOr<int> Mod(int a, int b) {
  return a - b * MTRY(Div(a, b));
}
```

In this example, StatusOr<int> is used to possibly propagate errors associated with zero-division. The use of `MVERIFY()` and `MTRY()` macros makes it clear which sections of the code are error-handling logic and which ones are 'expected' logic. 

MError strives to be declarative in nature, with most of the error-handling logic being applied in block sections throughout the code, without any need to repeat that logic in each line in which error handling is needed.

In the example, we 'patch' the error domain in `MERROR_DOMAIN()` to indicate we want to log a warning every time an error is detected. We also 'patch' the error generation in `MVERIFY()` to include more information and to specify how a boolean transforms into a Status. 

# Overview

The statement `MVERIFY(expr)` returns an error if expr is an error. If the argument is of the form `x relop y` where `relop` is a relational operator `(==, !=, <, >, <= or >=)`, the values of operands `x` and `y` get captured and included in the error description. This is the reason why there is no `MVERIFY_EQ`, `MVERIFY_GE`, etc.

```
Status CookBreakfast(int hunger_level) {
  GVERIFY(hunger_level >= 0).ErrorCode(absl::StatusCode::kInvalidArgument) << "Already well-fed";
  while (hunger_level--) {
    GVERIFY(MakeEggs());
  }
  return OkStatus();
}
```

The expression `MTRY(expr)` returns an error if `expr` is an error, and evaluates to the extracted value of `expr` otherwise. The argument should be a value wrapper with an extra error state: `StatusOr<T>`, `unique_ptr<T>`, `optional<T>`, etc.

```
StatusOr<string> ReadFile(string_view filepath);

Status PrintFile(string_view filepath) {
  string content = MTRY(ReadFile(filepath));
  cout << content << endl;
  return OkStatus();
}
```

The expression `MERROR()` unconditionally expands to an error builder. It can be used to return an error unconditionally or to pass it around.

```
Status MakeSandwich() {
  return MERROR(absl::StatusCode::kPermissionDenied) << "Eat vegetables";
}

void MakeCoffeeAsync(function<void(Status)> done) {
  done(MERROR(absl::StatusCode::kNotImplemented) << "I'm a teapot");
}
```

MError macros don't have built-in knowledge of error types. They rely on an error domain to classify objects as errors, to extract values from wrappers such as `StatusOr<T>,` to build errors, and to trigger side effects (e.g., logging). 

For example, when `MVERIFY(a == b)` is used in a function returning Status, the macro first evaluates `a == b` and obtains an object of type `bool`. It then passes the `bool` to the error domain asking whether it's an error. If the error domain says yes (e.g., by classifying `false` as error), the macro then asks the domain to describe the error as an object of type `Status`, passing to the domain all potentially relevant local context such as `__FILE__` and `__LINE__`. Thus,  `MVERIFY()` is responsible for catching local context and for control flow, while the error domain is responsible for type-specific error-handling logic and side effects. The macro is fixed but the error domain can be extended by the users mainly through 'patching' and domain extensions.

'Patching' is what allows us to declare code that must run on every error. The ability to add behaviour to the error domain using C++ and the interoperability it provides between different error types is what really sets MError apart from other approaches.

MError macros find the error domain by its special name -- MErrorDomain -- via unqualified name lookup. There is no MErrorDomain in the global scope, so every file that uses MError macros must explicitly specify its error domain. MError defines merror::Default -- an error domain with all common error-handling features for common users, including Status and logging with optional throttling. All that's needed to take advantage of this functionality is to create a local MErrorDomain alias pointing to merror::Default.

```
constexpr auto MErrorDomain = merror::Default();
```

Such a definition can be placed either in namespace scope in a cc file, or in function scope. While it's certainly possible to define an error domain with plain C++ as shown above, the canonical way of doing it utilizes `MERROR_DOMAIN()`.

```
constexpr MERROR_DOMAIN(merror::Default);
```

The expansion is exactly the same as the macro-free code snippet above.

The behavior of an error domain can be customized by calling its methods. Since error domains are immutable, these methods return a modified copy of the original domain. The process of creating a new domain based on an existing domain is called 'patching'.

```
constexpr MERROR_DOMAIN(merror::Default)
    .DefaultErrorCode(absl::StatusCode::kUnkown)
    .CerrLog();
```

We've just declared an error domain by patching `merror::Default`. The patch says to send all errors to `std::cerr` and to use `kUnkown` as the error code if no other error code is provided (the default behavior of `merror::Domain` in this case is to issue a compile error).

It's often convenient to define one error domain for a cc file and then use a slightly different domain in one of the functions or even one of the code blocks. This can be achieved by using the second form of `MERROR_DOMAIN()`. While `MERROR_DOMAIN(domain)` uses `domain` as the base, `MERROR_DOMAIN()` without arguments uses the current error domain -- the MErrorDomain object from the closest parent scope.

```
constexpr MERROR_DOMAIN(merror::Default)
    .DefaultErrorCode(absl::StatusCode::kUnkown)
    .CerrLog();

Status LaunchMissiles() {
  // In this function, send all errors to std::cout.
  MERROR_DOMAIN().CoutLog();
  ...
}
```

The effective error domain in `LaunchMissiles()` is `merror::Default().DefaultErrorCode(absl::StatusCode::kUnkown).CerrLog().CoutLog()`. The `CoutLog()` patch overrides `CerrLog()` because it comes after it, so all errors get sent to `std::cout`.

A patch can also be applied locally within the scope of a single macro.

```
MVERIFY(a > b)
    .ErrorCode(absl::StatusCode::kInvalidArgument)
    .CerrLog();
```

A patch specified during an error domain definition gets evaluated eagerly. It says what should be done whenever an error is encountered. A patch specified in the scope of a macro is evaluated only when an error actually happens. Specifying patches in both places is equivalent to specifying all of them in a single place, except for potential differences in performance.

<!--
Thus, the following three functions have equivalent behavior.

Status MixedPatches(int n) {
  constexpr GERROR_DOMAIN(gerror::Default)
      .Log(WARNING);
  GVERIFY(n > 0)
      .ErrorCode(INVALID_ARGUMENT)
      .Log(ERROR);
  ...
}

Status DomainPatch(int n) {
  constexpr GERROR_DOMAIN(gerror::Default)
      .Log(WARNING)
      .ErrorCode(INVALID_ARGUMENT)
      .Log(ERROR);
  GVERIFY(n > 0);
  ...
}

Status MacroPatch(int n) {
  constexpr GERROR_DOMAIN(gerror::Default);
  GVERIFY(n > 0)
      .Log(WARNING)
      .ErrorCode(INVALID_ARGUMENT)
      .Log(ERROR);
  ...
}
-->

A patch can capture dynamic state, too.

```
void HandleRequest(Status* status, const Request* req, Response* resp, Closure* done) {
  // On return, call done->Run().
  AutoClosureRunner done_runner(done);
  // On error, set status and return.
  MERROR_DOMAIN().Tee(status).Return();

  Id id = MTRY(GetIdFromRequest(*req, &id));
  MVERIFY(FillResponseForId(id, resp));
}
```

# Source Code Headers

Every file containing source code must include copyright and license
information. This includes any JS/CSS files that you might be serving out to
browsers. (This is to help well-intentioned people avoid accidental copying that
doesn't comply with the license.)

Apache header:

    Copyright 2021 Google LLC

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        https://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
