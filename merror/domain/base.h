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
// This library defines an extensible error domain framework. All code under
// //util/merror/domain uses the framework exclusively via its public API.
//
//                     ====[ Error Domains ]====
//
// The merror macros rely on error domains to classify objects as errors,
// construct error return values, trigger on-error side effects, etc. Error
// domains can be created with the following two functions:
//
//   Policy<PolicyExtension, Args...>()
//   Builder<BuilderExtension, Args...>()
//
// (Policy and builder extensions are explained below.)
//
// Individual error domains can be combined to yield bigger and more functional
// error domains. `EmptyDomain()` returns the most basic error domain that can
// be used as the starting point.
//
//   constexpr auto MyDomain = EmptyDomain().With(Foo, Bar, Baz);
//
// `With()` is associative: `d0.With(d1, ..., dN)` is equivalent to
// `d0.With(d1). ... .With(dN)`.
//
// `Domain<PolicyExtension, BuilderExtension>()` is a shortcut for
// `Policy<PolicyExtension>().With(Builder<BuilderExtension>())`.
//
//               ====[ Policy and Builder Extensions ]====
//
// Policy and builder extensions are class templates with any number of template
// parameters that publicly derive from the first parameter. When instantiated,
// they have an instance of `BasePolicy` and `BaseBuilder` respectively as the
// deepest ancestor. These classes have useful getters; check them out below.
//
// Policy extensions are responsible for the separation of good values from
// errors (`Verify()` and `Try()`) and the production of human-readable
// representations of objects (`PrintOperands`).
//
// Builder extensions are responsible for the construction of error return
// values (`BuildError`) and side effects.
//
// Policy and builder extensions must not have data members. All state must be
// stored in annotations.
//
//                       ====[ Annotations ]====
//
// Each instance of policy and builder has associated annotations -- a
// collection of heterogeneous values keyed by types. Annotations are added with
// `AddAnnotation<Key>(container, value)`, where `container` is either a policy
// or a builder. There can be zero or more values of potentially different types
// associated with a given key. The latest added annotation for the key can be
// retrieved with `GetAnnotation<Key>(container)`.
// `GetAnnotations<Key>(container)` returns all annotations for the key in the
// reverse order of their addition. 'GetAnnotationOr<Key>(container, value)`
// either returns the latest annotation or the default value if there are no
// annotations for the key.
//
// Builder annotations include annotations of the policy that produced the
// builder.
//
// `With()` combines annotations. Given functions `X(d)` and `Y(d)` that add
// annotations to their arguments, `X(a).With(Y(b))` is equivalent to
// `Y(X(a.With(b)))`.
//
//                        ====[ Example ]====
//
//   template <class Base>
//   struct AcceptBool : Base {
//     struct Acceptor {
//       bool IsError() const { return !value; }
//       bool GetCulprit() const { return false; }
//       bool value;
//     };
//     // When the argument of `MVERIFY()` is `false`, it's an error.
//     template <class R>
//     Acceptor Verify(Ref<R, bool> val) const { return {val.Get()}; }
//   };
//
//   // An empty tag type, used as an annotation key. The value is of type
//   // `bool`; the default is false.
//   struct LogEnabled {};
//
//   struct LogOptions {
//     template <class Base>
//     struct Policy : Base {
//       // Calling Log() on a policy enables logging for all merror macros that
//       // use the policy.
//       constexpr auto Log(bool enable = true) const {  // C++14 for simplicity
//         return AddAnnotation<LogEnabled>(*this, enable);
//       }
//     };
//     template <class Base>
//     struct Builder : Base {
//       // Calling Log() on a builder enables logging only for the selected
//       // instance of the merror macro.
//       auto Log(bool enable = true) {  // C++14 for simplicity
//         return AddAnnotation<LogEnabled>(std::move(*this), enable);
//       }
//     };
//   };
//
//   template <class Base, class Ret>
//   struct ErrorBuilder : Base {
//     Ret BuildError() {
//       // If logging has been enabled with Log(), log an error.
//       bool logging_enabled = GetAnnotationOr<LogEnabled>(*this, false);
//       if (logging_enabled) {
//         const auto& ctx = this->context();
//         // Error detected at foo.cc:42: MVERIFY(n < 100).
//         LOG(ERROR) << "Error detected at "
//                    << ctx.file << ":" << ctx.line << ": "
//                    << ctx.macro_str << "(" << ctx.args_str << ")";
//       }
//       // Return the default-constructed value from the function in which the
//       // error has been detected.
//       return Ret();
//     }
//   };
//
//   constexpr auto MyDomain = EmptyDomain().With(
//       Policy<AcceptBool>(),
//       // ErrorBuilder has two template arguments. We must pass one
//       // explicitly.
//       Builder<ErrorBuilder, bool>(),
//       Domain<LogOptions::Policy, LogOptions::Builder>());
//
//   bool Foo(int n) {
//     static constexpr auto MErrorDomain = MyDomain;
//     // Don't log if there is an error.
//     MVERIFY(n > 0);
//     // Enable logging just for this check.
//     MVERIFY(n < 100).Log();
//     ...
//     return true;
//   };
//
//   bool Bar(int n) {
//     // Log on errors by default.
//     static constexpr auto MErrorDomain = MyDomain.Log();
//     // Log if there is an error.
//     MVERIFY(n > 0);
//     // Disable logging just for this check.
//     MVERIFY(n < 100).Log(false);
//     ...
//     return true;
//   };

