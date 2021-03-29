MError
C++ Macro Error Handling Library

# Introduction
MError is a library for error handling in C++ without exceptions. It requires C++17 and only works for gcc or clang compilers.

Before MError, I was adamant to the opinion of avoiding macros for error handling. My TL at the time, would counter indicating that those macros served as a signal in the code to indicate error-handling code as opposed to normal expected flow.

MError was conceived by Roman Perepelitsa to tackle the explosion of error handling macros in our codebase. MError provides an alternative that replaces all of macros with 2 control-flow and 2 auxiliary macros. Furthermore, it provides a declarative, scoped mechanism to specify error-handling logic and great interoperability between 'error' types such as bool, std::optional and absl::Status.

I (ivserna) reluctantly started to use MError after I already contributed to its codebase. I am convinced MError or a similar solution will become the default error-handling and status propagation code in C++.

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
<!--
Extra functionality can be added to an error domain via extensions. For example, we could add the ability to increment a counter on error for monitoring. For this, we would need to implement a Monitoring extension and augment gerror::Domain with it. The usage would look as follows:

// The Monitoring extension adds method `Inc(string_view)` to the error domain.
// Unless `Inc` is called, the behavior of the domain is unchanged.
constexpr GERROR_DOMAIN(gerror::Domain).With(Monitoring);

Status VerifyUser(const User& user) {
  // On error within this function, increment "invalid-user" counter.
  GERROR_DOMAIN().Inc("invalid-user");
  // On error, increment "missing-gaia" counter.
  GVERIFY(user.has_gaia()).Inc("missing-gaia");
  GVERIFY(user.gaia() >= 0).Inc("negative-gaia");
  return OkStatus();
}

gerror::Default is in fact a collection of some two dozen extensions, each responsible for one aspect of the error domain's behavior: DescriptionBuilder enables streaming of error description with operator <<, AcceptOptional enables arguments of type optional<T>, etc.

Performance
Portability
Reference
Symbols
Dependencies
Macros
GERROR_DOMAIN
GERROR
GVERIFY
Expression Decomposition
GTRY
GASSIGN
Error Domain
GErrorDomain
Builder Patch
Status Error Code
Domain Patch
Error Types
Verify Errors
Try Errors
Return Errors
Side Errors
Gotchas
Return Error Type
Void Return Type
Automatic Return Type
GTRY in Expressions
GASSIGN and temporaries
Advanced Features
Introduction


The GTRY() conundrum and

GTRY() uses three GCC and clang compiler extensions: __COUNTER__, statement expressions and the elvis operator (x?:y). It doesn't work with other compilers.

Statement expressions are a non-standard way to create returning expressions, alternatives include both coroutines -- possibly standardized on C++20 -- and throw expressions.

Syntactically, none of those approaches really satisfy me. After successfully adapting coroutines to provide a GTRY() equivalent, the resulting coroutine solution feels backwards. It is my opinion that the language should provide a returning expression syntax first  --one that doesn't use exceptions-- and that coroutines and other features should be built on top of it.

Users with unusual requirements can assemble their own error domains based on the subset of the official extensions that they find useful and replace others with their own implementations. This flexibility is key to arresting the proliferation of error handling macros: if everything can be done by tweaking an error domain with plain C++, there is no need to define new macros.

However, extension implementation is not easy, and as such it is currently discouraged for GError users to create extensions without informing/including gerror-dev@, I hope that C++14 will significantly simplify error domain extensions.

Within google3 the standard error type is Status. When working with legacy or third party code it's occasionally necessary to deal with other kinds of error types and error reporting mechanisms. With a suitable extension, GError can translate foreign error-handling policy to the language of Status, thus restricting and encapsulating the use of custom error policy to a narrow scope.

StatusOr<size_t> CurrentThreadStackSize() {
  // The PThread extension instructs the error domain to handle arguments of
  // type int as pthread error codes.
  GERROR_DOMAIN(gerror::Default).With(PThread);

  pthread_attr_t attr = {};
  GVERIFY(pthread_getattr_np(pthread_self(), &attr));
  size_t stack_size = 0;
  GVERIFY(pthread_attr_getstacksize(&attr, &stack_size));
  GVERIFY(pthread_attr_destroy(&attr));
  return stack_size;
}


