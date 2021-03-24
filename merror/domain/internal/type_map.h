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
//
// The TypeMap data structure is similar to std::tuple except that elements are
// keyed by types instead of integers.
//
// The only way to create a TypeMap is to start with an empty one.
//
//   auto m = EmptyTypeMap();
//
// A TypeMap can be extended with `Add()` and queried with `Get()`, `GetOr()`
// and `GetAll()`. The existence of a key in a TypeMap can be checked with
// metafunction `Has<>`. Several instances of a TypeMap can be merged into one
// with `Merge()`.
//
// All accessors accept any number of type maps and behave as if all passed
// type maps were first merged.
//
//   auto m1 = EmptyTypeMap();
//   auto m2 = Add<void>(EmptyTypeMap(), 42);
//   // The same as Get<void>(Merge(m1, m2)) but without the copying.
//   int n = Get<void>(m1, m2);
//   assert(n == 42);
//
// This file reimplements several algorithms from //util/tuple for two reasons:
// 1. (Minor) Algorithms in //util/tuple aren't constexpr.
// 2. (Major) To avoid dependency on code outside of components.

#ifndef MERROR_5EDA97_DOMAIN_INTERNAL_TYPE_MAP_H_
#define MERROR_5EDA97_DOMAIN_INTERNAL_TYPE_MAP_H_

#include <stddef.h>

#include <tuple>
#include <type_traits>
#include <utility>

namespace merror {
namespace internal {
namespace type_map {

namespace internal_type_map {

// All symbols defined within namespace internal_type_map are internal to
// type_map.h. Do not reference them from other files.

template <bool C>
using EnableIf = typename ::std::enable_if<C, int>::type;

// Unlike in std::pair, the key here is a type without value.
template <class Key, class Value>
struct Pair {
  using key_type = Key;
  using mapped_type = Value;

  Value value;
};

template <class Key, class Value>
constexpr Pair<Key, typename std::decay<Value>::type> MakePair(Value&& value) {
  return {std::forward<Value>(value)};
}

template <class F, class Tuple, size_t... Is>
constexpr auto ApplyImpl(F&& f, Tuple&& tuple, std::index_sequence<Is...>)
    -> decltype(std::forward<F>(f)(
        std::get<Is>(std::forward<Tuple>(tuple))...)) {
  return std::forward<F>(f)(std::get<Is>(std::forward<Tuple>(tuple))...);
}

// Apply(F, forward_as_tuple(args...)) evaluates to F(args...).
template <class F, class Tuple>
constexpr auto Apply(F&& f, Tuple&& tuple) -> decltype(ApplyImpl(
    std::forward<F>(f), std::forward<Tuple>(tuple),
    std::make_index_sequence<
        std::tuple_size<typename std::decay<Tuple>::type>::value>())) {
  return ApplyImpl(
      std::forward<F>(f), std::forward<Tuple>(tuple),
      std::make_index_sequence<
          std::tuple_size<typename std::decay<Tuple>::type>::value>());
}

template <class T>
using IsReplicable = std::is_constructible<typename std::decay<T>::type, T>;

template <class Key, class Value>
struct PushFront {
  Value&& value;

  // Copies all arguments and the value.
  template <class... Elems,
            EnableIf<std::conjunction<IsReplicable<Value>,
                                      IsReplicable<Elems>...>::value> = 0>
  constexpr auto operator()(Elems&&... elems) const
      -> decltype(std::make_tuple(MakePair<Key>(std::forward<Value>(value)),
                                  std::forward<Elems>(elems)...)) {
    return std::make_tuple(MakePair<Key>(std::forward<Value>(value)),
                           std::forward<Elems>(elems)...);
  }
};

// Pair<Key, RemoveTag> in a TypeMap indicates that all values with the given
// key past this one should be ignored.
struct RemoveTag {};

// std::forward_as_tuple isn't constexpr in C++11, so we have to reimplement it.
template <class... Ts>
constexpr std::tuple<Ts&&...> ForwardAsTuple(Ts&&... ts) {
  return std::tuple<Ts&&...>(std::forward<Ts>(ts)...);
}

// TODO(romanp): the compile-time efficiency of this implementation depends on
// the quality of std::tuple_cat. Check whether the latter has decent
// implementation.
template <class Key>
struct GetAllImpl {
  template <class Elem, class... Elems,
            EnableIf<!std::is_same<
                Key, typename std::decay<Elem>::type::key_type>::value> = 0,
            class Self = GetAllImpl>
  constexpr auto operator()(Elem&& elem, Elems&&... elems) const
      -> decltype(Self()(std::forward<Elems>(elems)...)) {
    return Self()(std::forward<Elems>(elems)...);
  }

