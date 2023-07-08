/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <cstdlib>

#ifdef __APPLE__
#include <libkern/OSCacheControl.h>
#endif

#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

namespace memory {
  enum MemoryProtection {
    MemoryProtection_None = 0,
    MemoryProtection_Read = 1 << 0,
    MemoryProtection_Write = 1 << 1,
    MemoryProtection_Execute = 1 << 2,
  };

#ifdef _WIN32
  static std::size_t s_PageSize = 4096;
#else
  static std::size_t s_PageSize = sysconf(_SC_PAGESIZE);
#endif



  class MemoryBlock {
  private:
    static std::size_t CalculateRealSize(std::size_t size) {
      return (size + s_PageSize) & ~(s_PageSize - 1);
    }

#ifdef _WIN32
    static DWORD TranslateMemoryProtection(MemoryProtection prot) {
      bool has_read;
      bool has_write;
      bool has_execute;

      has_read = (prot & MemoryProtection_Read) == MemoryProtection_Read;
      has_write = (prot & MemoryProtection_Write) == MemoryProtection_Write;
      has_execute = (prot & MemoryProtection_Execute) == MemoryProtection_Execute;

      if (has_write && !has_execute) {
        return PAGE_READWRITE;
      } else if (has_read && has_write && has_execute) {
        return PAGE_EXECUTE_READWRITE;
      } else if (has_read && has_execute) {
        return PAGE_EXECUTE_READ;
      } else if (has_execute) {
        return PAGE_EXECUTE;
      } else if (has_read) {
        return PAGE_READONLY;
      } else {
        return PAGE_NOACCESS;
      }
    }
#else
    static int TranslateMemoryProtection(MemoryProtection prot) {
      int native_prot = PROT_NONE;

      if ((prot & MemoryProtection_Read) == MemoryProtection_Read) {
        native_prot |= PROT_READ;
      }

      if ((prot & MemoryProtection_Write) == MemoryProtection_Write) {
        native_prot |= PROT_WRITE;
      }

      if ((prot & MemoryProtection_Execute) == MemoryProtection_Execute) {
        native_prot |= PROT_EXEC;
      }

      return native_prot;
    }
#endif

    static void* AllocateMemory(std::size_t aligned_size, MemoryProtection prot) {
      auto native_prot = MemoryBlock::TranslateMemoryProtection(prot);

#ifdef _WIN32
      return VirtualAlloc(nullptr, aligned_size, MEM_COMMIT, native_prot);
#else
      int mode = MAP_PRIVATE;

      #if defined(MAP_ANONYMOUS)
        mode |= MAP_ANONYMOUS;
      #elif defined(MAP_ANON)
        mode |= MAP_ANON;
      #else
        #error "Anonymous page not supported"
      #endif

      #if defined(MAP_JIT)
        mode |= MAP_JIT;
      #endif

      void* p = mmap(nullptr, aligned_size, native_prot, mode, -1, 0);

      if (p == MAP_FAILED) {
        return nullptr;
      }

      return p;
#endif
    }

    static void FreeMemory(void *ptr, size_t size) {
#ifdef _WIN32
      VirtualFree(ptr, size, MEM_RELEASE);
#else
      munmap(ptr, size);
#endif
    }

  public:
    void *ptr;
    std::size_t size;
    std::size_t real_size;

    explicit MemoryBlock(std::size_t size, MemoryProtection prot) : size(size), real_size(MemoryBlock::CalculateRealSize(size)) {
      this->ptr = MemoryBlock::AllocateMemory(this->real_size, prot);

      if (this->ptr == nullptr) {
        throw new std::bad_alloc();
      }
    }

    ~MemoryBlock() {
      if (this->ptr != nullptr) {
        MemoryBlock::FreeMemory(this->ptr, this->real_size);
      }
    }

    bool Protect(MemoryProtection prot) {
      auto native_prot = MemoryBlock::TranslateMemoryProtection(prot);
#ifdef _WIN32
      DWORD oldProtect = 0;
      return VirtualProtect(this->ptr, this->real_size, native_prot, &oldProtect) != 0;
#else
      return mprotect(this->ptr, this->real_size, native_prot) == 0;
#endif
    }
  };

  class CodeBlockMemory {
    MemoryBlock memory_block;

    #if (defined (__APPLE__) && defined(__aarch64__)) || defined(_WIN32)
    static const MemoryProtection InitialMemoryProtection = static_cast<MemoryProtection>(MemoryProtection_Read | MemoryProtection_Write | MemoryProtection_Execute);
    #else
    static const MemoryProtection InitialMemoryProtection = static_cast<MemoryProtection>(MemoryProtection_Read | MemoryProtection_Write);
    #endif

  public:
    CodeBlockMemory(std::size_t size) : memory_block(size, InitialMemoryProtection) {
      #if defined(__APPLE__) && defined(__aarch64__)
      ProtectForWrite();
      #endif
    }

    bool ProtectForWrite() {
      #if defined(_WIN32)
      return true;
      #elif defined(__APPLE__) && defined(__aarch64__)
      pthread_jit_write_protect_np(0);
      return true;
      #else
      return memory_block.Protect(static_cast<MemoryProtection>(MemoryProtection_Read | MemoryProtection_Write));
      #endif
    }

    bool ProtectForExecute() {
      #if defined(_WIN32)
      return true;
      #elif defined(__APPLE__) && defined(__aarch64__)
      pthread_jit_write_protect_np(1);
      return true;
      #else
      return memory_block.Protect(static_cast<MemoryProtection>(MemoryProtection_Read | MemoryProtection_Execute));
      #endif
    }

    void Invalidate() {
      #if defined (__APPLE__)
        sys_icache_invalidate(memory_block.ptr, memory_block.real_size);
      #elif defined(_WIN32)
        FlushInstructionCache(GetCurrentProcess(), memory_block.ptr, memory_block.real_size);
      #elif defined(__linux__) && defined(__aarch64__)
        #error "Implement cache invalidation logic for linux aarch64"
      #endif
    }

    void *GetPointer() {
      return memory_block.ptr;
    }
  };


} // namespace memory
