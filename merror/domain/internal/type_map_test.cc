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

#include "merror/domain/internal/type_map.h"

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace merror {
namespace internal {
namespace type_map {
namespace {

TEST(TypeMap, Empty) {
  static_assert(!Has<int, EmptyTypeMap>(), "");
  EXPECT_EQ('A', GetOr<int>(EmptyTypeMap(), 'A'));
  EXPECT_EQ(std::make_tuple(), GetAll<int>(EmptyTypeMap()));
}

TEST(TypeMap, OneElement) {
  auto t = Add<int>(EmptyTypeMap(), 'A');
  static_assert(Has<int, decltype(t)>(), "");
  static_assert(!Has<char, decltype(t)>(), "");
  EXPECT_EQ('A', Get<int>(t));
  EXPECT_EQ('A', GetOr<int>(t, std::string("B")));
  EXPECT_EQ("B", GetOr<char>(t, std::string("B")));
  EXPECT_EQ(std::make_tuple('A'), GetAll<int>(t));
  EXPECT_EQ(std::make_tuple(), GetAll<char>(t));
}

TEST(TypeMap, TwoElements) {
  auto t = Add<float>(Add<int>(EmptyTypeMap(), 'A'), std::string("A"));
  static_assert(Has<int, decltype(t)>(), "");
  static_assert(Has<float, decltype(t)>(), "");
  static_assert(!Has<char, decltype(t)>(), "");
  EXPECT_EQ('A', Get<int>(t));
  EXPECT_EQ('A', GetOr<int>(t, ""));
  EXPECT_EQ("A", Get<float>(t));
  EXPECT_EQ("A", GetOr<float>(t, 42));
  EXPECT_EQ("B", GetOr<char>(t, std::string("B")));
}

TEST(TypeMap, Override) {
  auto t = Add<int>(Add<int>(EmptyTypeMap(), "A"), 'A');
  static_assert(Has<int, decltype(t)>(), "");
  EXPECT_EQ('A', Get<int>(t));
  EXPECT_EQ('A', GetOr<int>(t, std::string("B")));
  EXPECT_THAT(GetAll<int>(t), std::make_tuple('A', std::string("A")));
}

TEST(TypeMap, RemoveAll) {
  auto t =
      Add<float>(Add<int>(Add<int>(EmptyTypeMap(), std::string("A")), 'A'), 5);
  static_assert(Has<int, decltype(t)>(), "");
  static_assert(Has<float, decltype(t)>(), "");
  auto t_wo_int = RemoveAll<int>(t);
  auto t_wo_float = RemoveAll<float>(t);
  static_assert(Has<float, decltype(t_wo_int)>::value, "");
  static_assert(!Has<int, decltype(t_wo_int)>::value, "");
  static_assert(Has<int, decltype(t_wo_float)>::value, "");
  static_assert(!Has<float, decltype(t_wo_float)>::value, "");
  EXPECT_EQ('A', Get<int>(t_wo_float));
  EXPECT_THAT(GetAll<int>(t_wo_float), std::make_tuple('A', std::string("A")));
  EXPECT_EQ(5, Get<float>(t_wo_int));
  auto t_back_with_int = Add<int>(t_wo_int, std::string("Up"));
  EXPECT_THAT(GetAll<int>(t_back_with_int), std::make_tuple(std::string("Up")));
}

TEST(TypeMap, RemoveAllValueCategory) {
  auto t = Add<float>(Add<int>(EmptyTypeMap(), std::make_unique<int>(42)),
                      std::make_unique<int>(13));
  static_assert(Has<int, decltype(t)>(), "");
  static_assert(Has<float, decltype(t)>(), "");
  // RemoveAll also consumes all rvalues.
  auto t_wo_int = RemoveAll<int>(std::move(t));
  static_assert(!Has<int, decltype(t_wo_int)>::value, "");
  EXPECT_EQ(13, *Get<float>(t_wo_int));
  EXPECT_EQ(nullptr, Get<float>(t));
  EXPECT_EQ(nullptr, Get<int>(t));
}

template <class Expected, class Actual>
void SameType() {
  static_assert(std::is_same<Expected, Actual>(), "");
}

TEST(TypeMap, GetValueCategory) {
  auto t = Add<int>(EmptyTypeMap(), 'A');
  const auto& c = t;
  EXPECT_EQ('A', Get<int>(t));
  EXPECT_EQ('A', Get<int>(c));
  EXPECT_EQ('A', Get<int>(std::move(t)));
  EXPECT_EQ('A', Get<int>(std::move(c)));
  SameType<char&, decltype(Get<int>(t))>();
  SameType<const char&, decltype(Get<int>(c))>();
  SameType<char&&, decltype(Get<int>(std::move(t)))>();
  SameType<const char&&, decltype(Get<int>(std::move(c)))>();
}

TEST(TypeMap, GetOrValueCategory) {
  auto t = Add<int>(EmptyTypeMap(), 'A');
  {
    const auto& c = t;
    EXPECT_EQ('A', GetOr<int>(t, 0));
    EXPECT_EQ('A', GetOr<int>(c, 0));
    EXPECT_EQ('A', GetOr<int>(std::move(t), 0));
    EXPECT_EQ('A', GetOr<int>(std::move(c), 0));
    SameType<char&, decltype(GetOr<int>(t, 0))>();
    SameType<const char&, decltype(GetOr<int>(c, 0))>();
    SameType<char&&, decltype(GetOr<int>(std::move(t), 0))>();
    SameType<const char&&, decltype(GetOr<int>(std::move(c), 0))>();
  }

  {
    int n = 42;
    const int c = 42;
    EXPECT_EQ(42, GetOr<void>(t, n));
    EXPECT_EQ(42, GetOr<void>(t, c));
    EXPECT_EQ(42, GetOr<void>(t, std::move(n)));
    EXPECT_EQ(42, GetOr<void>(t, std::move(c)));
    SameType<int&, decltype(GetOr<void>(t, n))>();
    SameType<const int&, decltype(GetOr<void>(t, c))>();
    SameType<int&&, decltype(GetOr<void>(t, std::move(n)))>();
    SameType<const int&&, decltype(GetOr<void>(t, std::move(c)))>();
  }
}

TEST(TypeMap, GetAllValueCategory) {
  auto t = Add<int>(EmptyTypeMap(), 'A');
  const auto& c = t;
  EXPECT_EQ(std::make_tuple('A'), GetAll<int>(t));
  EXPECT_EQ(std::make_tuple('A'), GetAll<int>(c));
  EXPECT_EQ(std::make_tuple('A'), GetAll<int>(std::move(t)));
  EXPECT_EQ(std::make_tuple('A'), GetAll<int>(std::move(c)));
  SameType<std::tuple<char&>, decltype(GetAll<int>(t))>();
  SameType<std::tuple<const char&>, decltype(GetAll<int>(c))>();
  SameType<std::tuple<char&&>, decltype(GetAll<int>(std::move(t)))>();
  SameType<std::tuple<const char&&>, decltype(GetAll<int>(std::move(c)))>();
}

TEST(TypeMap, VariadicAccessors) {
  auto a = Add<int>(EmptyTypeMap(), 'A');
  auto b = Add<int>(EmptyTypeMap(), std::string("B"));
  auto e = EmptyTypeMap();
  using A = decltype(a);
  using B = decltype(b);
  using E = decltype(e);

  static_assert(!Has<int>(), "");
  EXPECT_EQ(42, GetOr<int>(42));
  EXPECT_EQ(std::make_tuple(), GetAll<int>());

  static_assert(!Has<int, E, E>(), "");
  EXPECT_EQ(42, GetOr<int>(e, e, 42));
  EXPECT_EQ(std::make_tuple(), GetAll<int>(e, e));

  static_assert(Has<int, E, A>(), "");
  EXPECT_EQ('A', GetOr<int>(e, a, 42));
  EXPECT_EQ(std::make_tuple('A'), GetAll<int>(e, a));

  static_assert(Has<int, A, E>(), "");
  EXPECT_EQ('A', GetOr<int>(a, e, 42));
  EXPECT_EQ(std::make_tuple('A'), GetAll<int>(a, e));

  static_assert(Has<int, A, A>(), "");
  EXPECT_EQ('A', GetOr<int>(a, a, 42));
  EXPECT_EQ(std::make_tuple('A', 'A'), GetAll<int>(a, a));

  static_assert(Has<int, A, B>(), "");
  EXPECT_EQ('A', GetOr<int>(a, b, 42));
  EXPECT_THAT(GetAll<int>(a, b), std::make_tuple('A', std::string("B")));
}

TEST(TypeMap, Merge) {
  auto ab = Add<void>(Add<int>(EmptyTypeMap(), 'A'), std::string("B"));
  auto c = Add<int>(EmptyTypeMap(), 'C');
  auto e = EmptyTypeMap();

  EXPECT_THAT(GetAll<int>(Merge()), std::make_tuple());
  EXPECT_THAT(GetAll<int>(Merge(e)), std::make_tuple());
  EXPECT_THAT(GetAll<int>(Merge(e, e)), std::make_tuple());
  auto m1 = Merge(c);
  EXPECT_THAT(GetAll<int>(m1), std::make_tuple('C'));
  auto m2 = Merge(ab, c);
  EXPECT_THAT(GetAll<int>(m2), std::make_tuple('C', 'A'));
  EXPECT_THAT(GetAll<void>(m2), std::make_tuple(std::string("B")));
}

TEST(TypeMap, Constexpr) {
  constexpr auto e = EmptyTypeMap();
  constexpr auto a = Add<int>(e, 'A');
  constexpr auto b = Add<float>(e, 'B');
  constexpr auto c = RemoveAll<int>(a);
  constexpr auto ab = Merge(a, b);
  {
    constexpr auto x = Get<int>(ab);
    (void)x;
  }
  {
    constexpr auto x = Get<float>(ab);
    (void)x;
  }
  {
    constexpr auto x = Get<int>(a, b, c);
    (void)x;
  }
  {
    constexpr auto x = Get<float>(a, b, c);
    (void)x;
  }
  {
    constexpr auto x = GetOr<int>(ab, "");
    (void)x;
  }
  {
    constexpr auto x = GetOr<float>(ab, "");
    (void)x;
  }
  {
    constexpr auto x = GetOr<int>(a, b, c, "");
    (void)x;
  }
  {
    constexpr auto x = GetOr<float>(a, b, c, "");
    (void)x;
  }
  {
    constexpr auto x = GetOr<void>("");
    (void)x;
  }
  {
    constexpr auto x = GetOr<void>(a, "");
    (void)x;
  }
  {
    constexpr auto x = GetOr<void>(a, b, "");
    (void)x;
  }
  {
    constexpr auto x = GetOr<void>(a, b, c, "");
    (void)x;
  }
  {
    constexpr auto x = GetAll<int>();
    (void)x;
  }
  {
    constexpr auto x = GetAll<int>(e);
    (void)x;
  }
  {
    constexpr auto x = GetAll<int>(b);
    (void)x;
  }
  {
    constexpr auto x = GetAll<int>(c);
    (void)x;
  }
}

}  // namespace
}  // namespace type_map
}  // namespace internal
}  // namespace merror
