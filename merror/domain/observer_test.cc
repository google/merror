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

#include "merror/domain/observer.h"

#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "merror/domain/base.h"
#include "merror/macros.h"

namespace merror {
namespace {

TEST(Observer, Works) {
  struct A {};
  struct B : Observer<A> {};
  B b = {};
  b.ObserveRetVal(42);
  b.ObserveRetVal("hello");
  static_assert(std::is_same<Observer<B>, B>(), "");
}

// The example from the header.
namespace example {

// Builder extension that prints the function's return value whenever an error
// is detected.
template <class Base>
struct RetValPrinter : Observer<Base> {
  template <class RetVal>
  void ObserveRetVal(const RetVal& ret_val) {
    std::cerr << "Error detected. Return value: " << ret_val;
    Observer<Base>::ObserveRetVal(ret_val);
  }
};

using PrintRetVal = Builder<RetValPrinter>;

template <class Base>
struct Return42 : Observer<Base> {
  int BuildError() && {
    const int res = 42;
    this->derived().ObserveRetVal(res);
    return res;
  }
};

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

TEST(Example, Works) {
  static constexpr auto MErrorDomain = PrintRetVal().With(Builder<Return42>());
  auto F = [] { return MERROR(); };
  std::string out;
  {
    CaptureStream c(std::cerr);
    EXPECT_EQ(42, F());
    out = c.str();
  }
  EXPECT_THAT(out, testing::EndsWith("Error detected. Return value: 42"));
}

}  // namespace example

}  // namespace
}  // namespace merror
