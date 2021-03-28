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
// This library defines `DescriptionBuilder()`. A merror domain extension that
// enables streaming of objects into merror policies and builders. It is a part
// of `merror::MErrorDomain()`.
//
// Whatever has been streamed can be retrieved with `GetPolicyDescription()` and
// `GetBuilderDescription()`. These functions are meant to be used from other
// extensions for logging and constructing error messages.
//
//   Status DoStuff(int x, const MyProto& y) {
//     static constexpr auto MErrorDomain =
//         merror::MErrorDomain() << "In DoStuff()";
//     MVERIFY(x >= 0) << "Not cool: " << y.ShortDebugString();
//     ...
//   }
//
// In the example above, when the error condition triggers,
// `GetPolicyDescription()` returns "In DoStuff()" and `GetBuilderDescription()`
// returns "Not cool: <...>".

#ifndef MERROR_5EDA97_DOMAIN_DESCRIPTION_H_
#define MERROR_5EDA97_DOMAIN_DESCRIPTION_H_

#include <memory>
#include <ostream>
#include <streambuf>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "merror/domain/base.h"
#include "merror/domain/defer.h"
#include "merror/domain/internal/stringstream.h"

namespace merror {

namespace internal_streamable {

template <class T,
          typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
constexpr T Streamable(T a) {
  return a;
}

template <class T>
std::reference_wrapper<T> Streamable(std::reference_wrapper<T> a) {
  return a;
}

template <class T>
using IOManip = T& (*)(T&);

template <class T>
constexpr IOManip<T> Streamable(IOManip<T> manip) {
  return manip;
}

inline std::string&& Streamable(std::string&& s) { return std::move(s); }

inline std::string Streamable(std::string_view s) { return std::string(s); }

inline std::string Streamable(const char* s) { return std::string(s); }

template <class T>
struct LazyStream {
  T f;
  friend std::ostream& operator<<(std::ostream& o, const LazyStream& lazy) {
    lazy.f(o);
    return o;
  }
};

template <class T,
          class E = decltype(std::declval<T&>()(std::declval<std::ostream&>()))>
constexpr LazyStream<std::decay_t<T>> Streamable(T&& t) {
  return {std::forward<T>(t)};
}

template <class T, class R = decltype(GStreamable(std::declval<T>()))>
constexpr R Streamable(T&& t) {
  return GStreamable(std::forward<T>(t));
}

}  // namespace internal_streamable

// A builder annotation with this key, if present, contains the error
// description written to the policy. The annotation is explicitly convertible
// to `StringPiece`.
struct PolicyDescriptionAnnotation {};

// A builder annotation with this key, if present, contains the error
// description written to the builder. The annotation is explicitly convertible
// to `StringPiece`.
struct BuilderDescriptionAnnotation {};

// Returns whatever has been streamed into the policy or null if nothing has
// been streamed. The argument must be a builder (!) not policy. It can be
// called even if the policy doesn't support streaming.
template <class Builder>
std::string_view GetPolicyDescription(const Builder& b) {
  return GetAnnotationOr<PolicyDescriptionAnnotation>(b, std::string_view());
}

// Returns whatever has been streamed into the builder or null if nothing has
// been streamed. It can be called even if the builder doesn't support
// streaming.
template <class Builder>
std::string_view GetBuilderDescription(const Builder& builder) {
  return std::string_view(GetAnnotationOr<BuilderDescriptionAnnotation>(
      builder, std::string_view()));
}

namespace internal_description {

// Copy-pasted from util/gtl/labs/enable_if.h to avoid a dependency on labs.
template <bool C>
using EnableIf = typename ::std::enable_if<C, int>::type;

// Annotations with this key are written to the policy. There may be more than
// one. Values are ostreamable objects.
struct StreamableAnnotation {};

// Key and value for identifying when new line must be added.
// Transforms to true on copy.
struct StreamableAnnotationMarker {
  constexpr StreamableAnnotationMarker() {}
  constexpr StreamableAnnotationMarker(const StreamableAnnotationMarker&)
      : copied(true) {}
  constexpr StreamableAnnotationMarker(StreamableAnnotationMarker&&) = default;
  const bool copied = false;
};
#if defined(__clang__)
#if __has_extension(is_trivially_constructible)
static_assert(__is_trivially_constructible(StreamableAnnotationMarker,
                                           StreamableAnnotationMarker&&),
              "");
#endif
#endif
// Whether to add a new line or not.
struct NewLine {
  const bool engaged;
  friend std::ostream& operator<<(std::ostream& o, NewLine s) {
    if (s.engaged) o << std::endl;
    return o;
  }
};

template <size_t I>
struct Print {
  template <class Tuple>
  void operator()(const Tuple& t, std::ostream* strm) const {
    *strm << std::get<I - 1>(t);
    Print<I - 1>()(t, strm);
  }
};

template <>
struct Print<0> {
  template <class Tuple>
  void operator()(const Tuple& t, std::ostream* strm) const {}
};

// Produces the value of `PolicyDescriptionAnnotation` from all instances of
// `StreamableAnnotation`.
template <class Policy>
std::string MakePolicyDescription(const Policy& policy) {
  std::string buffer;
  internal::StringStream strm(&buffer);
  // It's a tuple of references to everything that has been streamed into the
  // policy. The elements are in the reverse order.
  auto values = GetAnnotations<StreamableAnnotation>(policy);
  Print<std::tuple_size<decltype(values)>::value>()(values, &strm);
  return buffer;
}

// This policy extension allows objects to be streamed into merror policies.
template <class Base>
struct Policy : Base {
  // This overload kicks in if something has been streamed into `*this`.
  // It creates a builder with `PolicyDescriptionAnnotation` in it.
  template <class Context, class Self = Policy,
            EnableIf<HasAnnotation<StreamableAnnotation, Self>()> = 0>
  auto GetErrorBuilder(Context&& ctx) const
      -> decltype(AddAnnotation<PolicyDescriptionAnnotation>(
          Defer<Self>(this)->Base::GetErrorBuilder(std::move(ctx)),
          internal_description::MakePolicyDescription(*this))) {
    static_assert(!std::is_reference<Context>::value, "");
    return AddAnnotation<PolicyDescriptionAnnotation>(
        Base::GetErrorBuilder(std::move(ctx)),
        internal_description::MakePolicyDescription(*this));
  }

