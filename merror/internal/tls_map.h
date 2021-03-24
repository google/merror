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
//
// This library implements a thread-local multimap from non-negative integers to
// arbitrary values. Its performance is optimized specifically for the use
// within `MTRY()`. The ABSL_PREDICT_TRUE and ABSL_PREDICT_FALSE hints in the
// code reflect the assumptions.
//
// A fairly close model would be `thread_local std::map<int, std::stack<Any>>`.
//
//                           EXAMPLE
//
//   string* s1 = Put<string>(1, "s1");
//   assert(*s1 == "s1");
//
//   string* s2 = Put<string>(2, "s2");
//   assert(*s2 == "s2");
//
//   string* s1_dup = Put<string>(1, "s1_dup");  // another value with key 1
//   assert(*s1_dup == "s1_dup");
//
//   assert(Get<string>(1) == s1_dup);  // the last value with key 1
//   assert(Get<string>(2) == s2);
//
//   Remove<string>(1);  // remove s1_dup
//
//   assert(Get<string>(1) == s1);  // the first value with key 1 is still there
//
//   string* s3 = Put<string>(3, "s3");
//   assert(*s3 == "s3");
//   assert(s3 == s1_dup);  // memory previously occupied by s1_dup is reused

#ifndef MERROR_5EDA97_INTERNAL_TLS_MAP_H_
#define MERROR_5EDA97_INTERNAL_TLS_MAP_H_

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace merror {
namespace internal {
namespace tls_map {

namespace internal_tls_map {

#ifdef __has_builtin
#define MERROR_HAVE_BUILTIN(x) __has_builtin(x)
#else
#define MERROR_HAVE_BUILTIN(x) 0
#endif

#ifdef __has_attribute
#define MERROR_HAVE_ATTRIBUTE(x) __has_attribute(x)
#else
#define MERROR_HAVE_ATTRIBUTE(x) 0
#endif

#if (MERROR_HAVE_BUILTIN(__builtin_expect) || \
     (defined(__GNUC__) && !defined(__clang__)))
#define MERROR_PREDICT_FALSE(x) (__builtin_expect(false || (x), false))
#define MERROR_PREDICT_TRUE(x) (__builtin_expect(false || (x), true))
#else
#define MERROR_PREDICT_FALSE(x) (x)
#define MERROR_PREDICT_TRUE(x) (x)
#endif

#if MERROR_HAVE_ATTRIBUTE(always_inline) || \
    (defined(__GNUC__) && !defined(__clang__))
#define MERROR_ATTRIBUTE_ALWAYS_INLINE __attribute__((always_inline))
#else
#define MERROR_ATTRIBUTE_ALWAYS_INLINE
#endif

// All symbols defined within namespace internal_tls_map are internal to
// tls_map.h. Do not reference them from other files.

template <size_t kSize, size_t kAlignment>
class Node {
 public:
  // The default memory allocator doesn't support overaligned types.
  static_assert(kAlignment <= alignof(std::max_align_t), "");

  // See free function Put() below.
  static Node* Put(int key) {
    assert(key >= 0);
    if (MERROR_PREDICT_TRUE(head_ != nullptr && head_->key_ == -1)) {
      // This case is handled specially as an optimization. It has no effect on
      // observable behavior.
      head_->key_ = key;
      return head_;
    }
    Node* res;
    for (Node** p = &head_; true; p = &(*p)->next_) {
      if (*p == nullptr) {
        // This branch is taken at least once per Node type per thread. In
        // practice that's also the upper bound.
        res = new Node;
        // Instantiate cleanup_ on the first allocation. When the current thread
        // terminates, ~Cleanup() will delete all nodes we've allocated.
        if (head_ == nullptr) static_cast<void>(&cleanup_);
        break;
      }
      if ((*p)->key_ == -1) {
        // This branch is never taken in practice.
        res = *p;
        *p = res->next_;
        break;
      }
    }
    res->key_ = key;
    res->next_ = head_;
    head_ = res;
    return res;
  }

  // See free function Get() below.
  static Node* Get(int key) {
    assert(key >= 0);
    Node* p = head_;
    assert(p != nullptr);
    while (MERROR_PREDICT_FALSE(p->key_ != key)) {
      p = p->next_;
      assert(p != nullptr);
    }
    return p;
  }

  // See free function Size() below.
  static size_t Size() {
    size_t size = 0;
    for (Node* p = head_; p; p = p->next_)
      if (p->key_ >= 0) ++size;
    return size;
  }

  // See free function Capacity() below.
  static size_t Capacity() {
    size_t size = 0;
    for (Node* p = head_; p; p = p->next_) ++size;
    return size;
  }

  // See free function Clear() below.
  template <class T>
  static void Clear() {
    while (head_) {
      Node* node = head_;
      head_ = node->next_;
      if (node->key_ >= 0) node->value<T>()->~T();
      delete node;
    }
  }

  template <class T>
  T* value() {
    return reinterpret_cast<T*>(&value_);
  }

  // Marks a regular node as free so that it can be reused by Put().
  void Release() {
    assert(key_ >= 0);
    key_ = -1;
  }

 private:
  struct Cleanup {
    ~Cleanup() {
      while (head_) {
        Node* node = head_;
        head_ = node->next_;
        assert(node->key_ == -1);
        delete node;
      }
    }
  };

  Node() {}
  ~Node() {}

  // Non-negative for regular nodes. -1 for empty nodes.
  int key_;
  // Null for the last node.
  Node* next_;
  typename std::aligned_storage<kSize, kAlignment>::type value_;

  // TODO(romanp): try changing the type of `head_` to `Node` and using zero
  // `key_` as a free node marker. See if it makes code simpler and/or faster.
  static thread_local Node* head_;
  static thread_local Cleanup cleanup_;
};

template <size_t kSize, size_t kAlignment>
thread_local Node<kSize, kAlignment>* Node<kSize, kAlignment>::head_;
template <size_t kSize, size_t kAlignment>
thread_local
    typename Node<kSize, kAlignment>::Cleanup Node<kSize, kAlignment>::cleanup_;

// TODO(romanp): use std::max() once C++14 is available.
constexpr size_t Max(size_t a, size_t b) { return a > b ? a : b; }

// To reduce the number of thread-local variables, put all small objects with
// fundamental alignment into the same map.
template <class T>
using NodeT =
    Node<Max(sizeof(T), 32), Max(alignof(T), alignof(std::max_align_t))>;

}  // namespace internal_tls_map

// Inserts a node with the specified key into the current thread's map. Reuses
// previously removed nodes whenever possible. Direct-initializes the value from
// the supplied arguments. Returns a pointer to the value.
//
// It's legal to put several values with the same key. All of them will be kept.
//
// Requires: `key >= 0`.
template <class T, class... Args>
inline MERROR_ATTRIBUTE_ALWAYS_INLINE T* Put(int key, Args&&... args) {
  return new (internal_tls_map::NodeT<T>::Put(key)->template value<T>())
      T(std::forward<Args>(args)...);
}

// Returns a pointer to the value associated with the specified key. If there
// are several such values, the last inserted value is returned.
//
// It's the responsibility of the caller to specify the correct value type.
//
// Requires: `key >= 0`.
// Requires: there is a value associated with the specified key in the current
// thread's map, and its type is `T`.
template <class T>
inline MERROR_ATTRIBUTE_ALWAYS_INLINE T* Get(int key) {
  return internal_tls_map::NodeT<T>::Get(key)->template value<T>();
}

// Destroys the value associated with the specified key and marks the node as
// free so that it can be reused by Put(). If there are several such values, the
// last inserted value is removed.
//
// It's the responsibility of the caller to specify the correct value type.
//
// Requires: `key >= 0`.
// Requires: there is a value associated with the specified key in the current
// thread's map, and its type is `T`.
template <class T>
inline MERROR_ATTRIBUTE_ALWAYS_INLINE void Remove(int key) {
  auto* node = internal_tls_map::NodeT<T>::Get(key);
  node->template value<T>()->~T();
  node->Release();
}

namespace testing {

// Symbols from this namespace can be used only from tls_map_test.cc.

// Returns the number of elements in the current thread's map.
//
// Precondition: all elements in the current thread's map must be of type `T`.
template <class T>
size_t Size() {
  return internal_tls_map::NodeT<T>::Size();
}

// Returns the number of regular and free nodes in the current thread's map.
//
// Precondition: all elements in the current thread's map must be of type `T`.
template <class T>
size_t Capacity() {
  return internal_tls_map::NodeT<T>::Capacity();
}

// Deletes all regular and free nodes in the current thread's map.
//
// Precondition: all elements in the current thread's map must be of type `T`.
// Postcondition: `Size<T>() == 0 && Capacity<T>() == 0`.
template <class T>
void Clear() {
  internal_tls_map::NodeT<T>::template Clear<T>();
}

}  // namespace testing

}  // namespace tls_map
}  // namespace internal
}  // namespace merror

#endif  // MERROR_5EDA97_INTERNAL_TLS_MAP_H_