#ifndef MERROR_5EDA97_DOMAIN_BASE_H_
#define MERROR_5EDA97_DOMAIN_BASE_H_

#include <iosfwd>
#include <string>
#include <utility>

#include "merror/domain/defer.h"
#include "merror/domain/internal/type_map.h"
#include "merror/types.h"

namespace merror {

namespace internal_domain_base {

namespace type_map = internal::type_map;

// Copy-pasted from util/gtl/labs/enable_if.h to avoid a dependency on labs.
template <bool C>
using EnableIf = typename ::std::enable_if<C, int>::type;

template <class TheBuilder>
class BaseBuilder;

template <template <class> class Builder, class ThePolicy, class Context,
          class Annotations>
class ConcreteBuilder;

template <class B>
struct BuilderTraits;

template <template <class> class Builder, class ThePolicy, class Context,
          class Annotations>
struct BuilderTraits<
    ConcreteBuilder<Builder, ThePolicy, Context, Annotations>> {
  template <class Base>
  using BuilderType = Builder<Base>;
  using PolicyType = ThePolicy;
  using ContextType = Context;
  using AnnotationsType = Annotations;
};

template <class ThePolicy>
class BasePolicy;

template <template <class> class Policy, template <class> class Builder,
          class Annotations>
class ConcretePolicy;

template <class P>
struct PolicyTraits;

template <template <class> class Policy, template <class> class Builder,
          class Annotations>
struct PolicyTraits<ConcretePolicy<Policy, Builder, Annotations>> {
  template <class Base>
  using PolicyType = Policy<Base>;
  template <class Base>
  using BuilderType = Builder<Base>;
  using AnnotationsType = Annotations;
};

template <class B>
const typename BuilderTraits<B>::AnnotationsType& Annotations(
    const BaseBuilder<B>& b) {
  return static_cast<const B&>(b).annotations_;
}

template <class B>
typename BuilderTraits<B>::AnnotationsType& Annotations(BaseBuilder<B>& b) {
  return static_cast<B&>(b).annotations_;
}

template <class P>
constexpr const typename PolicyTraits<P>::AnnotationsType& Annotations(
    const BasePolicy<P>& p) {
  return static_cast<const P&>(p).annotations_;
}

template <class P>
constexpr typename PolicyTraits<P>::AnnotationsType& Annotations(
    BasePolicy<P>& p) {
  return static_cast<P&>(p).annotations_;
}

// The base class of all builders. Stateless. TheBuilder is an instantiation of
// ConcreteBuilder.
template <class TheBuilder>
class BaseBuilder {
 public:
  using BuilderType = TheBuilder;
  using PolicyType = typename BuilderTraits<BuilderType>::PolicyType;
  using ContextType = typename BuilderTraits<BuilderType>::ContextType;

