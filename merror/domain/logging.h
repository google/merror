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
//
// Defines `Logging`. This error domain extension enables text logging on
// errors.
//
// By default, errors aren't logged. You can instruct the error builder to log
// errors by calling `Log()` or `VLog()`.
//
//   bool Read(char* out, int64 n) {
//     // Log errors to VLOG(1).
//     MVERIFY(out != nullptr).VLog(1);
//     MVERIFY(n >= 0).VLog(1);
//
//     while (buffer_.size() < n) {
//       // Log errors to LOG(WARNING).
//       MVERIFY(ReadNextChunk()).Log(WARNING);
//     }
//
//     std::copy_n(buffer_.begin(), out, n);
//     buffer_.erase(buffer_.begin(), buffer_.begin() + n);
//     return true;
//   }
//
// Note: the full name of `WARNING` is `base_logging::WARNING`.
//
// Logging all errors like in the example above can be dangerous. If for some
// reason the error conditions start to trigger frequently, the code may become
// IO-bound due to the sheer volume of logging. To avoid this problem you can
// supply a log filter that allow a subset of the log records to pass through
// and suppress the rest.
//
//   // Log at most one error every 60 seconds.
//   MVERIFY(ReadNextChunk()).Log(WARNING, Every(absl::Seconds(60)));
//
// There are several log filters to choose from.
//
//  * `FirstN(uint64 n)` accepts the first `n` log records and rejects the rest.
//    Similar to the `LOG_FIRST_N` macro.
//
//  * `EveryN(uint64 n)` accepts every nth log record starting from the first.
//    Similar to the `LOG_EVERY_N` macro.
//
//  * `EveryPow2()` accepts log records with power-of-two one-based indices.
//    Similar to the `LOG_EVERY_POW2` macro.
//
//  * `Every(absl::Duration d)` accepts one log record every `d` starting from
//    the first. Similar to the `LOG_EVERY_N_SEC` macro.
//
// You can specify the default error via `DefaultLogFilter(filter)`. This filter
// will be used for all `Log()` and `VLog()` calls that don't specify one
// explicitly.
//
//   constexpr auto MErrorDomain =
//       merror::Default().DefaultLogFilter(Every(absl::Seconds(60)));
//
//   Status Foo(int n) {
//     // No filter specified -- will use the default "every 60 seconds".
//     MVERIFY(n > 0).VLog(1);
//     // Filter specified explicitly -- won't use the default.
//     MVERIFY(Bar()).Log(WARNING, EveryPow2());
//   }
//
// All logging-related methods can be called on the builder as well as on the
// policy. It can be convenient to enable logging for all errors by default by
// calling `Log()` on the policy.
//
//   constexpr auto MErrorDomain =
//       merror::Default().Log(WARNING, Every(absl::Seconds(60)));
//
// Logging can be disabled by calling `NoLog()`. It has no effect if logging
// was already disabled.
//
// The last of the calls to `Log()`, `VLog()` or `NoLog()` wins. Likewise, the
// last call to `DefaultLogFilter()` wins.
//
//   constexpr auto MErrorDomain =
//       merror::Default()
//           .DefaultLogFilter(FirstN(10));
//           .VLog(1, EveryPow2())
//           .NoLog();
//
//   MVERIFY(n > 0)
//       .Log(WARNING)
//       .DefaultLogFilter(Every(absl::Seconds(60)));
//
// In the example above there are two calls to `DefaultLogFilter()`. The last
// one wins: `DefaultLogFilter(Every(absl::Seconds(60)))`. There are 3 calls to
// `*Log()`. Again the last one wins: `Log(WARNING)`. The end result is
// equivalent to `Log(WARNING, Every(absl::Seconds(60)))`.
//
// TODO(romanp): support custom filters.
// TODO(romanp): support custom loggers.
// TODO(romanp): support custom formatters.

#ifndef MERROR_5EDA97_DOMAIN_LOGGING_H_
#define MERROR_5EDA97_DOMAIN_LOGGING_H_

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "merror/domain/base.h"
#include "merror/domain/defer.h"
#include "merror/domain/description.h"
#include "merror/domain/observer.h"
#include "merror/domain/print.h"

namespace merror {

using Duration = std::chrono::milliseconds;
using Time = std::chrono::time_point<std::chrono::system_clock>;

namespace internal_logging {
// Returns true if the log record passes the filter. `location_id` is from error
// context.
template <class Filter>
bool ShouldLog(const Filter& filter, uintptr_t location_id);

}  // namespace internal_logging

// Log filter that accepts the first N log records and rejects the rest.
//
//   // Logs: 1, 2.
//   for (int i = 1; i <= 5; ++i) MERROR().Log(INFO, FirstN(2)) << i;
class FirstN {
 public:
  // If `n <= 0`, rejects all records.
  constexpr explicit FirstN(int64_t n) : n_(n) {}