  // This overload kicks in if nothing has been streamed into `*this`.
  template <class Context, class Self = Policy,
            EnableIf<!HasAnnotation<StreamableAnnotation, Self>()> = 0>
  auto GetErrorBuilder(Context&& ctx) const
      -> decltype(Defer<Self>(this)->Base::GetErrorBuilder(std::move(ctx))) {
    static_assert(!std::is_reference<Context>::value, "");
    return Base::GetErrorBuilder(std::move(ctx));
  }
};

// The use while move in the arguments is ok for two reasons.
// - AddAnnotation captures by reference.
// - StreamableAnnotationMarker move operation is trivial.
template <class Base, class T>
constexpr auto AddStreamableAnnotationHelper(Policy<Base>&& p, T&& t)
    -> decltype(AddAnnotation<StreamableAnnotationMarker>(
        AddAnnotation<StreamableAnnotation>(
            AddAnnotation<StreamableAnnotation>(
                std::move(p),
                NewLine{GetAnnotationOr<StreamableAnnotationMarker>(
                            p, StreamableAnnotationMarker())
                            .copied}),
            std::forward<T>(t)),
        StreamableAnnotationMarker())) {
  return AddAnnotation<StreamableAnnotationMarker>(
      AddAnnotation<StreamableAnnotation>(
          AddAnnotation<StreamableAnnotation>(
              std::move(p), NewLine{GetAnnotationOr<StreamableAnnotationMarker>(
                                        p, StreamableAnnotationMarker())
                                        .copied}),
          std::forward<T>(t)),
      StreamableAnnotationMarker());
}

template <class Base, class T>
constexpr auto AddStreamableAnnotationHelper(const Policy<Base>& p, T&& t)
    -> decltype(AddAnnotation<StreamableAnnotationMarker>(
        AddAnnotation<StreamableAnnotation>(
            AddAnnotation<StreamableAnnotation>(
                p, NewLine{HasAnnotation<StreamableAnnotation>(p)}),
            std::forward<T>(t)),
        StreamableAnnotationMarker())) {
  return AddAnnotation<StreamableAnnotationMarker>(
      AddAnnotation<StreamableAnnotation>(
          AddAnnotation<StreamableAnnotation>(
              p, NewLine{HasAnnotation<StreamableAnnotation>(p)}),
          std::forward<T>(t)),
      StreamableAnnotationMarker());
}

#define MERROR_INTERNAL_USE_ATTRIBUTE_ENABLE_IF 0
#if defined(__clang__)
#if __has_attribute(enable_if)
#undef MERROR_INTERNAL_USE_ATTRIBUTE_ENABLE_IF
#define MERROR_INTERNAL_USE_ATTRIBUTE_ENABLE_IF 1
#endif
#endif

#if MERROR_INTERNAL_USE_ATTRIBUTE_ENABLE_IF
#define MERROR_INTERNAL_REQUIRE_CONSTEXPR_STRING(s) \
  __attribute__((enable_if(__builtin_strlen(s) + 1, \
                           "the argument must be a constexpr string")))
// String literals streamed into a policy are stored as pointers.
// MERROR_INTERNAL_REQUIRE_CONSTEXPR_STRING(s) ensures that the argument is a
// compile-time string.
template <class Base>
constexpr decltype(AddStreamableAnnotationHelper(
    std::declval<const Policy<Base>&>(), std::declval<const char*&>()))
operator<<(const Policy<Base>& p, const char* s)
    MERROR_INTERNAL_REQUIRE_CONSTEXPR_STRING(s) {
  return AddStreamableAnnotationHelper(p, s);
}

template <class Base>
constexpr decltype(AddStreamableAnnotationHelper(std::declval<Policy<Base>>(),
                                                 std::declval<const char*&>()))
operator<<(Policy<Base>&& p, const char* s)
    MERROR_INTERNAL_REQUIRE_CONSTEXPR_STRING(s) {
  return AddStreamableAnnotationHelper(std::move(p), s);
}

template <class Base>
constexpr decltype(AddStreamableAnnotationHelper(
    std::declval<const Policy<Base>&>(),
    std::declval<const std::string_view&>()))
operator<<(const Policy<Base>& p, const std::string_view& s)
    MERROR_INTERNAL_REQUIRE_CONSTEXPR_STRING(s.data()) {
  return AddStreamableAnnotationHelper(p, s);
}

template <class Base>
constexpr decltype(AddStreamableAnnotationHelper(
    std::declval<Policy<Base>>(), std::declval<const std::string_view&>()))
operator<<(Policy<Base>&& p, const std::string_view& s)
    MERROR_INTERNAL_REQUIRE_CONSTEXPR_STRING(s.data()) {
  return AddStreamableAnnotationHelper(std::move(p), s);
}
#undef MERROR_INTERNAL_REQUIRE_CONSTEXPR_STRING
#else
// String literals streamed into a policy are stored as pointers. In GCC
// we only take known size const char arrays.
template <class Base, size_t N>
constexpr decltype(AddStreamableAnnotationHelper(
    std::declval<const Policy<Base>&>(), std::declval<const char*>()))
operator<<(const Policy<Base>& p, const char (&s)[N]) {
  return AddStreamableAnnotationHelper(p, static_cast<const char*>(s));
}

template <class Base, size_t N>
constexpr decltype(AddStreamableAnnotationHelper(std::declval<Policy<Base>>(),
                                                 std::declval<const char*>()))
operator<<(Policy<Base>&& p, const char (&s)[N]) {
  return AddStreamableAnnotationHelper(std::move(p),
                                       static_cast<const char*>(s));
}
#endif
#undef MERROR_INTERNAL_USE_ATTRIBUTE_ENABLE_IF

template <class Base, class T>
constexpr decltype(AddStreamableAnnotationHelper(
    std::declval<const Policy<Base>&>(),
    internal_streamable::Streamable(std::declval<T>())))
operator<<(const Policy<Base>& p, T s) {
  return AddStreamableAnnotationHelper(
      p, internal_streamable::Streamable(std::move(s)));
}

template <class Base, class T>
constexpr decltype(AddStreamableAnnotationHelper(
    std::declval<Policy<Base>>(),
    internal_streamable::Streamable(std::declval<T>())))
operator<<(Policy<Base>&& p, T s) {
  return AddStreamableAnnotationHelper(
      std::move(p), internal_streamable::Streamable(std::move(s)));
}

// These overloads are just to deal with overloaded functions such as std::endl;
template <class Base>
constexpr decltype(AddStreamableAnnotationHelper(
    std::declval<const Policy<Base>&>(),
    std::declval<std::ostream& (*)(std::ostream&)>()))
operator<<(const Policy<Base>& p, std::ostream& (*manip)(std::ostream&)) {
  return AddStreamableAnnotationHelper(p, manip);
}

template <class Base>
constexpr decltype(AddStreamableAnnotationHelper(
    std::declval<Policy<Base>>(),
    std::declval<std::ostream& (*)(std::ostream&)>()))
operator<<(Policy<Base>&& p, std::ostream& (*manip)(std::ostream&)) {
  return AddStreamableAnnotationHelper(std::move(p), manip);
}

// This builder extension allows objects to be streamed into merror builders.
template <class Base>
struct Builder : Base {};

class BuilderStream {
 public:
  explicit operator std::string_view() const { return strm_->buffer; }
  std::ostream& strm() { return strm_->stream; }