  // Builder extensions can use the following getters from anywhere except the
  // constructor.

  // Returns *this cast to its dynamic type.
  BuilderType& derived() { return static_cast<BuilderType&>(*this); }
  const BuilderType& derived() const {
    return static_cast<const BuilderType&>(*this);
  }

  // Returns the policy whose `GetErrorBuilder()` method has produced this
  // builder.
  const PolicyType& policy() const { return derived().policy_; }

  // Returns error context.
  const ContextType& context() const { return derived().context_; }
};

// Generator for ConcreteBuilder that replaces annotations.
template <template <class> class B, class P, class C, class A1, class A2>
ConcreteBuilder<B, P, C, typename std::decay<A2>::type> ReplaceAnnotations(
    const ConcreteBuilder<B, P, C, A1>& b, A2&& annotations) {
  static_assert(!std::is_reference<C>::value, "");
  return {&b.policy_, std::move(b.context_), std::forward<A2>(annotations)};
}

// The most derived builder type.
template <template <class Base> class Builder, class ThePolicy, class Context,
          class Annotations>
class ConcreteBuilder
    : public Builder<BaseBuilder<
          ConcreteBuilder<Builder, ThePolicy, Context, Annotations>>> {
 public:
  static_assert(!std::is_reference<Context>::value, "");
  static_assert(!std::is_reference<Annotations>::value, "");

  // Aliases `*policy` and `context`.
  ConcreteBuilder(const ThePolicy* policy, Context&& context,
                  Annotations&& annotations)
      : policy_(*policy),
        context_(std::move(context)),
        annotations_(std::move(annotations)) {}

 private:
  template <class B>
  friend const typename BuilderTraits<B>::AnnotationsType& Annotations(
      const BaseBuilder<B>& b);

  template <class B>
  friend typename BuilderTraits<B>::AnnotationsType& Annotations(
      BaseBuilder<B>& b);

  template <template <class> class B, class P, class C, class A1, class A2>
  friend ConcreteBuilder<B, P, C, typename std::decay<A2>::type>
  ReplaceAnnotations(const ConcreteBuilder<B, P, C, A1>&, A2&&);

  template <class B>
  friend class BaseBuilder;

  // The policy whose `GetErrorBuilder()` method has produced this builder.
  const ThePolicy& policy_;
  // Error context.
  Context&& context_;
  // A TypeMap. See internal/type_map.h.
  Annotations annotations_;
};

// Merges a bunch of unary class templates into a single class template.
// MergeLayers<L1, ..., LN>::type<T> is LN<...<L1<T>>...>.
template <template <class> class... Ls>
struct MergeLayers {
  template <class Base>
  using type = Base;
};

template <template <class> class L, template <class> class... Ls>
struct MergeLayers<L, Ls...> {
  template <class Base>
  using type = typename MergeLayers<Ls...>::template type<L<Base>>;
};

// Generator for ConcretePolicy.
template <template <class> class P, template <class> class B, class A>
constexpr ConcretePolicy<P, B, typename std::decay<A>::type> MakePolicy(
    A&& annotations) {
  return ConcretePolicy<P, B, typename std::decay<A>::type>(
      std::forward<A>(annotations));
}

// The base class of all policies. Stateless. ThePolicy is an instantiation of
// ConcretePolicy.
template <class ThePolicy>
class BasePolicy {
 public:
  using PolicyType = ThePolicy;

  // Policy extensions can use this getter from anywhere except the constructor.
  constexpr const PolicyType& derived() const {
    return static_cast<const PolicyType&>(*this);
  }
  constexpr PolicyType& derived() { return static_cast<PolicyType&>(*this); }

