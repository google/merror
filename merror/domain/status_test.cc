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

#include "merror/domain/status.h"

#include <ostream>
#include <sstream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "merror/domain/bool.h"
#include "merror/domain/description.h"
#include "merror/domain/method_hooks.h"
#include "merror/domain/print_operands.h"
#include "merror/domain/return.h"
#include "merror/macros.h"

namespace merror {
namespace {

MATCHER(IsOk, "") { return arg.ok(); }

MATCHER_P(StatusIs, status, "") {
  if constexpr (std::is_same_v<std::decay_t<arg_type>, absl::Status>) {
    return arg.code() == status;
  } else {
    return arg.status().code() == status;
  }
}

MATCHER_P2(StatusIs, status, matcher, "") {
  if constexpr (std::is_same_v<std::decay_t<arg_type>, absl::Status>) {
    return arg.code() == status &&
           ExplainMatchResult(matcher, std::string(arg.message()),
                              result_listener);
  } else {
    return arg.status().code() == status &&
           ExplainMatchResult(matcher, std::string(arg.status().message()),
                              result_listener);
  }
}

using ::absl::StatusOr;
using ::testing::MatchesRegex;

template <bool Printable>
struct MyError {
  int code;
};

std::ostream& operator<<(std::ostream&, MyError<false>) = delete;

std::ostream& operator<<(std::ostream& strm, MyError<true> e) {
  return strm << e.code;
}

template <bool Printable>
absl::StatusCode ToCanonical(MyError<Printable> e) {
  switch (static_cast<absl::StatusCode>(e.code)) {
    case absl::StatusCode::kOk:
    case absl::StatusCode::kInternal:
      return static_cast<absl::StatusCode>(e.code);
    default:
      return absl::StatusCode::kUnknown;
  }
}

struct AcceptMyError {
  template <class Base>
  struct Policy : Hook<Base> {
    using Hook<Base>::MVerify;
    template <bool Printable>
    struct Acceptor {
      bool IsError() { return e.code != 0; }
      MyError<Printable> GetCulprit() && { return e; }
      MyError<Printable> e;
    };
    template <class R, bool Printable>
    Acceptor<Printable> MVerify(Ref<R, MyError<Printable>> e) const {
      return {e.Get()};
    }
  };

