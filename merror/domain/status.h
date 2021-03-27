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
// This library defines error domain extensions for dealing with `Status` and
// `StatusOr`. All of them are a part of `merror::MErrorDomain()`.
//
// The `StatusBuilder()` extension provides control over the returned
// `absl::Status` value.
//
// The method `ErrorCode()` instructs the error domain to use the provided error
// code when constructing `Status`.
//
//   Status DoStuff();
//
//   Status Foo(int x) {
//     MERROR_DOMAIN(merror::Default);
//     MVERIFY(DoStuff());                      // error code is propagated
//     MVERIFY(DoStuff()).ErrorCode(INTERNAL);  // INTERNAL error code
//     MVERIFY(x > 0).ErrorCode(INTERNAL);      // INTERNAL error code
//     MVERIFY(x < 100);                        // compile error
//     return Status::OK;
//   }
//
// The method `DefaultErrorCode()` sets the fallback error that will be used in
// situations where otherwise the code would fail to compile.
//
//   Status Bar(int x) {
//     MERROR_DOMAIN(merror::Default).DefaultErrorCode(UNKNOWN);
//     MVERIFY(DoStuff());                      // error code is propagated
//     MVERIFY(DoStuff()).ErrorCode(INTERNAL);  // INTERNAL error code
//     MVERIFY(x > 0).ErrorCode(INTERNAL);      // INTERNAL error code
//     MVERIFY(x < 100);                        // UNKNOWN error code
//     return Status::OK;
//   }
//
//  Methods `NoDefaultErrorCode()` and `NoErrorCode()` can be used to remove
//  previous defaults.
//

#ifndef MERROR_5EDA97_DOMAIN_STATUS_H_
#define MERROR_5EDA97_DOMAIN_STATUS_H_

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "merror/domain/defer.h"
#include "merror/domain/description.h"
#include "merror/domain/internal/indenting_stream.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/print.h"
#include "merror/domain/return.h"

namespace merror {

// A policy/builder annotation with this key, if present, contains
// value convertible to merror::StatusCode. It defines the error code when
// constructing Status.
struct ErrorCodeAnnotation {};

// A policy/builder annotation with this key, if present, contains
// value convertible to merror::StatusCode. It defines the error code when
// constructing Status unless some other source provides error code.
struct DefaultErrorCodeAnnotation {};

namespace internal_status {

// TODO(romanp): add knobs for controlling the format of description.
// TODO(iserna): Add support for AddPayload.
struct StatusBuilder {
  template <class Base>
  struct Policy : Base {
    constexpr auto ErrorCode(absl::StatusCode code) const {
      return AddAnnotation<ErrorCodeAnnotation>(*this, code);
    }

    constexpr auto NoErrorCode() const {
      return RemoveAnnotations<ErrorCodeAnnotation>(*this);
    }

    constexpr auto DefaultErrorCode(absl::StatusCode code) const {
      return AddAnnotation<DefaultErrorCodeAnnotation>(*this, code);
    }

    constexpr auto NoDefaultErrorCode() const {
      return RemoveAnnotations<DefaultErrorCodeAnnotation>(*this);
    }
  };

  template <class Base>
  struct Builder : Base {
    auto ErrorCode(absl::StatusCode code) && {
      return AddAnnotation<ErrorCodeAnnotation>(std::move(*this), code);
    }

    auto NoErrorCode() && {
      return RemoveAnnotations<ErrorCodeAnnotation>(std::move(*this));
    }

    auto DefaultErrorCode(absl::StatusCode code) && {
      return AddAnnotation<DefaultErrorCodeAnnotation>(std::move(*this), code);
    }

    auto NoDefaultErrorCode() && {
      return RemoveAnnotations<DefaultErrorCodeAnnotation>(std::move(*this));
    }
  };
};

// Assembles description for Status for the case where the culprit doesn't have
// a description.
template <class Builder, class Culprit>
std::string StatusDescription(const Builder& builder, const Culprit& culprit) {
  internal::IndentingStream strm;
  auto WritePrefix = [&](std::string_view prefix) {
    strm.indent(0);
    strm << '\n' << prefix;
    strm.indent(prefix.size());
  };
  const auto& ctx = builder.context();
  strm << ctx.file << ':' << ctx.line << ": ";
  bool has_headline = false;
  if (ctx.macro != Macro::kError) {
    strm << ctx.macro_str << "(" << ctx.args_str << ")";
    has_headline = true;
  }
  for (std::string_view description :
       {merror::GetPolicyDescription(builder),
        merror::GetBuilderDescription(builder)}) {
    description = absl::StripAsciiWhitespace(description);
    if (!description.empty()) {
      if (has_headline) strm << '\n';
      strm << description;
      has_headline = true;
    }
  }
  if (!has_headline) strm << ctx.macro_str << "(" << ctx.args_str << ")";
  if (ctx.rel_expr) {
    WritePrefix("Same as: ");
    strm << ctx.macro_str << '(' << ctx.rel_expr->left << ' '
         << ctx.rel_expr->op << ' ' << ctx.rel_expr->right << ')';
  }
  // Print only printable non-boring culprits.
  if (CanPrint<Builder, Culprit>() && !std::is_empty<Culprit>()) {
    WritePrefix("Culprit: ");
    merror::TryPrint(builder, culprit, &strm);
    absl::StripTrailingAsciiWhitespace(&strm.str());
  }
  return std::move(strm.str());
}

struct StatusAcceptor {
  bool IsError() const { return !status.ok(); }
  const absl::Status& GetCulprit() const { return status; }
  const absl::Status& status;
};

template <class StatusOr>
struct StatusOrAcceptor {
  StatusOr&& status_or;
  bool IsError() const { return !status_or.ok(); }
  auto GetValue() && -> decltype(*std::forward<StatusOr>(status_or)) {
    return *std::forward<StatusOr>(status_or);
  }
  const absl::Status& GetCulprit() && { return status_or.status(); }
};

template <class Base>
struct AcceptStatus : Hook<Base> {
  using Hook<Base>::MVerify;
  using Hook<Base>::MTry;

