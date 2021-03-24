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

#include "merror/domain/logging.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "merror/domain/base.h"
#include "merror/domain/bool.h"
#include "merror/domain/description.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/print.h"
#include "merror/domain/print_operands.h"
#include "merror/domain/return.h"
#include "merror/domain/status.h"
#include "merror/macros.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace merror {
namespace {

using ::testing::ElementsAre;
using ::testing::EndsWith;

// If `kQuantMs` is too low, the test may miss bugs. It'll print the following
// warning in this case:
//
//   ===============================================
//   Test is too slow. Consider increasing kQuantMs.
//   ===============================================
//
// If `kQuantMs` is too high, the test will take a long time to run.
constexpr Duration kQuant{50};

constexpr auto MyErrorDomain =
    EmptyDomain()
        .With(Logging(), MethodHooks(), Return(), AcceptBool(), Print(),
              PrintOperands(), DescriptionBuilder(), MakeStatus(),
              AcceptStatus())
        .Return();

constexpr auto MErrorDomain = MyErrorDomain;

struct CaptureStream {
  explicit CaptureStream(std::ostream& o) : other(o), sbuf(other.rdbuf()) {
    other.rdbuf(buffer.rdbuf());
  }
  ~CaptureStream() { other.rdbuf(sbuf); }
  const std::string str() const { return buffer.str(); }

 private:
  std::ostream& other;
  std::stringstream buffer;
  std::streambuf* const sbuf;
};

std::vector<std::string> Split(std::string_view out) {
  std::vector<std::string> ret;
  while (!out.empty()) {
    auto pos = out.find('\n');
    auto candidate = out.substr(0, pos);
    if (!candidate.empty()) ret.push_back(std::string(candidate));
    out.remove_prefix(pos + 1);
  }
  return ret;
}

TEST(Logging, Format) {
  std::string out;
  {
    CaptureStream c(std::cout);
    MERROR().CoutLog() << "hello";
    out = c.str();
  }
  EXPECT_THAT(out, testing::MatchesRegex(".*logging_test.cc.*hello.*"));
  {
    CaptureStream c(std::cout);
    MERROR().CoutLog();
    out = c.str();
  }
  EXPECT_THAT(out, testing::EndsWith("MERROR()\n"));
  {
    CaptureStream c(std::cout);
    []() {
      constexpr auto MErrorDomain = MyErrorDomain << " \n d1 \n d2 \n ";
      int n = 1;
      MVERIFY(n < 0).CoutLog() << " \n b1 \n b2 \n ";
    }();
    out = c.str();
  }
  EXPECT_THAT(out, testing::EndsWith("MVERIFY(n < 0)\n"
                                     "d1 \n"
                                     " d2\n"
                                     "b1 \n"
                                     " b2\n"
                                     "Same as: MVERIFY(1 < 0)\n"));
}

TEST(Logging, ConstexprPolicy) {
  constexpr auto MErrorDomain = MyErrorDomain.CoutLog()
                                    .CerrLog()
                                    .CoutLog(FirstN(0))
                                    .CoutLog(EveryN(0))
                                    .CoutLog(EveryPow2())
                                    .CoutLog(Every(std::chrono::milliseconds{
                                        std::numeric_limits<int64_t>::min()}));
  static_cast<void>(MErrorDomain);
}

TEST(Logging, FirstN) {
  std::string out;
  std::vector<std::string> v;
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i) MERROR().CoutLog(FirstN(0)) << i + 1;
    out = c.str();
  }
  EXPECT_THAT(out, "");
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i) MERROR().CoutLog(FirstN(1)) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i) MERROR().CoutLog(FirstN(2)) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("2")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i)
      MERROR().CoutLog(FirstN(std::numeric_limits<int64_t>::max())) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("2"), EndsWith("3")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i) MERROR().CoutLog(FirstN(-1)) << i + 1;
    out = c.str();
  }
  EXPECT_THAT(out, "");
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i)
      MERROR().CoutLog(FirstN(std::numeric_limits<int64_t>::min())) << i + 1;
    out = c.str();
  }
  EXPECT_THAT(out, "");
  {
    CaptureStream c(std::cout);
    uint64_t n[] = {1, 1, 3};
    for (int i = 0; i != 3; ++i) {
      MERROR().CoutLog(FirstN(n[i])) << i + 1;
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i) {
      MERROR().CoutLog(FirstN(i)) << i + 1;
    }
    out = c.str();
  }
  EXPECT_THAT(out, "");
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i) {
      MERROR().CoutLog(FirstN(i * 2)) << i + 1;
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("2"), EndsWith("3")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i) MERROR().CoutLog(FirstN(1)) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i) MERROR().CoutLog(FirstN(1)) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1")));
  {
    CaptureStream c(std::cout);
    constexpr auto MErrorDomain = MyErrorDomain.CoutLog(FirstN(1));
    for (int i = 0; i != 3; ++i) MERROR() << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1")));
}

