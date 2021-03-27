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

#include "merror/domain/description.h"

#include <utility>

#include "gtest/gtest.h"
#include "merror/domain/base.h"
#include "merror/domain/defer.h"
#include "merror/macros.h"

namespace merror {
namespace {

template <class F>
struct LazyStream {
  friend std::ostream& operator<<(std::ostream& o, const LazyStream& p) {
    p.f(o);
    return o;
  }
  F f;
};

template <class F>
LazyStream<F> GStreamable(LazyStream<F> f) {
  return f;
}

template <class F>
constexpr LazyStream<F> MakeLazyStream(F f) {
  return {std::move(f)};
}

struct ExpectedBuilderDescription {};

template <class Base>
struct MyPolicy : Base {
  struct Acceptor {
    bool IsError() const { return !val; }
    bool GetCulprit() const { return false; }
    bool val;
  };
  template <class R>
  Acceptor Verify(Ref<R, bool> val) const {
    return {val.Get()};
  }
  template <class Self = MyPolicy>
  constexpr auto GetCopy() const ->
      typename std::decay<decltype(Defer<Self>(this)->derived())>::type {
    return Defer<Self>(this)->derived();
  }
};

template <class Base>
struct MyBuilder : Base {
  void BuildError() && {
    std::string_view expected =
        GetAnnotationOr<ExpectedBuilderDescription>(*this, std::string_view());
    std::string_view actual = GetBuilderDescription(*this);
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(!expected.data(), !actual.data());
  }

  typename Base::BuilderType&& CheckPolicyDescription(
      std::string_view expected) && {
    std::string_view actual = GetPolicyDescription(*this);
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(!expected.data(), !actual.data());
    return std::move(this->derived());
  }