  // Extends the policy with extra functionality.
  //
  //   // Policy extension that classifies all non-positive integers as errors.
  //   template <class Base>
  //   struct PositiveEnforcer : Base {
  //     struct Acceptor {
  //       bool IsError() const { return n <= 0; }
  //       int GetCulprit() const { return n; }
  //       int value;
  //     };
  //     template <class R>
  //     Acceptor Verify(Ref<R, int> val) const { return {val.Get()}; }
  //   };
  //
  //   constexpr auto Pos = Policy<PositiveEnforcer>();
  //
  //   Status DoStuff(int n) {
  //     MVERIFY(With(Pos), n);
  //     ...
  //   }
  //
  // p.With(e1, ..., eN) is equivalent to p.With(e1). ... .With(eN).
  //
  // TODO(romanp): Add an rvalue overload when we stop supporting gcc 4.9.
  template <template <class> class... Ps, template <class> class... Bs,
            class... As, class X = void>
  constexpr auto With(const ConcretePolicy<Ps, Bs, As>&... extras) const
      -> decltype(MakePolicy<
                  MergeLayers<PolicyTraits<ThePolicy>::template PolicyType,
                              Ps...>::template type,
                  MergeLayers<PolicyTraits<ThePolicy>::template BuilderType,
                              Bs...>::template type>(
          type_map::Merge(Defer<X>(this)->derived().annotations_,
                          extras.annotations_...))) {
    return MakePolicy<MergeLayers<PolicyTraits<ThePolicy>::template PolicyType,
                                  Ps...>::template type,
                      MergeLayers<PolicyTraits<ThePolicy>::template BuilderType,
                                  Bs...>::template type>(
        type_map::Merge(derived().annotations_, extras.annotations_...));
  }

  // Called by the `MVERIFY(expr)` if `expr` is a relational binary expression
  // classified as an error.
  //
  // Should either do nothing and return false, or set `left_str` and
  // `right_str` to the human-readable representation of `left` and `right`
  // respectively and return true. In the latter case error context, accessible
  // to the builder via `context()`, will have non-null `rel_expr`.
  template <class L, class R>
  bool PrintOperands(const L& left, const R& right, std::string* left_str,
                     std::string* right_str) const {
    return false;
  }