TEST(Logging, EveryN) {
  std::string out;
  std::vector<std::string> v;
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) MERROR().CoutLog(EveryN(0)) << i + 1;
    out = c.str();
  }
  EXPECT_THAT(out, "");
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) MERROR().CoutLog(EveryN(1)) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("2"), EndsWith("3"),
                             EndsWith("4")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) MERROR().CoutLog(EveryN(2)) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) MERROR().CoutLog(EveryN(3)) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("4")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) MERROR().CoutLog(EveryN(-1)) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("2"), EndsWith("3"),
                             EndsWith("4")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) MERROR().CoutLog(EveryN(-2)) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i)
      MERROR().CoutLog(EveryN(std::numeric_limits<int64_t>::max())) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i)
      MERROR().CoutLog(EveryN(std::numeric_limits<int64_t>::min())) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 2; ++i) MERROR().CoutLog(EveryN(i * 2)) << i + 1;
    out = c.str();
  }
  EXPECT_THAT(out, "");
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 5; ++i) MERROR().CoutLog(EveryN(6 - i)) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("4"), EndsWith("5")));
  {
    CaptureStream c(std::cout);
    constexpr auto MErrorDomain = MyErrorDomain.CoutLog(EveryN(2));
    for (int i = 0; i != 4; ++i) MERROR() << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
}

TEST(Logging, EveryPow2) {
  std::string out;
  std::vector<std::string> v;
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 31; ++i) MERROR().CoutLog(EveryPow2()) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("2"), EndsWith("4"),
                             EndsWith("8"), EndsWith("16")));
  {
    CaptureStream c(std::cout);
    constexpr auto MErrorDomain = MyErrorDomain.CoutLog(EveryPow2());
    for (int i = 0; i != 7; ++i) MERROR() << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("2"), EndsWith("4")));
}

class LogCatcher {
 public:
  LogCatcher() {}

  LogCatcher(LogCatcher&&) = delete;

  std::vector<std::string> logs() {
    std::vector<std::string> ret;
    auto s = c_.str();
    for (auto e : Split(s)) {
      ret.push_back(std::string(e));
    }
    return ret;
  }

 private:
  CaptureStream c_{std::cout};
};

void TimeFilterQuantTest(const std::function<void(int)>& log) {
  std::optional<LogCatcher> log_catcher;
  log_catcher.emplace();
  const Time t0 = std::chrono::system_clock::now();
  for (int i = 0; i != 3; ++i) {
    for (int j = 0; j != 3; ++j) log(i * 3 + j + 1);
    if (std::chrono::system_clock::now() - t0 >= (i + 1) * kQuant) {
      log_catcher = absl::nullopt;
      std::cerr << "===============================================";
      std::cerr << "Test is too slow. Consider increasing kQuantMs.";
      std::cerr << "===============================================";
      return;
    }
    if (i != 2) std::this_thread::sleep_for(kQuant);
  }
  auto logs = log_catcher->logs();
  EXPECT_THAT(logs, ElementsAre(EndsWith("1"), EndsWith("4"), EndsWith("7")));
}

TEST(Logging, Every) {
  std::string out;
  std::vector<std::string> v;
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i)
      MERROR().CoutLog(Every(Duration::zero())) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("2"), EndsWith("3")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i) {
      // TODO(iserna): Figure out why max does not work.
      MERROR().CoutLog(Every(Duration{1000000})) << i + 1;
      if (i == 0) std::this_thread::sleep_for(kQuant);
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1")));
  TimeFilterQuantTest([](int n) { MERROR().CoutLog(Every(kQuant)) << n; });
  TimeFilterQuantTest([](int n) {
    const auto MErrorDomain = MyErrorDomain.CoutLog(Every(kQuant));
    MERROR() << n;
  });
}

