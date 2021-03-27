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
// This file is internal to merror. Don't include it directly and don't use
// anything that it defines.

#ifndef MERROR_5EDA97_DOMAIN_INTERNAL_CAPTURE_STREAM_H_
#define MERROR_5EDA97_DOMAIN_INTERNAL_CAPTURE_STREAM_H_

#include <ostream>
#include <streambuf>

#include "merror/domain/internal/stringstream.h"

namespace merror::internal {

struct CaptureStream {
  explicit CaptureStream(std::ostream& o) : other_(o), sbuf_(other_.rdbuf()) {
    other_.rdbuf(stream_.rdbuf());
  }
  ~CaptureStream() { other_.rdbuf(sbuf_); }
  const std::string& str() const { return buffer_; }

 private:
  std::ostream& other_;
  std::string buffer_;
  StringStream stream_{&buffer_};
  std::streambuf* const sbuf_;
};

}  // namespace merror::internal

#endif  // MERROR_5EDA97_DOMAIN_INTERNAL_CAPTURE_STREAM_H_