  template <class X = void>
  auto
  ExpectBuilderDescription(std::string_view expected) && -> decltype(AddAnnotation<
                                                                     ExpectedBuilderDescription>(
      Defer<X>(std::move(*this)), expected)) {
    return AddAnnotation<ExpectedBuilderDescription>(std::move(*this),
                                                     expected);
  }
};

constexpr auto MyDomain =
    Domain<MyPolicy, MyBuilder>().With(DescriptionBuilder());

TEST(PolicyDescription, Nothing) {
  auto F = [] {
    static constexpr auto& MErrorDomain = MyDomain;
    return MERROR().CheckPolicyDescription(std::string_view());
  };
  F();
}

TEST(PolicyDescription, StringLiteral) {
  auto F = [] {
    static constexpr auto MErrorDomain = MyDomain << "hello";
    return MERROR().CheckPolicyDescription("hello");
  };
  F();
}

TEST(PolicyDescription, TwoStringLiterals) {
  auto F = [] {
    static constexpr auto MErrorDomain = MyDomain << "hello"
                                                  << " world";
    return MERROR().CheckPolicyDescription("hello world");
  };
  F();
}

TEST(PolicyDescription, ConstexprStringView) {
  auto F = [] {
    constexpr std::string_view s = "hello";
    // Cannot be constexpr because -c opt uses gcc diagnoses.
    static const auto MErrorDomain = MyDomain << s << " world " << s;
    return MERROR().CheckPolicyDescription("hello world hello");
  };
  F();
}

TEST(PolicyDescription, NoConstExprStringView) {
  auto F = [] {
    std::string s("42");
    static const auto MErrorDomain = MyDomain << std::string_view(s);
    return MERROR().CheckPolicyDescription("42");
  };
  F();
}

TEST(PolicyDescription, NoConstExpConstCharPtr) {
  auto F = [] {
    static const auto MErrorDomain = MyDomain << std::string("hello").c_str();
    return MERROR().CheckPolicyDescription("hello");
  };
  F();
}

TEST(PolicyDescription, String) {
  auto F = [] {
    static const auto MErrorDomain = MyDomain << std::string("hello");
    return MERROR().CheckPolicyDescription("hello");
  };
  F();
}

TEST(PolicyDescription, TwoInts) {
  auto F = [] {
    static constexpr auto MErrorDomain = MyDomain << 1 << std::hex << 16
                                                  << std::endl;
    return MERROR().CheckPolicyDescription("110\n");
  };
  F();
}

TEST(PolicyDescription, LazyDump) {
  auto F = [] {
    int a = 1;
    static const auto MErrorDomain =
        MyDomain << MakeLazyStream([&](std::ostream& o) { o << a; });
    MERROR().CheckPolicyDescription("1");
    a = 3;
    MERROR().CheckPolicyDescription("3");
  };
  F();
}

TEST(PolicyDescription, Cref) {
  auto F = [] {
    int a = 1;
    static const auto MErrorDomain = MyDomain << std::cref(a);
    MERROR().CheckPolicyDescription("1");
    a = 3;
    MERROR().CheckPolicyDescription("3");
  };
  F();
}

TEST(PolicyDescription, NoCref) {
  auto F = [] {
    int a = 1;
    static const auto MErrorDomain = MyDomain << a;
    MERROR().CheckPolicyDescription("1");
    a = 3;
    MERROR().CheckPolicyDescription("1");
  };
  F();
}

TEST(PolicyDescription, DumpVars) {
  auto F = [] {
    int a = 1;
    static const auto MErrorDomain = MyDomain << [&](auto& o) { o << a; };
    MERROR().CheckPolicyDescription("1");
    a = 3;
    MERROR().CheckPolicyDescription("3");
  };
  F();
}

TEST(PolicyDescription, Nested) {
  static constexpr auto MErrorDomain = MyDomain << "hello"
                                                << " world!";
  auto F = [] {
    int a = 1;
    MERROR_DOMAIN() << [&](auto& o) { o << a; };
    MERROR().CheckPolicyDescription("hello world!\n1");
    a = 3;
    MERROR().CheckPolicyDescription("hello world!\n3");
  };
  F();
}

TEST(PolicyDescription, NestedTwo) {
  static constexpr auto MErrorDomain = MyDomain << "hello"
                                                << " world!";
  auto F = [] {
    int a = 1;
    MERROR_DOMAIN() << [&](auto& o) { o << a; };
    {
      MERROR_DOMAIN() << "dumb "
                      << " thing";
      MERROR().CheckPolicyDescription("hello world!\n1\ndumb  thing");
      a = 3;
      MERROR().CheckPolicyDescription("hello world!\n3\ndumb  thing");
    }
  };
  F();
}

TEST(PolicyDescription, Rvalue) {
  static constexpr auto MErrorDomain = MyDomain.GetCopy() << "hello"
                                                          << " world!";
  auto F = [] {
    int a = 1;
    MERROR_DOMAIN().GetCopy() << [&](auto& o) { o << a; };
    MERROR().CheckPolicyDescription("hello world!\n1");
    a = 3;
    MERROR().CheckPolicyDescription("hello world!\n3");
  };
  F();
}

TEST(BuilderDescription, Nothing) {
  auto F = [] {
    static constexpr auto& MErrorDomain = MyDomain;
    return MERROR().ExpectBuilderDescription(std::string_view());
  };
  F();
}

TEST(BuilderDescription, Something) {
  auto F = [] {
    static constexpr auto& MErrorDomain = MyDomain;
    return MERROR().ExpectBuilderDescription("42") << 4 << 2;
  };
  F();
}

TEST(BuilderDescription, Manipulators) {
  auto F = [] {
    static constexpr auto& MErrorDomain = MyDomain;
    return MERROR().ExpectBuilderDescription("10\n")
           << std::hex << 16 << std::endl;
  };
  F();
}

TEST(BuilderDescription, LazyBuilder) {
  auto F = [] {
    auto Fail = [] {
      ADD_FAILURE();
      return 0;
    };
    static constexpr auto& MErrorDomain = MyDomain;
    MVERIFY(true) << Fail();
  };
  F();
}

}  // namespace
}  // namespace merror
