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
#include "merror/domain/print.h"

#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "merror/domain/base.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/return.h"
#include "merror/macros.h"
#include "gtest/gtest.h"

namespace merror {
namespace {

template <class Base>
struct MakeString : Hook<Base> {
  using Hook<Base>::MakeMError;
  template <class Culprit>
  std::string MakeMError(ResultType<std::string>,
                         const Culprit& culprit) const {
    std::ostringstream strm;
    this->derived().PrintTo(culprit, &strm);
    return strm.str();
  }
};

template <class Base>
struct AcceptAnything : Hook<Base> {
  using Hook<Base>::MVerify;
  template <class T>
  struct Acceptor {
    bool IsError() const { return true; }
    const T& GetCulprit() const { return val; }
    const T& val;
  };
  template <class R, class T>
  Acceptor<T> MVerify(Ref<R, T> val) const {
    return {val.Get()};
  }
};

constexpr auto MErrorDomain =
    EmptyDomain().With(MethodHooks(), Return(), Policy<AcceptAnything>(),
                       Builder<MakeString>(), Print());

template <class Domain, class T>
std::string Stringize(const Domain& MErrorDomain, const T& obj) {
  MVERIFY(obj);
  static_cast<void>(1);
}

struct Unprintable {
  char c;
};

TEST(Print, PrintTo) {
  const auto domain =
      EmptyDomain().With(MethodHooks(), Return(), Policy<AcceptAnything>(),
                         Builder<MakeString>(), Print());

  EXPECT_EQ("42", Stringize(domain, 42));
  EXPECT_EQ("", Stringize(domain, std::string_view("")));
  EXPECT_EQ("\"\n", Stringize(domain, std::string_view("\"\n")));
}

TEST(Print, CanPrint) {
  static_assert(CanPrint<Print, int>(), "");
  static_assert(!CanPrint<Print, Unprintable>(), "");
  static_assert(!CanPrint<EmptyDomain, int>(), "");
  static_assert(!CanPrint<EmptyDomain, Unprintable>(), "");
}

TEST(Print, TryPrint) {
  {
    std::ostringstream strm;
    auto can_print = TryPrint(Print(), 42, &strm);
    static_assert(can_print, "");
    EXPECT_EQ("42", strm.str());
  }
  {
    std::ostringstream strm;
    auto can_print = TryPrint(EmptyDomain(), 42, &strm);
    static_assert(!can_print, "");
    EXPECT_EQ("", strm.str());
  }
}

}  // namespace
}  // namespace merror
