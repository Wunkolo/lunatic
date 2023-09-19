/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <fmt/format.h>
#include <lunatic/integer.hpp>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

#include "compiler.hpp"

namespace lunatic {

// T = data-type for object IDs (local to a pool)
// capacity = number of objects in a pool
// size = size of each object
template<typename T, size_t capacity, size_t size>
struct PoolAllocator {
  static constexpr size_t max_size = size;

  auto Allocate() -> void* {
    if (free_pools.head == nullptr) {
      free_pools.head = new Pool{};
      free_pools.tail = free_pools.head;
    }

    auto pool = free_pools.head;
    auto object = pool->Pop();

    if (pool->IsFull()) {
      // Remove pool from the front of the free list.
      free_pools.Remove(pool);

      // Add pool to the end of the full list.
      full_pools.Append(pool);
    }

    return object;
  }

  void Release(void* object) {
    auto obj = (typename Pool::Object*)object;
    auto pool = (Pool*)(obj - obj->id);

    if (pool->IsFull()) {
      // Remove pool from the full list.
      full_pools.Remove(pool);

      // Add pool to the end of the free list.
      free_pools.Append(pool);
    }

    pool->Push(obj);

    // TODO: keep track of how many objects are available
    // and decide if we should release the pool based on that.
    if (pool->IsEmpty()) {
      free_pools.Remove(pool);
      delete pool;
    }
  }

private:
  struct Pool {
    Pool() {
      T invert = capacity - 1;

      for (size_t id = 0; id < capacity; id++) {
        objects[id].id = id;
        
        stack.data[id] = invert - static_cast<T>(id);
      }

      stack.length = capacity;
    }

#ifdef _WIN32
    auto operator new(size_t) -> void* {
      return VirtualAlloc(nullptr, sizeof(Pool), MEM_COMMIT, PAGE_READWRITE);
    }

    void operator delete(void* object) {
      VirtualFree(object, sizeof(Pool), MEM_RELEASE);
    }
#endif

    bool IsFull() const { return stack.length == 0; }

    bool IsEmpty() const { return stack.length == capacity; }

    void Push(void* object) {
      stack.data[stack.length++] = ((Object*)object)->id;
    }

    auto Pop() -> void* {
      return &objects[stack.data[--stack.length]];
    }

    PACK(struct Object {
      u8 data[size];
      T id;
    }) objects[capacity];

    struct Stack {
      T data[capacity];
      size_t length;
    } stack;

    Pool* prev = nullptr;
    Pool* next = nullptr;
  };

  struct List {
    void Remove(Pool* pool) {
      if (pool->next == nullptr) {
        tail = pool->prev;
        if (tail) {
          tail->next = nullptr;
        }
      } else {
        pool->next->prev = pool->prev;
      }

      if (pool->prev == nullptr) {
        head = pool->next;
        if (head) {
          head->prev = nullptr;
        }
      } else {
        pool->prev->next = pool->next;
      }
    }

    void Append(Pool* pool) {
      if (head == nullptr) {
        head = pool;
        tail = pool;
        pool->prev = nullptr;
      } else {
        auto old_tail = tail;

        old_tail->next = pool;
        pool->prev = old_tail;
        tail = pool;
      }
      
      pool->next = nullptr;
    }

   ~List() {
      Pool* pool = head;

      while (pool != nullptr) {
        Pool* next_pool = pool->next;
        delete pool;
        pool = next_pool;
      }
    }

    Pool* head = nullptr;
    Pool* tail = nullptr;
  };

  List free_pools;
  List full_pools;
};

extern PoolAllocator<u16, 4096, 134> g_pool_alloc;

struct PoolObject {
  auto operator new(size_t size) -> void* {
#ifndef NDEBUG
    if (size > decltype(g_pool_alloc)::max_size) {
      throw std::runtime_error(
        fmt::format("PoolObject: requested size ({}) is larger than the supported maximum ({})",
          size, decltype(g_pool_alloc)::max_size));
    }
#endif
    return g_pool_alloc.Allocate();
  }

  void operator delete(void* object) {
    g_pool_alloc.Release(object);
  }
};

// Wrapper around our g_pool_alloc that implements the 'Allocator' named requirements:
// https://en.cppreference.com/w/cpp/named_req/Allocator
template<typename T>
struct StdPoolAlloc {
  static_assert(sizeof(T) <= decltype(g_pool_alloc)::max_size,
    "StdPoolAlloc: type exceeds maxmimum supported allocation size");

  using value_type = T;

  StdPoolAlloc() = default;

  template <typename T2>
  StdPoolAlloc(const StdPoolAlloc<T2>&) {}

  bool operator==(const StdPoolAlloc<T>&) const noexcept {
    return true;
  }

  bool operator!=(const StdPoolAlloc<T>&) const noexcept {
    return false;
  }

  auto allocate(std::size_t n) -> T* {
    return (T*)g_pool_alloc.Allocate();
  }

  void deallocate(T* p, size_t n) {
    g_pool_alloc.Release(p);
  }
};

} // namespace lunatic