 private:
  class Filter;
  friend bool internal_logging::ShouldLog<FirstN>(const FirstN&, uintptr_t);

  const int64_t n_;
};

// Log filter that accepts every Nth log record starting from the first.
//
//   // Logs: 1, 3, 5.
//   for (int i = 1; i <= 5; ++i) MERROR().Log(INFO, EveryN(2)) << i;
class EveryN {
 public:
  // The sign of `n` is ignored: `EveryN(-n)` is equivalent to `EveryN(n)`.
  // If `n == 0`, rejects all records.
  // If `n == 1`, accepts all records.
  constexpr explicit EveryN(int64_t n) : n_(n) {}

 private:
  class Filter;
  friend bool internal_logging::ShouldLog<EveryN>(const EveryN&, uintptr_t);

  const int64_t n_;
};

// Log filter that accepts log records with power-of-two one-based indices.
class EveryPow2 {
 private:
  class Filter;
  friend bool internal_logging::ShouldLog<EveryPow2>(const EveryPow2&,
                                                     uintptr_t);
};

// Log filter that accepts one log record per period starting from the first.
//
//   // Logs: 1, 3, 5.
//   for (int i = 1; i <= 5; ++i) {
//     MERROR().Log(INFO, Every(Seconds(2))) << i;
//     sleep(1);
//   }
class Every {
 public:
  // If `period <= Duration::zero()`, accepts all records.

  constexpr explicit Every(Duration period) : period_(period) {}

 private:
  class Filter;
  friend bool internal_logging::ShouldLog<Every>(const Every&, uintptr_t);

  const Duration period_;
};

// This type is used for two purposes:
//
//   * It's a filter that accepts all records.
//   * `LogAndFilter<T, NoFilter>` has no filter of its own and thus should fall
//     back to the default filter.
struct NoFilter {
  class Filter;
};

namespace internal_logging {

// The key for the default filter annotation. The value is a filter such as
// `FirstN` or `EveryN`.
struct DefaultFilterAnnotation {};

// The key for the logger annotation. The value is `LogAndFilter`.
struct LogAndFilterAnnotation {};

// Logger that sends data to /dev/null.
struct NullLogger {
  bool IsEnabled(const char* file, int line) const { return false; }
  void Log(const char*, int, std::string_view) const {}
};

struct CoutLogger {
  bool IsEnabled(const char* file, int line) const { return true; }
  void Log(const char*, int, std::string_view) const;
};

struct CerrLogger {
  bool IsEnabled(const char* file, int line) const { return true; }
  void Log(const char*, int, std::string_view) const;
};

template <class Log, class Filter>
struct LogAndFilter {
  static constexpr bool HasFilter() {
    return !std::is_same<Filter, NoFilter>();
  }
  Log log;
  Filter filter;
};

template <class Base>
struct Policy : Base {
  template <class Filter>
  constexpr auto DefaultLogFilter(Filter&& filter) const
      -> decltype(AddAnnotation<DefaultFilterAnnotation>(
          *this, std::forward<Filter>(filter))) {
    return AddAnnotation<DefaultFilterAnnotation>(*this,
                                                  std::forward<Filter>(filter));
  }

  template <class X = void>
  constexpr auto NoLog() const
      -> decltype(AddAnnotation<LogAndFilterAnnotation>(
          Defer<X>(*this), LogAndFilter<NullLogger, NoFilter>())) {
    return AddAnnotation<LogAndFilterAnnotation>(
        *this, LogAndFilter<NullLogger, NoFilter>());
  }

  template <class Annotation, class Logger, class Filter>
  constexpr auto LogImpl(Logger&& logger, Filter&& filter) const {
    return AddAnnotation<Annotation>(
        *this, LogAndFilter<std::decay_t<Logger>, std::decay_t<Filter>>{
                   std::forward<Logger>(logger), std::forward<Filter>(filter)});
  }

  template <class Filter = NoFilter, class X = void>
  constexpr auto CoutLog(Filter&& filter = NoFilter()) const {
    return LogImpl<LogAndFilterAnnotation>(CoutLogger{},
                                           std::forward<Filter>(filter));
  }

