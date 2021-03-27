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
// This library defines the `Return()` error domain extension.
//
// `BuildError()` is one of the required builder methods that an error domain
// must implement in order to be usable with the merror macros. The `Return()`
// extension implements this method and provides five policy- and
// builder-level knobs that control its behaviour.
//
// The knobs are `Return(val)`, `Return()`, `Return<Type>()`, `AutoReturn()` and
// `DeferReturn(f)`.
//
// Their semantics are described below. The examples use the default
// `merror::MErrorDomain()`, which contains `Return()`.
//
// `Return(val)` instructs the error domain to return `val` on error.
//
//   int Foo(int n) {
//     MERROR_DOMAIN(merror::Default);
//     MVERIFY(n > 0).Return(-1);
//     ...
//     return 0;
//   }
//
// When called without arguments, it'll return void on error.
//
//   void Foo(int n) {
//     MVERIFY(n > 0).Return();
//     ...
//   }
//
// `Return<Type>()` instructs the error domain to return
// `b.MakeError(ResultType<Type>(), b.context().culprit)` on error, where `b`
// is a reference to the const error builder. The builder must implement
// `Type MakeError(ResultType<Type>, const Culprit&) const`. Culprit is the
// source error, and the method should produce the target error of the specified
// type. `MakeError()` may be called multiple times on the same builder. It must
// not have side effects.
//
//   auto Foo = [](int n) {
//     static constexpr MERROR_DOMAIN(merror::Default).Return<bool>();
//     MVERIFY(n > 0);
//     ...
//     return true;
//   };
//
// `AutoReturn()` is equivalent to `Return<R>()`, where `R` is the declared
// result type of the function in which the merror macros are used. It is the
// default.
//
//   Status Foo(int n) {
//     // Don't need to call AutoReturn() because it's the default.
//     MERROR_DOMAIN(merror::Default);
//     MVERIFY(n > 0);
//     ...
//     return Status::OK;
//   };
//
// `AutoReturn()` can't be used in functions with automatic return type.
//
//   auto Bar = [](int n) {
//     MERROR_DOMAIN(merror::Default);
//     MVERIFY(n > 0);  // Compile error: can't deduce the result type.
//     ...
//   };
//
// `DeferReturn(f)` instruct the error domain to call the callable f to create
// the returned error, f must be fuctor with no parameters or with one
// parameter.
//
//   int Foo(int n) {
//     MERROR_DOMAIN(merror::Default);
//     MVERIFY(n > 0).DeferReturn([&] { return n; });
//     MVERIFY(n < 100).DeferReturn([] (util::Status status) {
//       return status.raw_code();
//     });
//     ...
//     return 0;
//   }
//
// If several knobs are used, the last one wins.
//
//   Status Foo(int n) {
//     MERROR_DOMAIN().Return(42);
//     // `AutoReturn()` overrides the previous `Return(42)`.
//     MVERIFY(n > 0).AutoReturn();
//     ...
//     return Status::OK;
//   };
//
// Right before returning from the current function, `ObserveRetVal(ret_val)` is
// called. `ret_val` is the const-qualified return value, or `Void` if returning
// `void`. `ObserveRetVal()` is the only method that may have side effects. See
// //util/merror/domain/observer.h for details.

#ifndef MERROR_5EDA97_DOMAIN_RETURN_H_
#define MERROR_5EDA97_DOMAIN_RETURN_H_

#include <utility>

#include "merror/domain/base.h"
#include "merror/domain/observer.h"
#include "merror/types.h"

