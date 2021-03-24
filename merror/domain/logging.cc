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

#include <assert.h>
#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "merror/domain/internal/indenting_stream.h"

namespace merror {

class FirstN::Filter {
 public:
  static bool AlwaysTrue(const FirstN&) { return false; }

  bool Test(const FirstN& cfg) {
    return i_.fetch_add(1, std::memory_order_relaxed) < cfg.n_;
  }

 private:
  std::atomic<int64_t> i_{0};
};

class EveryN::Filter {
 public:
  static bool AlwaysTrue(const EveryN& cfg) {
    return cfg.n_ == 1 || cfg.n_ == -1;
  }

  bool Test(const EveryN& cfg) {
    int64_t i = i_.fetch_add(1, std::memory_order_relaxed);
    return cfg.n_ != 0 && i % cfg.n_ == 0;
  }

 private:
  std::atomic<int64_t> i_{0};
};

class EveryPow2::Filter {
 public:
  static bool AlwaysTrue(const EveryPow2&) { return false; }

  bool Test(const EveryPow2&) {
    uint64_t i = i_.fetch_add(1, std::memory_order_relaxed);
    return (i & (i - 1)) == 0;
  }

 private:
  std::atomic<uint64_t> i_{1};
};

class Every::Filter {
 public:
  static bool AlwaysTrue(const Every& cfg) {
    return cfg.period_ <= Duration::zero();
  }

  bool Test(const Every& cfg) {
    const Time now = std::chrono::system_clock::now();
    Time logged = logged_.load(std::memory_order_relaxed);
    Duration period = cfg.period_;
    if (logged != Time::min() && now - logged < period) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    logged = logged_.load(std::memory_order_relaxed);
    if (logged != Time::min() && now - logged < period) return false;
    logged_.store(now, std::memory_order_relaxed);
    return true;
  }

 private:
  std::mutex mutex_;
  std::atomic<Time> logged_ = Time::min();
};

class NoFilter::Filter {
 public:
  static bool AlwaysTrue(const NoFilter&) { return true; }
  bool Test(const NoFilter&) { return true; }
};

namespace internal_logging {

template <class T>
size_t FastTypeId() {
  static int per_type;
  return reinterpret_cast<size_t>(&per_type);
}

using FilterKey = std::pair<uintptr_t, size_t>;

// Value is `Filter<T>*`. `FilterKey::first` is `location_id` from error
// context. `FilterKey::second` is `FastTypeId<Filter<T>>()`. It is technically
// possible to have several filters for the same location. See DynamicFilterType
// test for an example.

struct PairHash {
  template <class T1, class T2>
  std::size_t operator()(const std::pair<T1, T2>& pair) const {
    return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
  }
};

using SyncMap =
    std::pair<std::mutex, std::unordered_map<FilterKey, void*, PairHash>>;

static SyncMap* FilterMap() {
  static auto* res = new SyncMap;
  return res;
}

void CoutLogger::Log(const char* file, int line, std::string_view msg) const {
  std::cout << file << ":" << line << ": " << msg << std::endl;
}

void CerrLogger::Log(const char* file, int line, std::string_view msg) const {
  std::cerr << file << ":" << line << ": " << msg << std::endl;
}

template <class Filter>
bool ShouldLog(const Filter& filter, uintptr_t location_id) {
  if (Filter::Filter::AlwaysTrue(filter)) {
    // This is an optimization for memory usage: log sites with trivial
    // filters don't affect the size of the filter map. Strictly speaking,
    // this optimization is incorrect. Consider the following example:
    //
    //   DEFINE_int(n, 1, "");
    //
    //   void F() { MERROR().Log(INFO, EveryN(FLAGS_n)); }
    //
    //   void Test() {
    //     F();
    //     FLAGS_n = 100;
    //     F();
    //   }
    //
    // Without the optimization, the first call to `F()` would log but the
    // second wouldn't. With the optimization both calls will log.
    //
    // A situation like this isn't likely to arise in practice: starting with
    // logging cranked up to the max and then reducing it at runtime is very
    // unusual. Even if this happens, it's OK if we log a few extra records.
    // The memory savings are worth it.
    return true;
  }
  SyncMap* map = FilterMap();
  FilterKey key = {location_id,
                   internal_logging::FastTypeId<typename Filter::Filter>()};
  std::lock_guard<std::mutex> lk(map->first);
  auto iter = map->second.find(key);
  if (iter == map->second.end()) {
    // It's possible to speed up the BM_*_MultipleLocations benchmarks by
    // allocating memory aligned to ABSL_CACHELINE_SIZE on the next line (for
    // example, with posix_memalign). This will prevent cache line interference.
    // It would increase memory usage by each filter to ABSL_CACHELINE_SIZE
    // (typically 64 bytes).
    auto* value = new typename Filter::Filter();
    auto x = map->second.emplace(key, value);
    iter = x.first;
  }
  return static_cast<typename Filter::Filter*>(iter->second)->Test(filter);
}

template bool ShouldLog<NoFilter>(const NoFilter&, uintptr_t);
template bool ShouldLog<FirstN>(const FirstN&, uintptr_t);
template bool ShouldLog<EveryN>(const EveryN&, uintptr_t);
template bool ShouldLog<EveryPow2>(const EveryPow2&, uintptr_t);
template bool ShouldLog<Every>(const Every&, uintptr_t);

std::string FormatMessage(
    Macro macro, const char* macro_str, const char* args_str,
    RelationalExpression* rel_expr,
    const std::function<void(std::ostream*)>& print_culprit,
    std::string_view policy_description, std::string_view builder_description) {
  internal::IndentingStream strm;
  auto WritePrefix = [&](std::string_view prefix) {
    assert(!strm.str().empty());
    strm.indent(0);
    strm << '\n' << prefix;
    strm.indent(prefix.size());
  };
  if (macro != Macro::kError) strm << macro_str << '(' << args_str << ')';
  for (std::string_view description :
       {policy_description, builder_description}) {
    auto it = std::find_if(description.begin(), description.end(),
                           [](auto c) { return !std::isspace(c); });
    description.remove_prefix(it - description.begin());
    auto rit = std::find_if(description.rbegin(), description.rend(),
                            [](auto c) { return !std::isspace(c); });
    description.remove_suffix(rit - description.rbegin());
    if (!description.empty()) {
      if (!strm.str().empty()) strm << '\n';
      strm << description;
    }
  }
  if (strm.str().empty()) strm << macro_str << '(' << args_str << ')';
  if (rel_expr) {
    WritePrefix("Same as: ");
    strm << macro_str << '(' << rel_expr->left << ' ' << rel_expr->op << ' '
         << rel_expr->right << ')';
  }
  if (print_culprit) {
    WritePrefix("Culprit: ");
    print_culprit(&strm);
  }
  return std::move(strm.str());
}

}  // namespace internal_logging
}  // namespace merror