  template <
      class Elem, class... Elems,
      EnableIf<
          std::is_same<Key, typename std::decay<Elem>::type::key_type>::value &&
          std::is_same<RemoveTag, typename std::decay<
                                      Elem>::type::mapped_type>::value> = 0>
  constexpr std::tuple<> operator()(Elem&& elem, Elems&&... elems) const {
    return {};
  }

  template <
      class Elem, class... Elems,
      EnableIf<
          std::is_same<Key, typename std::decay<Elem>::type::key_type>::value &&
          !std::is_same<RemoveTag, typename std::decay<
                                       Elem>::type::mapped_type>::value> = 0,
      class Self = GetAllImpl>
  constexpr auto operator()(Elem&& elem, Elems&&... elems) const
      -> decltype(std::tuple_cat(ForwardAsTuple(std::forward<Elem>(elem).value),
                                 Self()(std::forward<Elems>(elems)...))) {
    return std::tuple_cat(ForwardAsTuple(std::forward<Elem>(elem).value),
                          Self()(std::forward<Elems>(elems)...));
  }

  constexpr std::tuple<> operator()() const { return {}; }
};

template <class T>
struct PushBack {
  T&& val;

  // Copies all arguments and the value.
  template <class... Ts>
  constexpr auto operator()(Ts&&... ts) const
      -> decltype(ForwardAsTuple(std::forward<Ts>(ts)...,
                                 std::forward<T>(val))) {
    return ForwardAsTuple(std::forward<Ts>(ts)..., std::forward<T>(val));
  }
};

// TODO(romanp): this implementation can be O(N^2) in runtime (e.g., if inlining
// doesn't work). Make it O(N).
struct Reverse {
  constexpr std::tuple<> operator()() const { return {}; }

  template <class T, class... Ts, class Self = Reverse>
  constexpr auto operator()(T&& t, Ts&&... ts) const
      -> decltype(Apply(PushBack<T>{std::forward<T>(t)},
                        Self()(std::forward<Ts>(ts)...))) {
    return Apply(PushBack<T>{std::forward<T>(t)},
                 Self()(std::forward<Ts>(ts)...));
  }
};

struct Cat {
  template <class... Tuples>
  constexpr auto operator()(Tuples&&... tuples) const
      -> decltype(std::tuple_cat(std::forward<Tuples>(tuples)...)) {
    return std::tuple_cat(std::forward<Tuples>(tuples)...);
  }
};

template <class Tuple>
constexpr auto Join(Tuple&& tuple)
    -> decltype(Apply(Cat(), std::forward<Tuple>(tuple))) {
  return Apply(Cat(), std::forward<Tuple>(tuple));
}

}  // namespace internal_type_map

using EmptyTypeMap = std::tuple<>;

// Does any of the maps have an element with the specified key?
template <class Key, class... Maps>
struct Has : std::disjunction<Has<Key, Maps>...> {};

template <class Key>
struct Has<Key> : std::false_type {};

template <class Key, class Map>
struct Has<Key, Map> : Has<Key, typename std::decay<Map>::type> {};

template <class Key>
struct Has<Key, EmptyTypeMap> : std::false_type {};

template <class Key, class K, class V, class... Keys, class... Values>
struct Has<Key, std::tuple<internal_type_map::Pair<K, V>,
                           internal_type_map::Pair<Keys, Values>...>>
    : std::integral_constant<
          bool,
          std::is_same<K, Key>::value
              ? !std::is_same<V, internal_type_map::RemoveTag>::value
              : Has<Key, std::tuple<internal_type_map::Pair<Keys, Values>...>>::
                    value> {};

// Returns a copy of the original map with the copy of the value added under the
// specified key. If there are already elements with the same key, they are
// kept. Get<Key>() and GetOr<Key>() will return the newest added value for the
// specified key.
template <class Key, class Map, class Value>
constexpr auto Add(Map&& map, Value&& value)
    -> decltype(internal_type_map::Apply(
        internal_type_map::PushFront<Key, Value>{std::forward<Value>(value)},
        std::forward<Map>(map))) {
  return internal_type_map::Apply(
      internal_type_map::PushFront<Key, Value>{std::forward<Value>(value)},
      std::forward<Map>(map));
}

// Returns a copy of the original map without any pair (key, value) for
// the specified key.
template <class Key, class Map>
constexpr auto RemoveAll(Map&& map)
    -> decltype(Add<Key>(std::forward<Map>(map),
                         internal_type_map::RemoveTag())) {
  return Add<Key>(std::forward<Map>(map), internal_type_map::RemoveTag());
}

// Returns a tuple of references to all values with the specified key in reverse
// order of their addition.
template <class Key, class Map>
constexpr auto GetAll(Map&& map)
    -> decltype(internal_type_map::Apply(internal_type_map::GetAllImpl<Key>(),
                                         std::forward<Map>(map))) {
  return internal_type_map::Apply(internal_type_map::GetAllImpl<Key>(),
                                  std::forward<Map>(map));
}

template <class Key, class... Maps>
constexpr auto GetAll(Maps&&... maps)
    -> decltype(std::tuple_cat(GetAll<Key>(std::forward<Maps>(maps))...)) {
  return std::tuple_cat(GetAll<Key>(std::forward<Maps>(maps))...);
}

// Equivalent to std::get<0>(GetAll<Key>(maps...)).
//
// TODO(romanp): implement this in a more efficient manner.
template <class Key, class... Maps>
constexpr auto Get(Maps&&... maps)
    -> decltype(std::get<0>(GetAll<Key>(std::forward<Maps>(maps)...))) {
  return std::get<0>(GetAll<Key>(std::forward<Maps>(maps)...));
}

namespace internal_type_map {

template <class Key>
struct GetOrImpl {
  template <class Value>
  constexpr Value&& operator()(Value&& value) const {
    return std::forward<Value>(value);
  }