namespace merror {

template <class T>
struct ResultType {
  using type = T;
};

namespace internal_return {

namespace impl {

template <class Builder, class Culprit>
struct AnyError {
  template <class Error>
  operator Error() const {
    return builder.MakeError(ResultType<Error>(), culprit);
  }
  const Builder& builder;
  const Culprit& culprit;
};

template <class Builder, class Culprit, class F>
auto Invoke(const Builder& builder, const Culprit& culprit, F&& f, int)
    -> decltype(std::forward<F>(f)(
        std::declval<AnyError<Builder, Culprit>>())) {
  return std::forward<F>(f)(AnyError<Builder, Culprit>{builder, culprit});
}

template <class Builder, class Culprit, class F>
auto Invoke(const Builder& builder, const Culprit& culprit, F&& f, unsigned)
    -> decltype(std::forward<F>(f)()) {
  return std::forward<F>(f)();
}

template <class Builder, class Culprit, class R, class T>
R MakeErrorByFnImpl(const Builder& b, const Culprit& culprit, R (*f)(T)) {
  return f(b.MakeError(ResultType<typename std::decay<T>::type>(), culprit));
}

template <class Builder, class Culprit, class R>
R MakeErrorByFnImpl(const Builder&, const Culprit&, R (*f)()) {
  return f();
}

template <class Builder, class Culprit, class F>
auto MakeErrorByFnImpl(const Builder& b, const Culprit& culprit, F&& f)
    -> decltype(Invoke(b, culprit, std::forward<F>(f), 0)) {
  return Invoke(b, culprit, std::forward<F>(f), 0);
}

template <class Builder, class Fn>
auto MakeErrorByFn(Builder* builder, Fn&& f)
    -> decltype(MakeErrorByFnImpl(*builder, builder->context().culprit,
                                  std::forward<Fn>(f))) {
  auto&& ret = MakeErrorByFnImpl(*builder, builder->context().culprit,
                                 std::forward<Fn>(f));
  const auto& cret = ret;
  builder->ObserveRetVal(cret);
  return std::forward<decltype(ret)>(ret);
}

}  // namespace impl

// An optional annotation whose value is one of the functors defined below. The
// default is ReturnAuto().
struct ReturnAnnotation {};

template <class R>
struct ReturnType {
  template <class Builder>
  R operator()(Builder* builder) const {
    const Builder* const_builder = builder;
    const auto& culprit = builder->context().culprit;
    auto res = const_builder->MakeError(ResultType<R>(), culprit);
    VerifyResultType<decltype(res)>();
    const auto& const_res = res;
    builder->ObserveRetVal(const_res);
    return res;
  }

 private:
  template <class Actual>
  static void VerifyResultType() {
    static_assert(std::is_same<Actual, R>(),
                  "MakeError() has incorrect result type");
  }
};

template <>
struct ReturnType<void> {
  template <class Builder>
  void operator()(Builder* builder) const {
    const Void v = {};
    builder->ObserveRetVal(v);
  }
};

template <class R>
struct ReturnValue {
  template <class Builder>
  R operator()(Builder* builder) {
    const R& const_ret = ret;
    builder->ObserveRetVal(const_ret);
    return std::move(ret);
  }
  template <class Builder>
  R operator()(Builder* builder) const {
    builder->ObserveRetVal(ret);
    return ret;
  }
  R ret;
};

template <class F>
struct ReturnByFn {
  F ret;
  template <class Builder>
  auto operator()(Builder* builder)
      -> decltype(impl::MakeErrorByFn(builder, std::move(ret))) {
    return impl::MakeErrorByFn(builder, std::move(ret));
  }
  template <class Builder>
  auto operator()(Builder* builder) const
      -> decltype(impl::MakeErrorByFn(builder, ret)) {
    return impl::MakeErrorByFn(builder, ret);
  }
};

// This specialization ensures that Policy<B>::Return<R>(R&&) gets disabled by
// SFINAE when R is void.
template <>
struct ReturnValue<void>;

template <class Base>
struct Builder;

struct ReturnAuto;

// This class is implicitly convertible to anything and explicitly convertible
// to nothing.
//
//   static_assert(std::is_convertible<ErrorFactory<B>, int>(), "");
//   static_assert(!std::is_constructible<int, ErrorFactory<B>>(), "");
//
// The second condition is relevant when converting to optional<T>. Without it
// the implicit conversion would be ambiguous.
template <class B>
class ErrorFactory {
 public:
  explicit ErrorFactory(B* builder) : builder_(*builder) {}

  template <class R>
  operator R() && {
    return ReturnType<R>()(&builder_);
  }

  // This overload makes explicit conversion to R ambiguous, which in turn makes
  // std::is_constructible<R, ErrorFactory>() false.
  template <class R, class = void>
  explicit operator R() && {
    return *this;
  }

 private:
  // These constructors are private to avoid returning ErrorFactory accidentally
  // from functions with `auto` result type.
  //
  //   auto F = [] {
  //     MERROR_DOMAIN(merror::Default);
  //     // Compile error: calling a private constructor of class ErrorFactory.
  //     MVERIFY(2 + 2 == 4);
  //     ...
  //   };
  ErrorFactory(const ErrorFactory&) = default;
  ErrorFactory(ErrorFactory&&) = default;

  // We friend all types that pass ErrorFactory through.
  friend struct ReturnAuto;
  template <class Base>
  friend struct Builder;
  template <class T>
  friend struct merror::MErrorAccess;