  template <class Base>
  struct Builder : Hook<Base> {
    using Hook<Base>::MakeMError;
    template <bool Printable>
    absl::Status MakeMError(ResultType<absl::Status> r,
                            MyError<Printable> expr) const {
      return this->derived().MakeError(r, ToCanonical(expr));
    }
  };
};

template <class Base>
struct Print : Base {
  template <class T>
  auto PrintTo(const T& obj, std::ostream* strm) const
      -> decltype(*strm << obj) {
    return *strm << obj;
  }
};

constexpr auto MyDomain = EmptyDomain().With(
    AcceptStatus(), Return(), MakeStatusFallback(), MethodHooks(), AcceptBool(),
    PrintOperands(), MakeStatus(), StatusBuilder(), DescriptionBuilder(),
    Domain<AcceptMyError::Policy, AcceptMyError::Builder>(),
    Domain<Print, Print>());

constexpr const auto MErrorDomain = MyDomain;

TEST(MakeStatus, Constexpr) {
  constexpr auto x = MyDomain.DefaultErrorCode(absl::StatusCode::kInternal)
                         .ErrorCode(absl::StatusCode::kInternal);
  static_cast<void>(x);
}

TEST(MakeStatus, AcceptStatus) {
  bool passed;
  auto F = [&](absl::Status status) -> absl::Status {
    passed = false;
    MVERIFY(status);
    passed = true;
    return absl::OkStatus();
  };
  EXPECT_TRUE(F(absl::OkStatus()).ok());
  EXPECT_TRUE(passed);
  EXPECT_THAT(F(absl::UnknownError("")).code(), absl::StatusCode::kUnknown);
  EXPECT_FALSE(passed);
}

TEST(MakeStatus, AcceptStatusOr) {
  bool passed;
  auto F = [&](StatusOr<int> obj) -> absl::Status {
    passed = false;
    int n = MTRY(obj);
    EXPECT_EQ(42, n);
    passed = true;
    return absl::OkStatus();
  };
  EXPECT_THAT(F(42), IsOk());
  EXPECT_TRUE(passed);
  EXPECT_THAT(F(absl::UnknownError("")), StatusIs(absl::StatusCode::kUnknown));
  EXPECT_FALSE(passed);
}

TEST(MakeStatusOr, AcceptStatusOr) {
  auto F = [](StatusOr<int> obj) -> StatusOr<std::string> {
    std::ostringstream strm;
    strm << MTRY(obj);
    return {strm.str()};
  };
  ASSERT_THAT(F(42), IsOk());
  EXPECT_THAT(*F(42), "42");
  EXPECT_THAT(F(absl::UnknownError("")), StatusIs(absl::StatusCode::kUnknown));
}

TEST(MakeStatus, DescriptionFromStatus) {
  {
    auto F = []() -> absl::Status {
      MVERIFY(absl::Status(absl::StatusCode::kInternal, " \n hello \n "));
      return absl::OkStatus();
    };
    EXPECT_THAT(F(), StatusIs(absl::StatusCode::kInternal, " \n hello \n "));
  }
  {
    auto F = []() -> absl::Status {
      constexpr auto MErrorDomain = MyDomain << " \n p1 \n p2 \n ";
      MVERIFY(absl::Status(absl::StatusCode::kInternal, " \n c1 \n c2 \n "))
          << " \n ";
      return absl::OkStatus();
    };
    EXPECT_THAT(F(), StatusIs(absl::StatusCode::kInternal,
                              "c1 \n"
                              " c2\n"
                              "p1 \n"
                              " p2"));
  }
  {
    auto F = []() -> absl::Status {
      constexpr auto MErrorDomain = MyDomain << " \n ";
      MVERIFY(absl::Status(absl::StatusCode::kInternal, " \n c1 \n c2 \n "))
          << " \n b1 \n b2 \n ";
      return absl::OkStatus();
    };
    EXPECT_THAT(F(), StatusIs(absl::StatusCode::kInternal,
                              "c1 \n"
                              " c2\n"
                              "b1 \n"
                              " b2"));
  }
  {
    auto F = []() -> absl::Status {
      constexpr auto MErrorDomain = MyDomain << " \n p1 \n p2 \n ";
      MVERIFY(absl::Status(absl::StatusCode::kInternal, " \n c1 \n c2 \n "))
          << " \n b1 \n b2 \n ";
      return absl::OkStatus();
    };
    EXPECT_THAT(F(), StatusIs(absl::StatusCode::kInternal,
                              "c1 \n"
                              " c2\n"
                              "p1 \n"
                              " p2\n"
                              "b1 \n"
                              " b2"));
  }
}

TEST(MakeStatus, DescriptionFromStatusOr) {
  auto F = []() -> absl::Status {
    MERROR_DOMAIN() << " \n bye \n ";
    MTRY(StatusOr<int>(
        absl::Status(absl::StatusCode::kInternal, " \n hello \n ")));
    return absl::OkStatus();
  };
  EXPECT_THAT(F(), StatusIs(absl::StatusCode::kInternal, "hello\nbye"));
}

TEST(MakeStatus, DescriptionFromNothing) {
  static auto UNKNOWN = absl::StatusCode::kUnknown;
  {
    auto F = []() -> absl::Status { return MERROR().ErrorCode(UNKNOWN); };
    // merror::Void is empty, so the culprit doesn't get printed.
    EXPECT_THAT(
        F(), StatusIs(UNKNOWN,
                      MatchesRegex(R"(.*status_test\.cc:[0-9]+: MERROR\(\))")));
  }
  {
    auto F = []() -> absl::Status {
      static constexpr auto MErrorDomain = MyDomain << " \n p1 \n p2 \n ";
      return MERROR().ErrorCode(UNKNOWN) << " \n b1 \n b2 \n ";
    };
    EXPECT_THAT(
        F(), StatusIs(UNKNOWN, MatchesRegex(R"(.*status_test\.cc:[0-9]+: p1 .)"
                                            R"( p2.)"
                                            R"(b1 .)"
                                            R"( b2)")));
  }
  {
    auto F = []() -> absl::Status {
      MVERIFY(
          MyError<false>{static_cast<int>(absl::StatusCode::kInvalidArgument)})
          .ErrorCode(absl::StatusCode::kAborted);
      return absl::OkStatus();
    };
    // MyError<false> is not ostreamable, so the culprit doesn't get printed.
    EXPECT_THAT(
        F(),
        StatusIs(
            absl::StatusCode::kAborted,
            MatchesRegex(
                R"(.*status_test\.cc:[0-9]+: )"
                R"(MVERIFY\(MyError<false>\{static_cast<int>\(absl::StatusCode::kInvalidArgument\)\}\))")));
  }
  {
    auto F = []() -> absl::Status {
      MVERIFY(MyError<true>{static_cast<int>(absl::StatusCode::kInternal)});
      return absl::OkStatus();
    };
    // MyError<true> is ostreamable and not empty, so the culprit gets printed.
    EXPECT_THAT(
        F(),
        StatusIs(
            absl::StatusCode::kInternal,
            MatchesRegex(
                R"(.*status_test\.cc:[0-9]+: )"
                R"(MVERIFY\(MyError<true>\{static_cast<int>\(absl::StatusCode::kInternal\)\}\))")));
  }
  {
    auto F = []() -> absl::Status {
      MVERIFY(2 + 2 == 5).ErrorCode(UNKNOWN);
      return absl::OkStatus();
    };
    EXPECT_THAT(
        F(), StatusIs(UNKNOWN,
                      MatchesRegex(
                          R"(.*status_test\.cc:[0-9]+: MVERIFY\(2 \+ 2 == 5\).)"
                          R"(Same as: MVERIFY\(4 == 5\))")));
  }
}

TEST(MakeStatus, ErrorCodeFromStatus) {
  {
    auto F = []() -> absl::Status {
      MVERIFY(absl::CancelledError(""))
          .DefaultErrorCode(absl::StatusCode::kUnknown);
      return absl::OkStatus();
    };
    EXPECT_THAT(F(), StatusIs(absl::StatusCode::kCancelled));
  }
  {
    auto F = []() -> absl::Status {
      MVERIFY(absl::CancelledError("")).ErrorCode(absl::StatusCode::kInternal);
      return absl::OkStatus();
    };
    EXPECT_THAT(F(), StatusIs(absl::StatusCode::kInternal));
  }
  {
    auto F = []() -> absl::Status {
      MVERIFY(absl::CancelledError(""))
          .DefaultErrorCode(absl::StatusCode::kUnknown)
          .ErrorCode(absl::StatusCode::kInternal);
      return absl::OkStatus();
    };
    EXPECT_THAT(F(), StatusIs(absl::StatusCode::kInternal));
  }
  {
    auto F = []() -> absl::Status {
      MVERIFY(absl::CancelledError(""))
          .ErrorCode(absl::StatusCode::kUnknown)
          .ErrorCode(absl::StatusCode::kInternal);
      return absl::OkStatus();
    };
    EXPECT_THAT(F(), StatusIs(absl::StatusCode::kInternal));
  }
}

TEST(MakeStatus, ErrorCodeFromStatusOr) {
  static const auto UNKNOWN = absl::StatusCode::kUnknown;
  static const auto CANCELLED = absl::StatusCode::kCancelled;
  static const auto INTERNAL = absl::StatusCode::kInternal;
  {
    auto F = []() -> absl::Status {
      MERROR_DOMAIN().DefaultErrorCode(UNKNOWN);
      MTRY(StatusOr<int>(absl::CancelledError()));
      return absl::OkStatus();
    };
    EXPECT_THAT(F(), StatusIs(CANCELLED));
  }
  {
    auto F = []() -> absl::Status {
      MERROR_DOMAIN().ErrorCode(INTERNAL);
      MTRY(StatusOr<int>(absl::CancelledError()));
      return absl::OkStatus();
    };
    EXPECT_THAT(F(), StatusIs(INTERNAL));
  }
  {
    auto F = []() -> absl::Status {
      MERROR_DOMAIN().DefaultErrorCode(UNKNOWN).ErrorCode(INTERNAL);
      MTRY(StatusOr<int>(absl::CancelledError()));
      return absl::OkStatus();
    };
    EXPECT_THAT(F(), StatusIs(INTERNAL));
  }
  {
    auto F = []() -> absl::Status {
      MERROR_DOMAIN().ErrorCode(UNKNOWN).ErrorCode(INTERNAL);
      MTRY(StatusOr<int>(absl::CancelledError()));
      return absl::OkStatus();
    };
    EXPECT_THAT(F(), StatusIs(INTERNAL));
  }
}

TEST(MakeStatus, ErrorCodeFromNothing) {
  static const auto UNKNOWN = absl::StatusCode::kUnknown;
  static const auto INTERNAL = absl::StatusCode::kInternal;
  {
    auto F = []() -> absl::Status {
      return MERROR().DefaultErrorCode(UNKNOWN);
    };
    EXPECT_THAT(F(), StatusIs(UNKNOWN));
  }
  {
    auto F = []() -> absl::Status { return MERROR().ErrorCode(INTERNAL); };
    EXPECT_THAT(F(), StatusIs(INTERNAL));
  }
  {
    auto F = []() -> absl::Status {
      return MERROR().DefaultErrorCode(UNKNOWN).ErrorCode(INTERNAL);
    };
    EXPECT_THAT(F(), StatusIs(INTERNAL));
  }
  {
    auto F = []() -> absl::Status {
      return MERROR().ErrorCode(UNKNOWN).DefaultErrorCode(INTERNAL);
    };
    EXPECT_THAT(F(), StatusIs(UNKNOWN));
  }
  {
    auto F = []() -> absl::Status {
      return MERROR()
          .ErrorCode(UNKNOWN)
          .DefaultErrorCode(INTERNAL)
          .NoErrorCode();
    };
    EXPECT_THAT(F(), StatusIs(INTERNAL));
  }
  {
    auto F = []() -> absl::Status {
      return MERROR().ErrorCode(UNKNOWN).NoErrorCode().ErrorCode(INTERNAL);
    };
    EXPECT_THAT(F(), StatusIs(INTERNAL));
  }
  {
    auto F = []() -> absl::Status {
      return MERROR().DefaultErrorCode(UNKNOWN).DefaultErrorCode(INTERNAL);
    };
    EXPECT_THAT(F(), StatusIs(INTERNAL));
  }
}

}  // namespace
}  // namespace merror