  template <class First, class Second, class... Rest,
            EnableIf<Has<Key, First>{}> = 0>
  constexpr auto operator()(First&& first, Second&& second,
                            Rest&&... rest) const
      -> decltype(Get<Key>(std::forward<First>(first))) {
    return Get<Key>(std::forward<First>(first));
  }

  template <class First, class Second, class... Rest,
            EnableIf<!Has<Key, First>{}> = 0, class Self = GetOrImpl>
  constexpr auto operator()(First&& first, Second&& second,
                            Rest&&... rest) const
      -> decltype(Self()(std::forward<Second>(second),
                         std::forward<Rest>(rest)...)) {
    return Self()(std::forward<Second>(second), std::forward<Rest>(rest)...);
  }
};

}  // namespace internal_type_map

// The last argument is the default value. The rest are type maps.
// If Has<Key>(maps...), returns Get<Key>(maps...). Otherwise returns the
// default.
template <class Key, class... Rest>
constexpr auto GetOr(Rest&&... rest) -> decltype(
    internal_type_map::GetOrImpl<Key>()(std::forward<Rest>(rest)...)) {
  return internal_type_map::GetOrImpl<Key>()(std::forward<Rest>(rest)...);
}

// Merges multiple maps into one. The order of elements is as you expect:
//
//   auto a = Add<void>(EmptyTypeMap(), 1);
//   auto b = Add<void>(EmptyTypeMap(), 2);
//   auto merged = Merge(a, b);
//   assert(merged == Add<void>(a, 2));
//
// More rigorously:
//
//   Merge(EmptyTypemap()$Add<K1>(v1)$...$Add<KN>(vN),
//         EmptyTypemap()$Add<L1>(u1)$...$Add<LM>(uM))
//
// is equivalent to
//
//   EmptyTypemap()$Add<K1>(v1)$...$Add<KN>(vN)$Add<L1>(u1)$...$Add<LM>(uM)
//
// Where m$Add<K>(v) is a shorthand for Add<K>(m, v).
template <class... Maps>
constexpr auto Merge(Maps&&... maps) -> decltype(internal_type_map::Join(
    internal_type_map::Reverse()(std::forward<Maps>(maps)...))) {
  return internal_type_map::Join(
      internal_type_map::Reverse()(std::forward<Maps>(maps)...));
}

}  // namespace type_map
}  // namespace internal
}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_INTERNAL_TYPE_MAP_H_