TODO(iserna): Indicate which extensions that will not be included in the default domain are in the pipeline.
Performance
Error path has negligible overhead compared to the equivalent hand-written code. In the case of GTRY(), the slowest of all macros, the overhead is around 1.5ns. Successful paths of GVERIFY() and GTRY() have no overhead.
Portability
GTRY() uses three GCC and clang compiler extensions: __COUNTER__, statement expressions and the elvis operator (x?:y). It doesn't work with other compilers. The rest of the library uses standard C++11.
Reference
Symbols
GERROR_DOMAIN
GERROR
GVERIFY
GTRY
GASSIGN
gerror::Default
Dependencies
Build target: //util/gerror.
Include header: util/gerror/gerror.h.
Macros
GERROR_DOMAIN
GERROR_DOMAIN() defines an error domain in the current scope. Other GError macros such as GVERIFY() and GTRY() rely on the error domain to perform their duties.

GERROR_DOMAIN(Base) expands to const auto GErrorDomain = (Base()), which allows you to apply a policy patch by pasting it after the macro. It can be optionally preceded by constexpr.

constexpr GERROR_DOMAIN(gerror::Default).Log(ERROR);


GERROR_DOMAIN() without arguments expands to two declarations:

const auto& _unique_variable_ = GErrorDomain;
const auto GErrorDomain = _unique_variable_


You can use this form to patch an error domain that you already have in scope. For example, suppose you have GErrorDomain in the file scope and want to use GErrorDomain.Log(ERROR) as your error domain in one of the functions.

// At the top of foo.cc.
//
// Within foo.cc, the error domain is
// gerror::Default().DefaultErrorCode(UNKNOWN).
constexpr GERROR_DOMAIN(gerror::Default).DefaultErrorCode(UNKNOWN);

Status Foo(int n) {
  // Within `Foo()`, the error domain is
  // gerror::Default().DefaultErrorCode(UNKNOWN).Log(ERROR).
  GERROR_DOMAIN().Log(ERROR);
  ...
}

GERROR
GERROR() creates an error, which can be returned or passed around.

Status F() {
  return GERROR().ErrorCode(UNIMPLEMENTED) << "Sorry";
}


Simplified expansion:
Status F() {
  return GErrorDomain().GetErrorBuilder(...).ErrorCode(UNIMPLEMENTED) << "Sorry";
}

GVERIFY
GVERIFY(expr) evaluates its argument and returns if it's an error.

Status F(int n) {
  GVERIFY(n > 0).ErrorCode(INVALID_ARGUMENT) << "Nope";
  ...
}


Simplified expansion:
Status F(int n) {
  const auto& domain = GErrorDomain();
  if (domain.IsVerifyError(n > 0))
    return domain.GetErrorBuilder(...).ErrorCode(INVALID_ARGUMENT) << "Nope";
  ...
}

Expression Decomposition
If the argument of GVERIFY() is of the form x relop y where relop is a relational operator (==, !=, <, >, <= or >=), the values of x and y get captured and included in the error description. This is the reason why there is no GVERIFY_EQ(), GVERIFY_GE(), etc.

Status F(int n, int m) {
  // Status description:
  //    foo/bar/baz.cc:195: GVERIFY(n + 1 < m)
  //    Same as: GVERIFY(42 < 24)
  GVERIFY(n + 1 < m);
  ...
}


Parenthesize the expression to disable its decomposition
// Disable expression decomposition.
// The values of n + 1 and m won't be captured.
GVERIFY((n + 1 < m));

GTRY
GTRY(expr) is an expression that unwraps the expression argument if the argument is not an error, otherwise it returns from the current function with the appropriate error type.

StatusOr<int> N();

StatusOr<string> S() {
  int n = GTRY(N());
  return StrCat(n);
}


Simplified expansion:

StatusOr<int> N();

StatusOr<string> S() {
  int n = ({
    const auto& domain = GErrorDomain();
    auto&& input = N();
    if (domain.IsTryError(input)) return domain.GetErrorBuilder(...);
    domain.GetValue(input);
  });
  return StrCat(n);
}