  B& builder_;
};

struct ReturnAuto {
  template <class Builder>
  ErrorFactory<Builder> operator()(Builder* builder) const {
    return ErrorFactory<Builder>(builder);
  }
};

template <class Base>
struct Policy : Base {
  // Instructs the error domain to return the specified value on error.
  template <class R>
  constexpr auto Return(R&& ret) const
      -> decltype(AddAnnotation<ReturnAnnotation>(
          *this,
          ReturnValue<typename std::decay<R>::type>{std::forward<R>(ret)})) {
    return AddAnnotation<ReturnAnnotation>(
        *this, ReturnValue<typename std::decay<R>::type>{std::forward<R>(ret)});
  }

  // Instructs the error domain to return a value of the specified type on
  // error. The value is constructed by calling `MakeError()`.
  template <class R = void>
  constexpr auto Return() const
      -> decltype(AddAnnotation<ReturnAnnotation>(*this, ReturnType<R>())) {
    return AddAnnotation<ReturnAnnotation>(*this, ReturnType<R>());
  }

  template <class F>
  constexpr auto DeferReturn(F&& f) const
      -> decltype(AddAnnotation<ReturnAnnotation>(
          *this,
          ReturnByFn<typename std::decay<F>::type>{std::forward<F>(f)})) {
    return AddAnnotation<ReturnAnnotation>(
        *this, ReturnByFn<typename std::decay<F>::type>{std::forward<F>(f)});
  }

  // The same as Return<R>(), where R is the declared function's result type.
  // Can't be used in functions with automatic return type.
  template <class X = void>
  constexpr auto AutoReturn() const -> decltype(AddAnnotation<ReturnAnnotation>(
      Defer<X>(*this), internal_return::ReturnAuto())) {
    return AddAnnotation<ReturnAnnotation>(*this,
                                           internal_return::ReturnAuto());
  }
};

template <class Base>
struct Builder : Observer<Base> {
  // Instructs the error domain to return the specified value on error.
  template <class R>
  auto Return(R&& ret) && -> decltype(AddAnnotation<ReturnAnnotation>(
      std::move(*this),
      ReturnValue<typename std::decay<R>::type>{std::forward<R>(ret)})) {
    return AddAnnotation<ReturnAnnotation>(
        std::move(*this),
        ReturnValue<typename std::decay<R>::type>{std::forward<R>(ret)});
  }

  // Instructs the error domain to return a value of the specified type on
  // error. The value is constructed by calling `MakeError()`.
  template <class R = void>
  auto Return() && -> decltype(AddAnnotation<ReturnAnnotation>(
      std::move(*this), ReturnType<R>())) {
    return AddAnnotation<ReturnAnnotation>(std::move(*this), ReturnType<R>());
  }

  template <class F>
  auto DeferReturn(F&& f) && -> decltype(AddAnnotation<ReturnAnnotation>(
      std::move(*this),
      ReturnByFn<typename std::decay<F>::type>{std::forward<F>(f)})) {
    return AddAnnotation<ReturnAnnotation>(
        std::move(*this),
        ReturnByFn<typename std::decay<F>::type>{std::forward<F>(f)});
  }

  // The same as Return<R>(), where R is the declared function's result type.
  // Can't be used in functions with automatic return type.
  template <class X = void>
  auto AutoReturn() && -> decltype(AddAnnotation<ReturnAnnotation>(
      Defer<X>(std::move(*this)), internal_return::ReturnAuto())) {
    return AddAnnotation<ReturnAnnotation>(std::move(*this),
                                           internal_return::ReturnAuto());
  }

  // This method is called directly by the merror macros.
  template <class X = void>
  auto BuildError() && -> decltype(GetAnnotationOr<ReturnAnnotation>(
      Defer<X>(*this),
      internal_return::ReturnAuto())(&Defer<X>(this)->derived())) {
    return GetAnnotationOr<ReturnAnnotation>(
        *this, internal_return::ReturnAuto())(&this->derived());
  }
};

}  // namespace internal_return

// Implements `BuildError()` builder method on top of `MakeError()`. Calls
// `ObserveRetVal()` on error.
//
// Adds `Return(val)`, `Return<Type>()`, `AutoReturn()` and `DeferReturn(f)`
// methods to the policy and builder.
//
// See comments at the top of the file for details.
using Return = Domain<internal_return::Policy, internal_return::Builder>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_RETURN_H_