  // Called by the merror macros when an error is detected.
  template <class Context>
  ConcreteBuilder<PolicyTraits<PolicyType>::template BuilderType, PolicyType,
                  Context, type_map::EmptyTypeMap>
  GetErrorBuilder(Context&& ctx) const {
    static_assert(!std::is_reference<Context>::value, "");
    return {&this->derived(), std::move(ctx), {}};
  }
};

// Generator for ConcretePolicy that replaces annotations.
template <template <class> class P, template <class> class B, class A1,
          class A2>
constexpr ConcretePolicy<P, B, typename std::decay<A2>::type>
ReplaceAnnotations(const ConcretePolicy<P, B, A1>&, A2&& annotations) {
  return ConcretePolicy<P, B, typename std::decay<A2>::type>(
      std::forward<A2>(annotations));
}

// The most derived policy type.
// TODO(romanp): replace __attribute__((warn_unused)) with a macro from
// base/port.h or base/macros.h when one is added.
template <template <class Base> class Policy,
          template <class Base> class Builder, class Annotations>
class __attribute__((warn_unused)) ConcretePolicy
    : public Policy<BasePolicy<ConcretePolicy<Policy, Builder, Annotations>>> {
 public:
  static_assert(!std::is_reference<Annotations>::value, "");

  template <class A = Annotations,
            EnableIf<std::is_same<A, type_map::EmptyTypeMap>{}> = 0>
  constexpr ConcretePolicy() {}
  constexpr explicit ConcretePolicy(Annotations&& annotations)
      : annotations_(std::move(annotations)) {}
  constexpr explicit ConcretePolicy(const Annotations& annotations)
      : annotations_(annotations) {}

  // operator() is called by the merror macros. It's an identity function.
  ConcretePolicy& operator()() & { return *this; }
  constexpr const ConcretePolicy& operator()() const& { return *this; }
  ConcretePolicy&& operator()() && { return std::move(*this); }
  constexpr const ConcretePolicy&& operator()() const&& {
    return std::move(*this);
  }

 private:
  template <class P>
  friend constexpr const typename PolicyTraits<P>::AnnotationsType& Annotations(
      const BasePolicy<P>& p);

  template <class P>
  friend constexpr typename PolicyTraits<P>::AnnotationsType& Annotations(
      BasePolicy<P>& p);

  template <class P>
  friend class BasePolicy;

  template <template <class> class P, template <class> class B, class A>
  friend class ConcretePolicy;

  // A TypeMap. See internal/type_map.h.
  Annotations annotations_;
};

template <class Base>
using Id = Base;

// Binds all arguments but the first of a class template.
template <template <class, class...> class TT, class... Bound>
struct Bind {
  template <class Base>
  using type = TT<Base, Bound...>;
};

// Generator for error domain from policy extension.
template <template <class, class...> class Impl, class... Bound>
using Policy = ConcretePolicy<Bind<Impl, Bound...>::template type, Id,
                              type_map::EmptyTypeMap>;

// Generator for error domain from builder extension.
template <template <class, class...> class Impl, class... Bound>
using Builder = ConcretePolicy<Id, Bind<Impl, Bound...>::template type,
                               type_map::EmptyTypeMap>;

// Generator for error domain from policy and builder extensions.
template <template <class> class PolicyImpl, template <class> class BuilderImpl>
using Domain = ConcretePolicy<PolicyImpl, BuilderImpl, type_map::EmptyTypeMap>;

template <class Key, class P>
typename type_map::Has<Key, decltype(Annotations(std::declval<P>()))>::type
HasAnnotationImpl(const BasePolicy<P>&);

template <class Key, class B>
typename type_map::Has<
    Key, decltype(Annotations(std::declval<B>())),
    decltype(Annotations(std::declval<typename B::PolicyType>()))>::type
HasAnnotationImpl(const BaseBuilder<B>&);

// Container must be either a policy or a builder. Returns true if there is an
// associated annotation with the given key.
template <class Key, class Container>
constexpr bool HasAnnotation() {
  return decltype(HasAnnotationImpl<Key>(std::declval<Container>()))::value;
}
template <class Key, class Container>
constexpr bool HasAnnotation(const Container& c) {
  return HasAnnotation<Key, Container>();
}

// Adds the annotation to the policy or builder.
template <class Key, class P, class Value>
constexpr auto AddAnnotation(const BasePolicy<P>& p, Value&& value)
    -> decltype(ReplaceAnnotations(
        p.derived(),
        type_map::Add<Key>(Annotations(p), std::forward<Value>(value)))) {
  return ReplaceAnnotations(
      p.derived(),
      type_map::Add<Key>(Annotations(p), std::forward<Value>(value)));
}
template <class Key, class P, class Value>
constexpr auto AddAnnotation(BasePolicy<P>&& p, Value&& value)
    -> decltype(ReplaceAnnotations(
        p.derived(), type_map::Add<Key>(std::move(Annotations(p)),
                                        std::forward<Value>(value)))) {
  return ReplaceAnnotations(p.derived(),
                            type_map::Add<Key>(std::move(Annotations(p)),
                                               std::forward<Value>(value)));
}
template <class Key, class B, class Value>
auto AddAnnotation(const BaseBuilder<B>& b, Value&& value)
    -> decltype(ReplaceAnnotations(
        b.derived(),
        type_map::Add<Key>(Annotations(b), std::forward<Value>(value)))) {
  return ReplaceAnnotations(
      b.derived(),
      type_map::Add<Key>(Annotations(b), std::forward<Value>(value)));
}
template <class Key, class B, class Value>
auto AddAnnotation(BaseBuilder<B>&& b, Value&& value)
    -> decltype(ReplaceAnnotations(
        b.derived(), type_map::Add<Key>(std::move(Annotations(b)),
                                        std::forward<Value>(value)))) {
  return ReplaceAnnotations(b.derived(),
                            type_map::Add<Key>(std::move(Annotations(b)),
                                               std::forward<Value>(value)));
}

// Removes annotations from the policy or builder.
template <class Key, class P>
constexpr auto RemoveAnnotations(const BasePolicy<P>& p)
    -> decltype(ReplaceAnnotations(p.derived(),
                                   type_map::RemoveAll<Key>(Annotations(p)))) {
  return ReplaceAnnotations(p.derived(),
                            type_map::RemoveAll<Key>(Annotations(p)));
}
template <class Key, class P>
constexpr auto RemoveAnnotations(BasePolicy<P>&& p)
    -> decltype(ReplaceAnnotations(
        p.derived(), type_map::RemoveAll<Key>(std::move(Annotations(p))))) {
  return ReplaceAnnotations(
      p.derived(), type_map::RemoveAll<Key>(std::move(Annotations(p))));
}
template <class Key, class B>
auto RemoveAnnotations(const BaseBuilder<B>& b)
    -> decltype(ReplaceAnnotations(b.derived(),
                                   type_map::RemoveAll<Key>(Annotations(b)))) {
  return ReplaceAnnotations(b.derived(),
                            type_map::RemoveAll<Key>(Annotations(b)));
}
template <class Key, class B>
auto RemoveAnnotations(BaseBuilder<B>&& b) -> decltype(ReplaceAnnotations(
    b.derived(), type_map::RemoveAll<Key>(std::move(Annotations(b))))) {
  return ReplaceAnnotations(
      b.derived(), type_map::RemoveAll<Key>(std::move(Annotations(b))));
}

// Retrieves the annotation from the policy or builder.
template <class Key, class P>
constexpr auto GetAnnotation(const BasePolicy<P>& p)
    -> decltype(type_map::Get<Key>(Annotations(p))) {
  return type_map::Get<Key>(Annotations(p));
}
template <class Key, class P>
constexpr auto GetAnnotation(BasePolicy<P>& p)
    -> decltype(type_map::Get<Key>(Annotations(p))) {
  return type_map::Get<Key>(Annotations(p));
}
template <class Key, class P>
constexpr auto GetAnnotation(BasePolicy<P>&& p)
    -> decltype(type_map::Get<Key>(std::move(Annotations(p)))) {
  return type_map::Get<Key>(std::move(Annotations(p)));
}
template <class Key, class B>
auto GetAnnotation(const BaseBuilder<B>& b)
    -> decltype(type_map::Get<Key>(Annotations(b), Annotations(b.policy()))) {
  return type_map::Get<Key>(Annotations(b), Annotations(b.policy()));
}
template <class Key, class B>
auto GetAnnotation(BaseBuilder<B>& b)
    -> decltype(type_map::Get<Key>(Annotations(b), Annotations(b.policy()))) {
  return type_map::Get<Key>(Annotations(b), Annotations(b.policy()));
}
template <class Key, class B>
auto GetAnnotation(BaseBuilder<B>&& b)
    -> decltype(type_map::Get<Key>(std::move(Annotations(b)),
                                   Annotations(b.policy()))) {
  return type_map::Get<Key>(std::move(Annotations(b)), Annotations(b.policy()));
}

// Returns the annotation or the default value if not found.
template <class Key, class P, class Value>
constexpr auto GetAnnotationOr(const BasePolicy<P>& p, Value&& value)
    -> decltype(type_map::GetOr<Key>(Annotations(p),
                                     std::forward<Value>(value))) {
  return type_map::GetOr<Key>(Annotations(p), std::forward<Value>(value));
}
template <class Key, class P, class Value>
constexpr auto GetAnnotationOr(BasePolicy<P>& p, Value&& value)
    -> decltype(type_map::GetOr<Key>(Annotations(p),
                                     std::forward<Value>(value))) {
  return type_map::GetOr<Key>(Annotations(p), std::forward<Value>(value));
}
template <class Key, class P, class Value>
constexpr auto GetAnnotationOr(BasePolicy<P>&& p, Value&& value)
    -> decltype(type_map::GetOr<Key>(std::move(Annotations(p)),
                                     std::forward<Value>(value))) {
  return type_map::GetOr<Key>(std::move(Annotations(p)),
                              std::forward<Value>(value));
}
template <class Key, class B, class Value>
auto GetAnnotationOr(const BaseBuilder<B>& b, Value&& value)
    -> decltype(type_map::GetOr<Key>(Annotations(b), Annotations(b.policy()),
                                     std::forward<Value>(value))) {
  return type_map::GetOr<Key>(Annotations(b), Annotations(b.policy()),
                              std::forward<Value>(value));
}
template <class Key, class B, class Value>
auto GetAnnotationOr(BaseBuilder<B>& b, Value&& value)
    -> decltype(type_map::GetOr<Key>(Annotations(b), Annotations(b.policy()),
                                     std::forward<Value>(value))) {
  return type_map::GetOr<Key>(Annotations(b), Annotations(b.policy()),
                              std::forward<Value>(value));
}
template <class Key, class B, class Value>
auto GetAnnotationOr(BaseBuilder<B>&& b, Value&& value)
    -> decltype(type_map::GetOr<Key>(std::move(Annotations(b)),
                                     Annotations(b.policy()),
                                     std::forward<Value>(value))) {
  return type_map::GetOr<Key>(std::move(Annotations(b)),
                              Annotations(b.policy()),
                              std::forward<Value>(value));
}

// Returns all annotations with the given key.
template <class Key, class P>
constexpr auto GetAnnotations(const BasePolicy<P>& p)
    -> decltype(type_map::GetAll<Key>(Annotations(p))) {
  return type_map::GetAll<Key>(Annotations(p));
}
template <class Key, class P>
constexpr auto GetAnnotations(BasePolicy<P>& p)
    -> decltype(type_map::GetAll<Key>(Annotations(p))) {
  return type_map::GetAll<Key>(Annotations(p));
}
template <class Key, class P>
constexpr auto GetAnnotations(BasePolicy<P>&& p)
    -> decltype(type_map::GetAll<Key>(std::move(Annotations(p)))) {
  return type_map::GetAll<Key>(std::move(Annotations(p)));
}
template <class Key, class B>
auto GetAnnotations(const BaseBuilder<B>& b)
    -> decltype(type_map::GetAll<Key>(Annotations(b),
                                      Annotations(b.policy()))) {
  return type_map::GetAll<Key>(Annotations(b), Annotations(b.policy()));
}
template <class Key, class B>
auto GetAnnotations(BaseBuilder<B>& b)
    -> decltype(type_map::GetAll<Key>(Annotations(b),
                                      Annotations(b.policy()))) {
  return type_map::GetAll<Key>(Annotations(b), Annotations(b.policy()));
}
template <class Key, class B>
auto GetAnnotations(BaseBuilder<B>&& b)
    -> decltype(type_map::GetAll<Key>(std::move(Annotations(b)),
                                      Annotations(b.policy()))) {
  return type_map::GetAll<Key>(std::move(Annotations(b)),
                               Annotations(b.policy()));
}

}  // namespace internal_domain_base

using internal_domain_base::Builder;
using internal_domain_base::Domain;
using internal_domain_base::Policy;

using internal_domain_base::AddAnnotation;
using internal_domain_base::GetAnnotation;
using internal_domain_base::GetAnnotationOr;
using internal_domain_base::GetAnnotations;
using internal_domain_base::HasAnnotation;
using internal_domain_base::RemoveAnnotations;

// The most basic error domain. It can't be used with the merror macros out of
// the box because it deliberately lacks several required methods. It should be
// extended via `With()` to build a full-featured domain.
//
//   constexpr auto MyErrorDomain = merror::EmptyDomain().With(...);
//
// It's often better to start with `merror::MErrorDomain()` than with
// `merror::EmptyDomain()`. If you really need the latter, consider copy-pasting
// the definition of `merror::MErrorDomain()` and removing the extensions you
// don't need.
using EmptyDomain = Domain<internal_domain_base::Id, internal_domain_base::Id>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_BASE_H_
