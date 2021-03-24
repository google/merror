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

#include "merror/internal/tls_map.h"

#include <memory.h>
#include <stdint.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"

namespace merror {
namespace internal {
namespace tls_map {
namespace {

using tls_map::testing::Size;
using tls_map::testing::Capacity;
using tls_map::testing::Clear;

class X {
 public:
  X() {}
  ~X() { assert("01234567890123456789" == fingerprint_); }

 private:
  X(const X&) = delete;
  X& operator=(const X&) = delete;

  std::string fingerprint_ = "01234567890123456789";
};

template <class T>
struct Cleanup {
  Cleanup() {}
  Cleanup(const Cleanup&) = delete;
  ~Cleanup() {
    Clear<T>();
    assert(0 == Size<T>());
    assert(0 == Capacity<T>());
  }
};

TEST(TlsMap, Threads) {
  std::vector<std::thread> v;
  for (int i = 0; i < 2; ++i) {
    v.emplace_back([&] {
      Put<X>(0);
      Remove<X>(0);
    });
  }
  for (auto& t : v) {
    t.join();
  }
}

TEST(TlsMap, Empty) {
  Cleanup<X> cleanup;
  EXPECT_EQ(0, Size<X>());
  EXPECT_EQ(0, Capacity<X>());
}

TEST(TlsMap, OneValue) {
  Cleanup<X> cleanup;
  X* p = Put<X>(0);
  EXPECT_EQ(1, Size<X>());
  EXPECT_EQ(1, Capacity<X>());

  EXPECT_EQ(p, Get<X>(0));
}

TEST(TlsMap, TwoValues) {
  Cleanup<X> cleanup;
  X* p0 = Put<X>(0);
  X* p1 = Put<X>(1);
  EXPECT_NE(p0, p1);
  EXPECT_EQ(2, Size<X>());
  EXPECT_EQ(2, Capacity<X>());

  EXPECT_EQ(p0, Get<X>(0));
  EXPECT_EQ(p1, Get<X>(1));
}

TEST(TlsMap, DuplicateKeys) {
  Cleanup<X> cleanup;
  X* a = Put<X>(0);
  X* b = Put<X>(0);
  EXPECT_NE(a, b);
  EXPECT_EQ(2, Size<X>());
  EXPECT_EQ(2, Capacity<X>());

  EXPECT_EQ(b, Get<X>(0));
  Remove<X>(0);
  EXPECT_EQ(1, Size<X>());
  EXPECT_EQ(2, Capacity<X>());

  EXPECT_EQ(a, Get<X>(0));
}

TEST(TlsMap, Remove) {
  Cleanup<X> cleanup;
  X* x = Put<X>(0);
  EXPECT_EQ(1, Size<X>());
  EXPECT_EQ(1, Capacity<X>());

  Remove<X>(0);
  EXPECT_EQ(0, Size<X>());
  EXPECT_EQ(1, Capacity<X>());

  EXPECT_EQ(x, Put<X>(0));
  EXPECT_EQ(1, Size<X>());
  EXPECT_EQ(1, Capacity<X>());
}

TEST(TlsMap, Reuse) {
  Cleanup<X> cleanup;
  X* p0 = Put<X>(0);
  X* p1 = Put<X>(1);
  X* p2 = Put<X>(2);
  EXPECT_NE(p0, p1);
  EXPECT_NE(p0, p2);
  EXPECT_NE(p1, p2);
  EXPECT_EQ(3, Size<X>());
  EXPECT_EQ(3, Capacity<X>());

  Remove<X>(0);
  Remove<X>(1);
  EXPECT_EQ(1, Size<X>());
  EXPECT_EQ(3, Capacity<X>());

  EXPECT_EQ(p1, Put<X>(3));
}

TEST(TlsMap, LargeValue) {
  using T = std::aligned_storage<1 << 20>;
  Cleanup<T> cleanup;
  T* p = Put<T>(0);
  EXPECT_EQ(0, reinterpret_cast<uintptr_t>(p) % alignof(T));
  memset(p, 0, sizeof(T));
}

struct Notification {
  void Notify() {
    {
      std::lock_guard<std::mutex> lk(mu);
      ready = true;
    }
    cv.notify_all();
  }

  void WaitForNotification() {
    std::unique_lock<std::mutex> lk(mu);
    cv.wait(lk, [this] { return ready; });
  }

  std::condition_variable cv;
  bool ready = false;
  std::mutex mu;
};

TEST(TlsMap, MultipleThreads) {
  Cleanup<X> cleanup;
  X* a = Put<X>(0);
  Notification n1, n2, n3;

  std::thread thr([&]() {
    Cleanup<X> cleanup;
    EXPECT_EQ(0, Size<X>());
    EXPECT_EQ(0, Capacity<X>());

    X* b = Put<X>(0);
    EXPECT_NE(a, b);
    EXPECT_EQ(1, Size<X>());
    EXPECT_EQ(1, Capacity<X>());
    EXPECT_EQ(b, Get<X>(0));

    n1.Notify();
    n2.WaitForNotification();

    EXPECT_EQ(1, Size<X>());
    EXPECT_EQ(1, Capacity<X>());

    n3.Notify();
  });

  n1.WaitForNotification();

  EXPECT_EQ(1, Size<X>());
  EXPECT_EQ(1, Capacity<X>());
  EXPECT_EQ(a, Get<X>(0));

  n2.Notify();
  n3.WaitForNotification();
  thr.join();
}

TEST(TlsMap, ThreadCleanup) {
  for (int n = 0; n != 3; ++n) {
    std::thread thr([n]() {
      for (int i = 0; i != n; ++i) Put<X>(0);
      for (int i = 0; i != n; ++i) Remove<X>(0);
      EXPECT_EQ(0, Size<X>());
      EXPECT_EQ(n, Capacity<X>());
    });
    thr.join();
  }
}

// This is the example from the top of tls_map.h.
TEST(TlsMap, Example) {
  Cleanup<std::string> cleanup;  // not part of the example

  std::string* s1 = Put<std::string>(1, "s1");
  EXPECT_EQ("s1", *s1);
  std::string* s2 = Put<std::string>(2, "s2");
  EXPECT_EQ("s2", *s2);
  std::string* s1_dup = Put<std::string>(1, "s1_dup");
  EXPECT_EQ("s1_dup", *s1_dup);

  EXPECT_EQ(s1_dup, Get<std::string>(1));
  EXPECT_EQ(s2, Get<std::string>(2));

  Remove<std::string>(1);

  EXPECT_EQ(s1, Get<std::string>(1));

  std::string* s3 = Put<std::string>(3, "s3");
  EXPECT_EQ("s3", *s3);
  EXPECT_EQ(s1_dup, s3);
}

}  // namespace
}  // namespace tls_map
}  // namespace internal
}  // namespace merror