TEST(Logging, DefaultLogFilter) {
  std::string out;
  std::vector<std::string> v;
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i)
      MERROR().DefaultLogFilter(EveryN(2)).CoutLog() << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) {
      MERROR().DefaultLogFilter(FirstN(1)).DefaultLogFilter(EveryN(2)).CoutLog()
          << i + 1;
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i)
      MERROR().DefaultLogFilter(EveryN(2)).CoutLog(FirstN(2)) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("2")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) {
      MERROR().DefaultLogFilter(EveryN(2)).CoutLog(FirstN(2)).CoutLog()
          << i + 1;
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) {
      static constexpr auto MErrorDomain =
          MyErrorDomain.DefaultLogFilter(EveryN(2));
      MERROR().CoutLog() << i + 1;
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) {
      static constexpr auto MErrorDomain =
          MyErrorDomain.DefaultLogFilter(FirstN(2)).DefaultLogFilter(EveryN(2));
      MERROR().CoutLog() << i + 1;
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) {
      static constexpr auto MErrorDomain =
          MyErrorDomain.DefaultLogFilter(FirstN(2));
      MERROR().CoutLog().DefaultLogFilter(EveryN(2)) << i + 1;
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) {
      static constexpr auto MErrorDomain =
          MyErrorDomain.DefaultLogFilter(FirstN(2));
      MERROR().CoutLog(EveryN(2)) << i + 1;
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
}

TEST(Logging, LoggerOverrides) {
  std::string out;
  std::vector<std::string> v;
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) MERROR().CerrLog().CoutLog(EveryN(2)) << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
  {
    CaptureStream c(std::cerr);
    for (int i = 0; i != 4; ++i) MERROR().CoutLog(EveryN(2)).CerrLog() << i + 1;
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("2"), EndsWith("3"),
                             EndsWith("4")));
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 4; ++i) {
      static constexpr auto MErrorDomain = MyErrorDomain.CerrLog();
      MERROR().CoutLog(EveryN(2)) << i + 1;
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
  {
    CaptureStream c(std::cerr);
    for (int i = 0; i != 4; ++i) {
      static constexpr auto MErrorDomain = MyErrorDomain.CoutLog(EveryN(2));
      MERROR().CerrLog() << i + 1;
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("2"), EndsWith("3"),
                             EndsWith("4")));
}

struct TypeErasedBuilder {
  void BuildError() { impl(); }
  std::function<void()> impl;
};

template <class Base>
struct DynamicFilter : Base {
  template <class Filter1, class Filter2>
  TypeErasedBuilder Filter(int n, Filter1&& filter1, Filter2&& filter2) {
    if (n % 2) {
      auto self = std::move(this->derived())
                      .DefaultLogFilter(std::forward<Filter1>(filter1))
                  << n;
      auto p = std::make_shared<decltype(self)>(std::move(self));
      return {[p]() { std::move(*p).BuildError(); }};
    } else {
      auto self = std::move(this->derived())
                      .DefaultLogFilter(std::forward<Filter2>(filter2))
                  << n;
      auto p = std::make_shared<decltype(self)>(std::move(self));
      return {[p]() { std::move(*p).BuildError(); }};
    }
  }
};

TEST(Logging, DynamicFilterType) {
  std::string out;
  std::vector<std::string> v;
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 10; ++i) {
      static constexpr auto MErrorDomain =
          MyErrorDomain.With(Builder<DynamicFilter>());
      // Logging with alternating filters: one record with `EveryN(2)`, one with
      // `FirstN(1)`. The result is the same as if we had two `MERROR()`
      // statements:
      //
      //   if (n % 2)
      //     MERROR().CoutLog(EveryN(2));
      //   else
      //     MERROR().CoutLog(FirstN(1));
      MERROR().CoutLog().Filter(i + 1, EveryN(2), FirstN(1));
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("2"), EndsWith("5"),
                             EndsWith("9")));
}

TEST(Logging, AlwaysTrueThenFalse) {
  std::string out;
  std::vector<std::string> v;
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 2; ++i) {
      MERROR().CoutLog(EveryN(i + 1)) << i + 1;
    }
    out = c.str();
  }
  // Ideally, the second record should be suppressed. It doesn't get suppressed
  // as an unfortunate side effect of an optimization. See comments in
  // `ShouldLog()`.
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("2")));
}

TEST(Logging, BecomeAlwaysTrue) {
  std::string out;
  std::vector<std::string> v;
  {
    CaptureStream c(std::cout);
    for (int i = 0; i != 3; ++i) {
      MERROR().CoutLog(EveryN(3 - i)) << i + 1;
    }
    out = c.str();
  }
  v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("1"), EndsWith("3")));
}

TEST(Logging, MultipleLocations) {
  std::string out;
  {
    CaptureStream c(std::cout);
    static constexpr auto MErrorDomain = MyErrorDomain.CoutLog(EveryN(2));
    for (int i = 0; i != 4; ++i) {
      MERROR() << "A" << i + 1;
      MERROR() << "B" << i + 1;
    }
    out = c.str();
  }
  std::vector<std::string> v = Split(out);
  EXPECT_THAT(v, ElementsAre(EndsWith("A1"), EndsWith("B1"), EndsWith("A3"),
                             EndsWith("B3")));
}

}  // namespace
}  // namespace merror