 private:
  // StringStream isn't movable, so we store it on the heap.
  struct Stream {
    std::string buffer;
    internal::StringStream stream{&buffer};
  };
  std::unique_ptr<Stream> strm_{new Stream};
};

template <class Base, class T,
          EnableIf<HasAnnotation<BuilderDescriptionAnnotation, Base>()> = 0>
typename Base::BuilderType&& Write(Builder<Base>&& b, const T& val) {
  GetAnnotation<BuilderDescriptionAnnotation>(b).strm() << val;
  return std::move(b.derived());
}

template <class Base, class T,
          EnableIf<!HasAnnotation<BuilderDescriptionAnnotation, Base>()> = 0>
auto Write(Builder<Base>&& b, const T& val)
    -> decltype(AddAnnotation<BuilderDescriptionAnnotation>(std::move(b),
                                                            BuilderStream())) {
  return internal_description::Write(
      AddAnnotation<BuilderDescriptionAnnotation>(std::move(b),
                                                  BuilderStream()),
      val);
}

template <class Base, class T>
auto operator<<(Builder<Base>&& b, const T& val)
    -> decltype(internal_description::Write(std::move(b), val)) {
  return internal_description::Write(std::move(b), val);
}

template <class Base>
auto operator<<(Builder<Base>&& b, std::ostream& (*manip)(std::ostream&))
    -> decltype(internal_description::Write(std::move(b), manip)) {
  return internal_description::Write(std::move(b), manip);
}

}  // namespace internal_description

// This domain extension adds the ability to stream objects into merror policies
// and builders.
using DescriptionBuilder =
    Domain<internal_description::Policy, internal_description::Builder>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_DESCRIPTION_H_
