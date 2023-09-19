/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <functional>
#include <lunatic/integer.hpp>
#include <vector>

#include "common/pool_allocator.hpp"
#include "decode/definition/common.hpp"
#include "ir/emitter.hpp"
#include "state.hpp"

namespace lunatic {
namespace frontend {

struct BasicBlock : PoolObject {
  using CompiledFn = uintptr;

  union Key {
    Key() = default;

    explicit Key(State& state) {
      value  = state.GetGPR(Mode::User, GPR::PC) >> 1;
      value |= u64(state.GetCPSR().v & 0x3F) << 31; // mode and thumb bit
    }

    explicit Key(u64 value) : value(value) {}

    Key(u32 address, Mode mode, bool thumb) {
      value  = address >> 1;
      value |= static_cast<u64>(mode) << 31;
      if (thumb) {
        value |= 1ULL << 36;
      }
    }

    [[nodiscard]] auto Address() const -> u32 {
      return (value & 0x7FFFFFFF) << 1;
    }

    [[nodiscard]] auto Mode() const -> Mode {
      return static_cast<lunatic::Mode>((value >> 31) & 0x1F);
    }

    [[nodiscard]] bool Thumb() const {
      return value & (1ULL << 36);
    }

    [[nodiscard]] bool IsEmpty() const {
      return value == 0;
    }

    bool operator==(Key const& other) const {
      return value == other.value;
    }

    bool operator!=(Key const& other) const {
      return value != other.value;
    }

    explicit operator bool() const {
      return value != 0u;
    }
    
    // bits  0 - 30: address[31:1]
    // bits 31 - 35: CPU mode
    // bit       36: thumb-flag
    u64 value = 0;
  } key;

  BasicBlock() = default;
  explicit BasicBlock(Key key) : key(key) {}

 ~BasicBlock() {
    for (auto& callback : release_callbacks) callback(*this);
  }

  bool operator==(BasicBlock const& other) const {
    return key == other.key;
  }

  bool operator!=(BasicBlock const& other) const {
    return key != other.key;
  }

  void RegisterReleaseCallback(std::function<void(BasicBlock const&)> const& callback) {
    release_callbacks.push_back(callback);
  }

  int length = 0;

  struct MicroBlock {
    Condition condition;
    IREmitter emitter;
    int length = 0;
  };

  std::vector<MicroBlock> micro_blocks;

  // Pointer to the compiled code.
  CompiledFn function = (CompiledFn)0;

  struct BranchTarget {
    Key key{};
    u8* patch_location = nullptr;
    Condition condition = Condition::AL;
  } branch_target;

  std::vector<BasicBlock*> linking_blocks;

  u32 hash = 0;
  bool enable_fast_dispatch = true;
  bool uses_exception_base = false;

private:
  std::vector<std::function<void(BasicBlock const&)>> release_callbacks;
};

} // namespace lunatic::frontend
} // namespace lunatic

template<>
struct std::hash<lunatic::frontend::BasicBlock::Key> {
  std::size_t operator()(lunatic::frontend::BasicBlock::Key const& key) const {
    return std::hash<u64>{}(key.value);
  }
};