  template <class Filter = NoFilter, class X = void>
  constexpr auto CerrLog(Filter&& filter = NoFilter()) const {
    return LogImpl<LogAndFilterAnnotation>(CerrLogger{},
                                           std::forward<Filter>(filter));
  }
};

extern template bool ShouldLog<NoFilter>(const NoFilter&, uintptr_t);
extern template bool ShouldLog<FirstN>(const FirstN&, uintptr_t);
extern template bool ShouldLog<EveryN>(const EveryN&, uintptr_t);
extern template bool ShouldLog<EveryPow2>(const EveryPow2&, uintptr_t);
extern template bool ShouldLog<Every>(const Every&, uintptr_t);

// In order to avoid code bloat, all formatting is within this non-inline
// function.
//
// `macro`, `macro_str`, `args_str` and `rel_expr` are from error context.
// `print_culprit` can be null.
std::string FormatMessage(
    Macro macro, const char* macro_str, const char* args_str,
    RelationalExpression* rel_expr,
    const std::function<void(std::ostream*)>& print_culprit,
    std::string_view policy_description, std::string_view builder_description);

template <class Base>
struct Builder : Observer<Base> {
  template <class Filter>
  auto
  DefaultLogFilter(Filter&& filter) && -> decltype(AddAnnotation<
                                                   DefaultFilterAnnotation>(
      std::move(*this), std::forward<Filter>(filter))) {
    return AddAnnotation<DefaultFilterAnnotation>(std::move(*this),
                                                  std::forward<Filter>(filter));
  }

  template <class X = void>
  auto NoLog() && -> decltype(AddAnnotation<LogAndFilterAnnotation>(
      std::move(Defer<X>(*this)), LogAndFilter<NullLogger, NoFilter>())) {
    return AddAnnotation<LogAndFilterAnnotation>(
        std::move(*this), LogAndFilter<NullLogger, NoFilter>());
  }

  template <class Annotation, class Logger, class Filter>
  auto LogImpl(Logger&& logger, Filter&& filter) && {
    return AddAnnotation<Annotation>(
        std::move(*this),
        LogAndFilter<std::decay_t<Logger>, std::decay_t<Filter>>{
            std::forward<Logger>(logger), std::forward<Filter>(filter)});
  }

  template <class Filter = NoFilter, class X = void>
  auto CoutLog(Filter&& filter = NoFilter()) && {
    return std::move(*this).template LogImpl<LogAndFilterAnnotation>(
        CoutLogger{}, std::forward<Filter>(filter));
  }

  template <class Filter = NoFilter, class X = void>
  auto CerrLog(Filter&& filter = NoFilter()) && {
    return std::move(*this).template LogImpl<LogAndFilterAnnotation>(
        CerrLogger{}, std::forward<Filter>(filter));
  }

  template <class RetVal>
  void ObserveRetVal(const RetVal& ret_val) {
    struct Cleanup {
      ~Cleanup() {
        static_cast<Observer<Base>*>(builder)->ObserveRetVal(ret_val);
      }
      Builder* builder;
      const RetVal& ret_val;
    } cleanup{this, ret_val};
    static_cast<void>(cleanup);
    const auto& ctx = this->context();
    auto create_message = [&]() {
      // Formatting and logging is ~100 times slower than filtering.
      std::function<void(std::ostream*)> print_culprit;
      using Culprit = typename Builder::ContextType::Culprit;
      // Print only printable non-boring culprits.
      if (CanPrint<Builder, Culprit>() &&
          !std::is_empty<typename std::decay<Culprit>::type>()) {
        print_culprit = [&](std::ostream* strm) {
          merror::TryPrint(this->derived(), ctx.culprit, strm);
        };
      }
      return FormatMessage(ctx.macro, ctx.macro_str, ctx.args_str, ctx.rel_expr,
                           print_culprit, merror::GetPolicyDescription(*this),
                           merror::GetBuilderDescription(*this));
    };
    auto logger = GetAnnotationOr<LogAndFilterAnnotation>(
        *this, LogAndFilter<NullLogger, NoFilter>());
    if (!logger.log.IsEnabled(ctx.file, ctx.line)) return;
    // `ShouldLog()` takes ~20ns when returning false.
    if (logger.HasFilter()) {
      if (!ShouldLog(logger.filter, ctx.location_id)) return;
    } else {
      if (!ShouldLog(
              GetAnnotationOr<DefaultFilterAnnotation>(*this, NoFilter()),
              ctx.location_id))
        return;
    }
    logger.log.Log(ctx.file, ctx.line, create_message());
  }
};

}  // namespace internal_logging

// Error domain extension for text logging.
using Logging = Domain<internal_logging::Policy, internal_logging::Builder>;

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_LOGGING_H_
