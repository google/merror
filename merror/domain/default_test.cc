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
// This is a smoke test. It verifies that `merror::MErrorDomain` isn't
// completely broken. The individual extensions of which it is comprised are
// tested in their respective tests.

#include "merror/domain/default.h"

#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "merror/macros.h"

namespace merror {
namespace {

using ::absl::StatusOr;
using ::testing::ContainsRegex;

constexpr MERROR_DOMAIN(merror::Default);

StatusOr<int> Div(int a, int b) {
  MVERIFY(b != 0).ErrorCode(absl::StatusCode::kInvalidArgument)
      << "Cannot divide by zero";
  return a / b;
}

StatusOr<int> Mod(int a, int b) { return a - b * MTRY(Div(a, b)); }

TEST(Default, Works) {
  auto res = Mod(8, 3);
  EXPECT_THAT(*res, 2);
  res = Mod(8, 0);
  EXPECT_THAT(res.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(
      std::string(res.status().message()),
      ContainsRegex(
          R"(.*default_test.cc:[0-9]+: MVERIFY\(b != 0\).*Cannot divide by zero.*Same as: MVERIFY\(0 != 0\))"));
}

}  // namespace
}  // namespace merror
