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

#include "merror/domain/print_operands.h"

#include <sstream>
#include <string>

#include "merror/domain/base.h"
#include "merror/domain/bool.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/return.h"
#include "merror/macros.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace merror {
namespace {

using ::testing::MatchesRegex;

template <class Base>
struct MakeString : Hook<Base> {
  template <class Culprit>
  std::string MakeMError(ResultType<std::string>, const Culprit&) const {
    std::ostringstream strm;
    if (const RelationalExpression* rel_expr = this->context().rel_expr)
      strm << *rel_expr;
    return strm.str();
  }
};

template <class Base>
struct Print : Base {
  void PrintTo(int obj, std::ostream* strm) const { *strm << obj; }
  void PrintTo(double obj, std::ostream* strm) const { *strm << obj; }
  void PrintTo(const void* obj, std::ostream* strm) const { *strm << obj; }
  void PrintTo(std::string_view obj, std::ostream* strm) const { *strm << obj; }
};

template <class T, class U>
std::string Stringize(const T& t, const U& u) {
  constexpr auto MErrorDomain =
      EmptyDomain()
          .With(MethodHooks(), Return(), AcceptBool(), PrintOperands(),
                Domain<Print, Print>(), Builder<MakeString>())
          .Return<std::string>();
  MVERIFY(t == u);
  return "equal";
}

struct Unprintable {
  template <class T>
  friend bool operator==(Unprintable, const T&) {
    return false;
  }
  template <class T>
  friend bool operator==(const T&, Unprintable) {
    return false;
  }
};

TEST(Bool, Works) {
  EXPECT_EQ("42 == 3.5", Stringize(42, 3.5));
  std::string abc = "abc";
  const char* xyz = "xyz";
  EXPECT_THAT(Stringize(abc.c_str(), xyz),
              MatchesRegex("0x[0-9a-f]+ == 0x[0-9a-f]+"));
  EXPECT_EQ("abc == xyz", Stringize(abc, xyz));
  EXPECT_EQ("xyz == abc", Stringize(xyz, abc));
  EXPECT_EQ("", Stringize(42, Unprintable()));
  EXPECT_EQ("", Stringize(Unprintable(), 42));
}

}  // namespace
}  // namespace merror