  template <class R>
  StatusAcceptor MVerify(Ref<R, absl::Status> val) const {
    return {val.Get()};
  }

  template <class R, class T>
  StatusOrAcceptor<R> MTry(Ref<R, absl::StatusOr<T>> val) const {
    return {val.Forward()};
  }
};

template <class Base>
struct MakeStatus : Hook<Base> {
  using Hook<Base>::MakeMError;

  absl::Status MakeMError(ResultType<absl::Status>,
                          const absl::StatusCode& culprit) const {
    absl::StatusCode code =
        GetAnnotationOr<ErrorCodeAnnotation>(*this, culprit);
    return absl::Status(
        code, internal_status::StatusDescription(this->derived(), Void()));
  }

  absl::Status MakeMError(ResultType<absl::Status>,
                          const absl::Status& culprit) const {
    absl::StatusCode code =
        GetAnnotationOr<ErrorCodeAnnotation>(*this, culprit.code());
    std::string_view policy_description = merror::GetPolicyDescription(*this);
    std::string_view builder_description = merror::GetBuilderDescription(*this);
    if (policy_description.empty() && builder_description.empty()) {
      // If nothing has been streamed anything into the policy or the builder,
      // keep the description from the culprit.
      return HasAnnotation<ErrorCodeAnnotation>(*this)
                 ? absl::Status(code, culprit.message())
                 : culprit;
    }
    // If something has been streamed, join the three descriptions to form a
    // combined description.
    std::string_view parts[] = {culprit.message(), policy_description,
                                builder_description};
    for (std::string_view& s : parts) s = absl::StripAsciiWhitespace(s);
    auto end = std::remove(std::begin(parts), std::end(parts), "");
    auto begin = std::begin(parts);
    std::string message;
    if (end != begin) {
      message = *begin;
      for (++begin; end != begin; ++begin) {
        message.append("\n");
        message.append(*begin);
      }
    }
    return HasAnnotation<ErrorCodeAnnotation>(*this)
               ? absl::Status(code, message)
               : absl::Status(culprit.code(), message);
  }

  template <class T>
  absl::Status MakeMError(ResultType<absl::Status> r,
                          const absl::StatusOr<T>& culprit) const {
    return this->derived().MakeError(r, culprit.status());
  }

  template <class T, class Culprit>
  absl::StatusOr<T> MakeMError(ResultType<absl::StatusOr<T>>,
                               const Culprit& culprit) const {
    return this->derived().MakeError(ResultType<absl::Status>(), culprit);
  }
};

template <class Base>
struct MakeStatusFallback;

template <class Base, class Culprit>
absl::Status Fallback(ResultType<absl::Status>,
                      const MakeStatusFallback<Base>& b,
                      const Culprit& culprit) {
  static_assert(HasAnnotation<DefaultErrorCodeAnnotation, Base>() ||
                    HasAnnotation<ErrorCodeAnnotation, Base>(),
                "Use .ErrorCode() or .DefaultErrorCode() to set error code");
  const absl::StatusCode& code = GetAnnotationOr<ErrorCodeAnnotation>(
      b, GetAnnotationOr<DefaultErrorCodeAnnotation>(b, Void()));
  return absl::Status(code,
                      internal_status::StatusDescription(b.derived(), culprit));
}

template <class R, class Base, class Culprit>
R Fallback(ResultType<R> r, const MakeStatusFallback<Base>& b,
           const Culprit& culprit) {
  return b.Base::MakeError(r, culprit);
}

template <class Base>
struct MakeStatusFallback : Base {
  template <class R, class Culprit>
  auto MakeError(ResultType<R> r, const Culprit& culprit) const
      -> decltype(internal_status::Fallback(r, *this, culprit)) {
    return internal_status::Fallback(r, *this, culprit);
  }
};

}  // namespace internal_status

// Error domain extension that adds ErrorCode() and DefaultErrorCode() methods
// to the policy and builder.
using StatusBuilder = Domain<internal_status::StatusBuilder::Policy,
                             internal_status::StatusBuilder::Builder>;

// Error domain extension that enables merror macros to accept Status and
// StatusOr arguments.
using AcceptStatus = Policy<internal_status::AcceptStatus>;

// Error domain extension that enables merror macros to return Status and
// StatusOr when the culprit contains error code.
using MakeStatus = Builder<internal_status::MakeStatus>;

// Error domain extension that enables merror macros to return Status and
// StatusOr. Works for any kind of culprit.
//
// If your error domain has ADL hooks, put MakeStatusFallback behind them.
//
//   EmptyDomain().With(MakeStatusFallback(), AdlHooks(), MethodHooks(), ...)
using MakeStatusFallback = Builder<internal_status::MakeStatusFallback>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_STATUS_H_
