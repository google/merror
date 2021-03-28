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

#include "merror/domain/base.h"

#include <tuple>
#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "merror/domain/defer.h"
#include "merror/domain/internal/capture_stream.h"
#include "merror/macros.h"

namespace merror {
namespace {

using ::testing::_;
using ::testing::HasSubstr;

struct Any {
  template <class T>
  Any(const T&) {}
};

template <class Base, class... Tags>
struct Tagged : Base {};

template <class Base>
using TaggedChar = Tagged<Base, char>;

template <class Base>
using TaggedIntVoid = Tagged<Base, int, void>;

template <class Base>
bool IsTagged(const Tagged<Tagged<Base, char>, int, void>&) {
  return true;
}

bool IsTagged(Any) { return false; }

struct FakeContext {
  static FakeContext&& Instance() {
    static FakeContext ctx = {};
    return std::move(ctx);
  }
};

template <class Domain>
auto GetBuilder(Domain&& domain)
    -> decltype(std::forward<Domain>(domain).GetErrorBuilder(
        FakeContext::Instance())) {
  return std::forward<Domain>(domain).GetErrorBuilder(FakeContext::Instance());
}

TEST(Composition, Policy) {
  static constexpr auto a = Policy<Tagged, char>();
  static constexpr auto b = Policy<Tagged, int, void>();
  EXPECT_TRUE(IsTagged(EmptyDomain().With(a, b)));
  EXPECT_FALSE(IsTagged(GetBuilder(EmptyDomain().With(a, b))));
  EXPECT_TRUE(IsTagged(EmptyDomain().With(a).With(b)));
  EXPECT_FALSE(IsTagged(GetBuilder(EmptyDomain().With(a).With(b))));
  EXPECT_TRUE(IsTagged(a.With(b)));
  EXPECT_FALSE(IsTagged(GetBuilder(a.With(b))));
}

TEST(Composition, Builder) {
  static constexpr auto a = Builder<Tagged, char>();
  static constexpr auto b = Builder<Tagged, int, void>();
  EXPECT_FALSE(IsTagged(EmptyDomain().With(a, b)));
  EXPECT_FALSE(IsTagged(EmptyDomain().With(a).With(b)));
  EXPECT_TRUE(IsTagged(GetBuilder(a.With(b))));
}

TEST(Composition, Domain) {
  static constexpr auto a = Domain<TaggedChar, TaggedChar>();
  static constexpr auto b = Domain<TaggedIntVoid, TaggedIntVoid>();
  EXPECT_TRUE(IsTagged(EmptyDomain().With(a, b)));
  EXPECT_TRUE(IsTagged(GetBuilder(EmptyDomain().With(a, b))));
  EXPECT_TRUE(IsTagged(EmptyDomain().With(a).With(b)));
  EXPECT_TRUE(IsTagged(GetBuilder(EmptyDomain().With(a).With(b))));
  EXPECT_TRUE(IsTagged(a.With(b)));
  EXPECT_TRUE(IsTagged(GetBuilder(a.With(b))));
}

TEST(Composition, Annotations) {
  static constexpr auto a =
      AddAnnotation<void>(AddAnnotation<void>(EmptyDomain(), 1), 2);
  static constexpr auto b =
      AddAnnotation<void>(AddAnnotation<void>(EmptyDomain(), 3), 4);
  static constexpr auto c =
      AddAnnotation<void>(AddAnnotation<void>(EmptyDomain(), 5), 6);
  EXPECT_EQ(std::make_tuple(6, 5, 4, 3, 2, 1),
            GetAnnotations<void>(a.With(b, c)));
  EXPECT_EQ(std::make_tuple(6, 5, 4, 3, 2, 1),
            GetAnnotations<void>(GetBuilder(a.With(b, c))));
  EXPECT_EQ(std::make_tuple(),
            GetAnnotations<void>(RemoveAnnotations<void>(a.With(b, c))));
}

struct MoveOnly {
  MoveOnly() = default;
  MoveOnly(MoveOnly&&) = default;
  MoveOnly(const MoveOnly&) = delete;
  int value;
  friend bool operator==(const MoveOnly& a, const MoveOnly& b) {
    return a.value == b.value;
  }
};

TEST(Annotations, ConstexprPolicy) {
  static constexpr auto domain = AddAnnotation<void>(
      AddAnnotation<void>(EmptyDomain(), MoveOnly{1}), MoveOnly{2});
  {
    static constexpr std::tuple<const MoveOnly&, const MoveOnly&> annotations =
        GetAnnotations<void>(domain);
    EXPECT_EQ(std::make_tuple(MoveOnly{2}, MoveOnly{1}), annotations);
  }
  {
    static constexpr const MoveOnly& annotation = GetAnnotation<void>(domain);
    EXPECT_EQ(MoveOnly{2}, annotation);
  }
  {
    static constexpr const MoveOnly& annotation =
        GetAnnotationOr<void>(domain, 42);
    EXPECT_EQ(MoveOnly{2}, annotation);
  }
  {
    static constexpr int annotation = GetAnnotationOr<void*>(domain, 42);
    EXPECT_EQ(42, annotation);
  }
  {
    static constexpr auto a = AddAnnotation<void>(EmptyDomain(), 1);
    static constexpr auto b = AddAnnotation<void>(a, 2);
    EXPECT_EQ(std::make_tuple(2, 1), GetAnnotations<void>(b));
  }
  {
    static constexpr auto a = AddAnnotation<void>(EmptyDomain(), 1);
    static constexpr auto b = AddAnnotation<int>(a, 2);
    static constexpr auto c = AddAnnotation<int>(RemoveAnnotations<int>(b), 5);
    EXPECT_EQ(std::make_tuple(5), GetAnnotations<int>(c));
  }
}

TEST(Annotations, NonConstPolicy) {
  auto domain = AddAnnotation<void>(
      AddAnnotation<void>(EmptyDomain(), MoveOnly{1}), MoveOnly{2});
  {
    std::tuple<MoveOnly&, MoveOnly&> annotations = GetAnnotations<void>(domain);
    EXPECT_EQ(std::make_tuple(MoveOnly{2}, MoveOnly{1}), annotations);
  }
  {
    MoveOnly& annotation = GetAnnotation<void>(domain);
    EXPECT_EQ(MoveOnly{2}, annotation);
  }
  {
    MoveOnly& annotation = GetAnnotationOr<void>(domain, 42);
    EXPECT_EQ(MoveOnly{2}, annotation);
  }
  {
    int n = 0;
    int& annotation = GetAnnotationOr<void*>(domain, n);
    EXPECT_EQ(&n, &annotation);
  }
  {
    const int n = 0;
    const int& annotation = GetAnnotationOr<void*>(domain, n);
    EXPECT_EQ(&n, &annotation);
  }
  {
    int n = 0;
    int&& annotation = GetAnnotationOr<void*>(domain, std::move(n));
    EXPECT_EQ(&n, &annotation);
  }
}

TEST(Annotations, RvaluePolicy) {
  auto domain = AddAnnotation<void>(
      AddAnnotation<void>(EmptyDomain(), MoveOnly{1}), MoveOnly{2});
  {
    std::tuple<MoveOnly&&, MoveOnly&&> annotations =
        GetAnnotations<void>(std::move(domain));
    EXPECT_EQ(std::make_tuple(MoveOnly{2}, MoveOnly{1}), annotations);
  }
  {
    MoveOnly&& annotation = GetAnnotation<void>(std::move(domain));
    EXPECT_EQ(MoveOnly{2}, annotation);
  }
  {
    MoveOnly&& annotation = GetAnnotationOr<void>(std::move(domain), 42);
    EXPECT_EQ(MoveOnly{2}, annotation);
  }
  {
    int n = 0;
    int& annotation = GetAnnotationOr<void*>(std::move(domain), n);
    EXPECT_EQ(&n, &annotation);
  }
  {
    const int n = 0;
    const int& annotation = GetAnnotationOr<void*>(std::move(domain), n);
    EXPECT_EQ(&n, &annotation);
  }
  {
    int n = 0;
    int&& annotation = GetAnnotationOr<void*>(std::move(domain), std::move(n));
    EXPECT_EQ(&n, &annotation);
  }
}

TEST(Annotations, ConstBuilder) {
  static constexpr auto domain =
      AddAnnotation<void>(EmptyDomain(), MoveOnly{1});
  const auto builder = AddAnnotation<void>(GetBuilder(domain), MoveOnly{2});
  {
    std::tuple<const MoveOnly&, const MoveOnly&> annotations =
        GetAnnotations<void>(builder);
    EXPECT_EQ(std::make_tuple(MoveOnly{2}, MoveOnly{1}), annotations);
  }
  {
    const MoveOnly& annotation = GetAnnotation<void>(builder);
    EXPECT_EQ(MoveOnly{2}, annotation);
  }
  {
    const MoveOnly& annotation = GetAnnotationOr<void>(builder, 42);
    EXPECT_EQ(MoveOnly{2}, annotation);
  }
  {
    int n = 0;
    int& annotation = GetAnnotationOr<void*>(builder, n);
    EXPECT_EQ(&n, &annotation);
  }
  {
    const int n = 0;
    const int& annotation = GetAnnotationOr<void*>(builder, n);
    EXPECT_EQ(&n, &annotation);
  }
  {
    int n = 0;
    int&& annotation = GetAnnotationOr<void*>(builder, std::move(n));
    EXPECT_EQ(&n, &annotation);
  }
  {
    const auto x = AddAnnotation<void>(GetBuilder(domain), 2);
    const auto y = AddAnnotation<void>(x, 3);
    EXPECT_EQ(std::make_tuple(3, 2, MoveOnly{1}), GetAnnotations<void>(y));
  }
}

TEST(Annotations, NonConstBuilder) {
  static constexpr auto domain =
      AddAnnotation<void>(EmptyDomain(), MoveOnly{1});
  auto builder = AddAnnotation<void>(GetBuilder(domain), MoveOnly{2});
  {
    std::tuple<MoveOnly&, const MoveOnly&> annotations =
        GetAnnotations<void>(builder);
    EXPECT_EQ(std::make_tuple(MoveOnly{2}, MoveOnly{1}), annotations);
  }
  {
    MoveOnly& annotation = GetAnnotation<void>(builder);
    EXPECT_EQ(MoveOnly{2}, annotation);
  }
  {
    MoveOnly& annotation = GetAnnotationOr<void>(builder, 42);
    EXPECT_EQ(MoveOnly{2}, annotation);
  }
  {
    int n = 0;
    int& annotation = GetAnnotationOr<void*>(builder, n);
    EXPECT_EQ(&n, &annotation);
  }
  {
    const int n = 0;
    const int& annotation = GetAnnotationOr<void*>(builder, n);
    EXPECT_EQ(&n, &annotation);
  }
  {
    int n = 0;
    int&& annotation = GetAnnotationOr<void*>(builder, std::move(n));
    EXPECT_EQ(&n, &annotation);
  }
}

TEST(Annotations, RvalueBuilder) {
  static constexpr auto domain =
      AddAnnotation<void>(EmptyDomain(), MoveOnly{1});
  auto builder = AddAnnotation<void>(GetBuilder(domain), MoveOnly{2});
  {
    std::tuple<MoveOnly&&, const MoveOnly&> annotations =
        GetAnnotations<void>(std::move(builder));
    EXPECT_EQ(std::make_tuple(MoveOnly{2}, MoveOnly{1}), annotations);
  }
  {
    MoveOnly&& annotation = GetAnnotation<void>(std::move(builder));
    EXPECT_EQ(MoveOnly{2}, annotation);
  }
  {
    MoveOnly&& annotation = GetAnnotationOr<void>(std::move(builder), 42);
    EXPECT_EQ(MoveOnly{2}, annotation);
  }
  {
    int n = 0;
    int& annotation = GetAnnotationOr<void*>(std::move(builder), n);
    EXPECT_EQ(&n, &annotation);
  }
  {
    const int n = 0;
    const int& annotation = GetAnnotationOr<void*>(std::move(builder), n);
    EXPECT_EQ(&n, &annotation);
  }
  {
    int n = 0;
    int&& annotation = GetAnnotationOr<void*>(std::move(builder), std::move(n));
    EXPECT_EQ(&n, &annotation);
  }
}

TEST(Annotations, HasAnnotation) {
  static constexpr auto domain = AddAnnotation<void>(EmptyDomain(), 1);
  const auto builder = AddAnnotation<void*>(GetBuilder(domain), 2);
  using Domain = decltype(domain);
  using Builder = decltype(builder);
  static_assert(HasAnnotation<void>(domain), "");
  static_assert(!HasAnnotation<void*>(domain), "");
  static_assert(!HasAnnotation<void**>(domain), "");
  static_assert(HasAnnotation<void>(builder), "");
  static_assert(HasAnnotation<void*>(builder), "");
  static_assert(!HasAnnotation<void**>(builder), "");
  static_assert(HasAnnotation<void, Domain>(), "");
  static_assert(!HasAnnotation<void*, Domain>(), "");
  static_assert(!HasAnnotation<void**, Domain>(), "");
  static_assert(HasAnnotation<void, Builder>(), "");
  static_assert(HasAnnotation<void*, Builder>(), "");
  static_assert(!HasAnnotation<void**, Builder>(), "");
}

template <class Expected, class Actual>
void ExpectSame(Expected&& expected, Actual&& actual) {
  static_assert(std::is_same<Expected, Actual>(), "");
  EXPECT_EQ(&expected, &actual);
}

template <class T>
const T& Const(const T& val) {
  return val;
}

TEST(Getters, Policy) {
  {
    static constexpr auto policy =
        Policy<Tagged, char>().With(Policy<Tagged, int>());
    static constexpr const auto& derived = policy.derived();
    ExpectSame(policy, derived);
    static constexpr const auto& self_lval = policy();
    ExpectSame(policy, self_lval);
    static constexpr const auto&& self_rval = std::move(policy)();
    ExpectSame(std::move(policy), std::forward<decltype(self_rval)>(self_rval));
  }
  {
    auto policy = Policy<Tagged, char>().With(Policy<Tagged, int>());
    ExpectSame(policy, policy());
    ExpectSame(Const(policy), Const(policy)());
    ExpectSame(std::move(policy), std::move(policy()));
    ExpectSame(std::move(Const(policy)), std::move(Const(policy)()));
  }
}

TEST(Getters, Builder) {
  const auto policy = Policy<Tagged, char>().With(Policy<Tagged, int>());
  auto builder = GetBuilder(policy);
  ExpectSame(builder, builder.derived());
  ExpectSame(Const(builder), Const(builder).derived());
  ExpectSame(policy, builder.policy());
  ExpectSame(Const(FakeContext::Instance()), builder.context());
}

// The example from the top of base.h.
namespace example {

template <class Base>
struct AcceptBool : Base {
  struct Acceptor {
    // When the argument of `MVERIFY()` is `false`, it's an error.
    bool IsError() const { return !val; }
    bool GetCulprit() const { return false; }
    bool val;
  };
  template <class R>
  Acceptor Verify(Ref<R, bool> val) const {
    return {val.Get()};
  }
};

// An empty tag type, used as an annotation key. The value is of type `bool`;
// the default is false.
struct LogEnabled {};

struct LogOptions {
  template <class Base>
  struct Policy : Base {
    // Calling Log() on a policy enables logging for all merror macros that use
    // the policy.
    template <class X = void>
    constexpr auto Log(bool enable = true) const
        -> decltype(AddAnnotation<LogEnabled>(Defer<X>(*this), enable)) {
      return AddAnnotation<LogEnabled>(*this, enable);
    }
  };
  template <class Base>
  struct Builder : Base {
    // Calling Log() on a builder enables logging only for the selected instance
    // of the merror macro.
    template <class X = void>
    auto Log(bool enable = true)
        -> decltype(AddAnnotation<LogEnabled>(Defer<X>(std::move(*this)),
                                              enable)) {
      return AddAnnotation<LogEnabled>(std::move(*this), enable);
    }
  };
};

template <class Base, class Ret>
struct ErrorBuilder : Base {
  Ret BuildError() {
    // If logging has been enabled with Log(), log an error.
    bool logging_enabled = GetAnnotationOr<LogEnabled>(*this, false);
    if (logging_enabled) {
      const auto& ctx = this->context();
      // Error detected at foo.cc:42: MVERIFY(n < 100).
      std::cout << "Error detected at " << ctx.file << ":" << ctx.line << ": "
                << ctx.macro_str << "(" << ctx.args_str << ")";
    }
    // Return the default-constructed value from the function in which the error
    // has been detected.
    return Ret();
  }
};

constexpr auto MyDomain = EmptyDomain().With(
    Policy<AcceptBool>(),
    // ErrorBuilder has two template arguments. We must pass one explicitly.
    Builder<ErrorBuilder, bool>(),
    Domain<LogOptions::Policy, LogOptions::Builder>());

bool Foo(int n) {
  static constexpr auto MErrorDomain = MyDomain;
  // Don't log if there is an error.
  MVERIFY(n > 0);
  // Enable logging just for this check.
  MVERIFY(n < 100).Log();
  return true;
}

bool Bar(int n) {
  // Log on errors by default.
  static constexpr auto MErrorDomain = MyDomain.Log();
  // Log if there is an error.
  MVERIFY(n > 0);
  // Disable logging just for this check.
  MVERIFY(n < 100).Log(false);
  return true;
}

TEST(Example, Functional) {
  std::string out;
  bool b;
  {
    internal::CaptureStream c(std::cout);
    b = Foo(1);
    out = c.str();
  }
  EXPECT_TRUE(b);
  EXPECT_TRUE(out.empty());
  {
    internal::CaptureStream c(std::cout);
    b = Foo(-1);
    out = c.str();
  }
  EXPECT_FALSE(b);
  EXPECT_TRUE(out.empty());
  {
    internal::CaptureStream c(std::cout);
    b = Foo(200);
    out = c.str();
  }
  EXPECT_FALSE(b);
  EXPECT_THAT(out, HasSubstr("Error detected"));
  {
    internal::CaptureStream c(std::cout);
    b = Bar(1);
    out = c.str();
  }
  EXPECT_TRUE(b);
  EXPECT_TRUE(out.empty());
  {
    internal::CaptureStream c(std::cout);
    b = Bar(-1);
    out = c.str();
  }
  EXPECT_FALSE(b);
  EXPECT_THAT(out, HasSubstr("Error detected"));
  {
    internal::CaptureStream c(std::cout);
    b = Bar(200);
    out = c.str();
  }
  EXPECT_FALSE(b);
  EXPECT_TRUE(out.empty());
}

}  // namespace example

}  // namespace
}  // namespace merror