The complete expansion is obviously more complicated to be able to deal with references and temporaries in the argument expression. GTRY(expr) works every time expr should work, i.e. all temporaries last until the end of the full expression which contains GTRY().
Error Domain
The macros rely on an error domain for figuring out whether a given value is an error, for building errors for returning, etc. They find the current error domain by evaluating GErrorDomain().
GErrorDomain
There is no GErrorDomain in the global namespace, so you have to explicitly bring in the error domain you want to use into current scope before you can use GError macros. In order to use the default error domain, put the following at the top of the .cc file right after the using-declarations together with other file-level constants.

constexpr GERROR_DOMAIN(gerror::Default);


You can also define GErrorDomain in block scope.
Builder Patch
An error builder is created whenever an error is detected. It's responsible for constructing the return value. A builder patch is a sequence of function and operator calls on an error builder. It allows you to customize the return value and trigger side effects. The builder patch is evaluated only on the error path.

GERROR() ${builder-patch};
GVERIFY(expr) ${builder-patch};
GTRY(expr, _ ${builder-patch});
GTRY(expr, ${builder-patch});  // only if ${builder-patch} starts with Foo(...)


GERROR() << "blah";
GERROR().ErrorCode(INTERNAL) << "blah";

GVERIFY(n > 0) << "blah";
GVERIFY(n > 0).ErrorCode(INTERNAL) << "blah";

GTRY(expr, _ << "blah");
GTRY(expr, ErrorCode(INTERNAL) << "blah");

// Technically valid but uncommon.
GTRY(expr, _.ErrorCode() << "blah");


Some of the builder patches currently supported by gerror::Default:
<< "blah" << 42 << "blah": fill error description.
ErrorCode(INTERNAL): set error code.
DefaultErrorCode(UNKNOWN): set default error code.
Return(val): return the specified value.
Return<T>(): return an error of the specified type.
Return() or Return<void>(): return void.
Tee(ptr_to_error): fill the error (can be RPC, Task, Status, etc.). 
Tee(callable): If the callable takes no arguments or it can be called with the generated culprit. functor will be called on error.
Log(ERROR): log on error.
Status Error Code
When returning Status or StatusOr<T> GError macros need to figure out which error code to use. They apply the following algorithm:

if ErrorCode() specified {
  use ErrorCode()
}
if input has error code {
  use input error code
}
if DefaultErrorCode() specified {
  use DefaultErrorCode()
}
compile error


Here's an example:
constexpr GERROR_DOMAIN(gerror::Default).DefaultErrorCode(d);
GVERIFY(status);  // it's `status.code()`
GVERIFY(status).ErrorCode(e);  // it's `e`
GVERIFY(n > 0);  // it's d


If you call ErrorCode() or DefaultErrorCode() multiple times, the last call wins. Builder patches are applied after domain patches, which means that ErrorCode() on builder overrides any prior ErrorCode() calls on the domain.
Domain Patch

A domain is the result of evaluation of GErrorDomain(). It's responsible for classifying input values as errors or non-errors and for extracting values from wrappers such as StatusOr<T>. Each execution of a GError macro creates its own domain. 

You can apply a domain patch and save the result in a local GErrorDomain. This way all macros using the domain will have their behavior changed.

constexpr GERROR_DOMAIN(gerror::Default) ${policy-patch};


constexpr GERROR_DOMAIN(gerror::Default).CLib();
GVERIFY(nice(39));  // CLib() is implicit


Domain patches can't be applied to error builders. It's too late to tell the builder how to distinguish between error and non-error inputs -- the input has already been tested and found erroneous at that point.

// Compile error: CLib() is a domain only patch and can't be applied to error 
// builders.
GVERIFY(nice(39)).CLib();


Conversely, builder patches can be applied in GERROR_DOMAIN(). This can be a convenient alternative to specifying the same side effects on every macro invocation.

constexpr GERROR_DOMAIN(gerror::Default)
    .DefaultErrorCode(UNKNOWN)
    .Log(ERROR);

Status F(int n, int m) {
  // DefaultErrorCode(UNKNOWN) and Log(ERROR) are implicit.
  GVERIFY(n > 0);
  GVERIFY(m < 100);
  ...
}


