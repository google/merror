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

#ifndef MERROR_5EDA97_DOMAIN_DEFER_H_
#define MERROR_5EDA97_DOMAIN_DEFER_H_

#include <utility>

namespace merror {

// `Defer<Ts...>(expr)` can be used to turn any name into a dependent name. The
// explicitly-specified template arguments must be dependent types.
template <class... Ts, class U>
constexpr U&& Defer(U&& val) {
  return static_cast<U&&>(val);
}

}  // namespace merror

#endif  // MERROR_5EDA97_DOMAIN_DEFER_H_
