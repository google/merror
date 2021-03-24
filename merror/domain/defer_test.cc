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

#include "merror/domain/defer.h"

#include <utility>

#include "gtest/gtest.h"

namespace merror {
namespace {

template <class Expected, class Actual>
void ExpectSame(Expected&& expected, Actual&& actual) {
  static_assert(std::is_same<Expected, Actual>(), "");
  EXPECT_EQ(&expected, &actual);
}

TEST(Defer, Functional) {
  int a = 0;
  const int b = 0;

  ExpectSame(a, Defer<>(a));
  ExpectSame(b, Defer<>(b));
  ExpectSame(std::move(a), Defer<>(std::move(a)));
  ExpectSame(std::move(b), Defer<>(std::move(b)));

  ExpectSame(a, Defer<void>(a));
  ExpectSame(b, Defer<void>(b));
  ExpectSame(std::move(a), Defer<void>(std::move(a)));
  ExpectSame(std::move(b), Defer<void>(std::move(b)));

  ExpectSame(a, Defer<void, int>(a));
  ExpectSame(b, Defer<void, int>(b));
  ExpectSame(std::move(a), Defer<void, int>(std::move(a)));
  ExpectSame(std::move(b), Defer<void, int>(std::move(b)));
}

}  // namespace
}  // namespace merror
