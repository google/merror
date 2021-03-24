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

#include "merror/domain/internal/indenting_stream.h"

#include "gtest/gtest.h"

namespace merror {
namespace internal {
namespace {

TEST(IndentingStream, NoIndentation) {
  {
    IndentingStream strm;
    EXPECT_EQ("", strm.str());
  }
  {
    IndentingStream strm;
    strm << "abc";
    EXPECT_EQ("abc", strm.str());
  }
  {
    IndentingStream strm;
    strm << "\n";
    EXPECT_EQ("\n", strm.str());
  }
  {
    IndentingStream strm;
    strm << "abc\n";
    EXPECT_EQ("abc\n", strm.str());
  }
  {
    IndentingStream strm;
    strm << "abc\ndef";
    EXPECT_EQ("abc\ndef", strm.str());
  }
}

TEST(IndentingStream, FromNewLine) {
  {
    IndentingStream strm;
    strm.indent(2);
    EXPECT_EQ("", strm.str());
  }
  {
    IndentingStream strm;
    strm.indent(2);
    strm << "abc";
    EXPECT_EQ("  abc", strm.str());
  }
  {
    IndentingStream strm;
    strm.indent(2);
    strm << "\n";
    EXPECT_EQ("\n", strm.str());
  }
  {
    IndentingStream strm;
    strm.indent(2);
    strm << "abc\n";
    EXPECT_EQ("  abc\n", strm.str());
  }
  {
    IndentingStream strm;
    strm.indent(2);
    strm << "abc\ndef";
    EXPECT_EQ("  abc\n  def", strm.str());
  }
}

TEST(IndentingStream, FromUnfinishedLine) {
  {
    IndentingStream strm;
    strm << "x";
    strm.indent(2);
    EXPECT_EQ("x", strm.str());
  }
  {
    IndentingStream strm;
    strm << "x";
    strm.indent(2);
    strm << "abc";
    EXPECT_EQ("xabc", strm.str());
  }
  {
    IndentingStream strm;
    strm << "x";
    strm.indent(2);
    strm << "\n";
    EXPECT_EQ("x\n", strm.str());
  }
  {
    IndentingStream strm;
    strm << "x";
    strm.indent(2);
    strm << "abc\n";
    EXPECT_EQ("xabc\n", strm.str());
  }
  {
    IndentingStream strm;
    strm << "x";
    strm.indent(2);
    strm << "abc\ndef";
    EXPECT_EQ("xabc\n  def", strm.str());
  }
}

}  // namespace
}  // namespace internal
}  // namespace merror