Error Types
gerror::Default error domain supports several error types.
Verify Errors
GVERIFY() with the default error domain currently supports inputs of the following types:
bool
T*
Status
lvalue try errors (VerifyViaTry)

Try Errors
GTRY() with the default error domain currently supports inputs of the following types:
T*
smart pointers
optional<T>
StatusOr<T>

Return Errors
All GError macros with the default error domain can return values of the following types:
bool
T*
smart pointers
void
optional<T>
Status
StatusOr
Side Errors
All GError macros with the default error domain can currently populate side errors (specified via Tee(ptr) builder patch) of the following types:
all return error types

Gotchas
Return Error Type
In most cases, GError macros can infer the type of the error they should return. There are two exceptions: functions returning void and functions with automatic return type deduction.
Void Return Type
GError macros won't compile out of the box in a void returning function. You have to use Return() or Return<void>() builder patch.

void F(int n) {
  // Unless n > 0, return.
  GVERIFY(n > 0).Return();
  ...
}


Use a local GErrorDomain to avoid repeating yourself.

void F(int n) {
  constexpr GERROR_DOMAIN(gerror::Default).Return();
  GVERIFY(n > 0);
  GVERIFY(n < 100);
  ...
}

Automatic Return Type
GError macros won't compile out of the box in a function with automatic return type (e.g., a lambda without the explicit return type). The best solution is to specify the return type.

[](int n) -> Status {
  GVERIFY(n > 0);
  ...
};


However, if the result type is void, it won't help. See Void Return Type above for solution.

An alternative (and not recommended) solution is to use Return(value) or Return<Type>() builder patch.

[](int n) {
  GVERIFY(n > 0).Return<Status>();
  GVERIFY(n > 0).Return(InternalError("should not happen"));
  ...
};

GTRY in Expressions
When using GTRY() in expressions, remember that the order of argument evaluation is unspecified in C++. If you aren't being careful, it's easy to introduce memory leaks and surprising effects that get manifested only when errors happen. Consider the following example:

StatusOr<T> Make();

void Consume(const T&, U* p) { delete p; }

Status Test() {
  // Don't do this: `U` may leak if Make() fails.
  Consume(GTRY(Make()), new U);
  ...
}


new U can get evaluated before GTRY(). If the latter returns, U leaks. Using std::unique_ptr doesn't always fix the problem.

StatusOr<T> Make();

void Consume(const T&, std::unique_ptr<U> p);

Status Test() {
  // Don't do this: `U` may leak if Make() fails.
  Consume(GTRY(Make()), std::unique_ptr<U>(new U));
  ...
}


The compiler is allowed to evaluate new U first, followed by GTRY() and finally call the constructor of std::unique_ptr. If GTRY() returns, U once again leaks. Switching to gtl::MakeUnique<U>() will fix the issue here. However, simplifying the expression by introducing a temporary variable might be better.

Here's another example that doesn't exhibit a memory leak but can still have surprising behaviour when an error occurs.

StatusOr<T> Make();

Status Test(map<string, T>* m) {
  (*m)["abc"] = GTRY(Make());
  ...
}


If Make() returns an error, it's unspecified whether (*m)["abc"] gets evaluated. If it does, the map may end up with a default-constructed value under key "abc".

If you've used C++ with exceptions, these problems should be familiar. You should treat GTRY(expr) the way you would treat a potentially throwing expression.

Advanced Features
GError offers a flexible framework for customizing the behavior of the three macros by defining your own error domain in regular C++. gerror::Default is built on top of the same public API that other google3 packages can use. Status support, description streaming, logging, even the low-level Return() and Tee() patches are implemented in gerror::Default without special support from the macros. 
If gerror::Default doesn't match your needs perfectly, you can either define your own extensions and overlay them, or even build an error domain from scratch.

An error domain allows you to customize the following:
Return error types.
Input error types.
Value unwrapping.
Error side effects.
Error descriptions.
Logging.

The issue of extending and creating error domains is outside the scope of this document, and it is discussed in its own document.

TODO(iserna): The extension API is not documented yet. When stable provide a link to it.

--->

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
