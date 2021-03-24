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

#include "merror/domain/optional.h"

#include <string>

#include "merror/domain/return.h"
#include "merror/macros.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace merror {
namespace {

using std::nullopt;
using std::nullopt_t;
using std::optional;

constexpr auto MErrorDomain =
    EmptyDomain()
        .With(MethodHooks(), Return(), AcceptOptional(), MakeOptional());

TEST(Optional, ReturnOptional) {
  bool passed;
  auto F = [&](optional<int> n) -> optional<std::string> {
    passed = false;
    std::string res = std::to_string(MTRY(n));
    passed = true;
    return res;
  };
  EXPECT_EQ(optional<std::string>("42"), F(42));
  EXPECT_TRUE(passed);
  EXPECT_EQ(nullopt, F(nullopt));
  EXPECT_FALSE(passed);
}

TEST(Optional, ReturnNullopt) {
  bool passed;
  auto F = [&](optional<int> n) -> optional<std::string> {
    MERROR_DOMAIN().Return<nullopt_t>();
    passed = false;
    std::string res = std::to_string(MTRY(n));
    passed = true;
    return res;
  };
  EXPECT_EQ(optional<std::string>("42"), F(42));
  EXPECT_TRUE(passed);
  EXPECT_EQ(nullopt, F(nullopt));
  EXPECT_FALSE(passed);
}

}  // namespace
}  // namespace merror
