/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lunatic/coprocessor.hpp>
#include <lunatic/memory.hpp>
#include <memory>

namespace lunatic {

enum class GPR {
  R0 = 0,
  R1 = 1,
  R2 = 2,
  R3 = 3,
  R4 = 4,
  R5 = 5,
  R6 = 6,
  R7 = 7,
  R8 = 8,
  R9 = 9,
  R10 = 10,
  R11 = 11,
  R12 = 12,
  SP = 13,
  LR = 14,
  PC = 15,
};

enum class Mode : uint {
  User = 0x10,
  FIQ = 0x11,
  IRQ = 0x12,
  Supervisor = 0x13,
  Abort = 0x17,
  Undefined = 0x1B,
  System = 0x1F
};

union StatusRegister {
  struct {
    Mode mode : 5;
    uint thumb : 1;
    uint mask_fiq : 1;
    uint mask_irq : 1;
    uint reserved : 19;
    uint q : 1;
    uint v : 1;
    uint c : 1;
    uint z : 1;
    uint n : 1;
  } f;
  u32 v = static_cast<u32>(Mode::System);
};

struct CPU {
  struct Descriptor {
    Memory& memory;
    std::array<Coprocessor*, 16> coprocessors = {};
    u32 exception_base = 0;
    enum class Model {
      ARM7,
      ARM9
    } model = Model::ARM9;
    int block_size = 32;
  };

  virtual ~CPU() = default;

  virtual void Reset() = 0;
  virtual auto IRQLine() -> bool& = 0;
  virtual auto WaitForIRQ() -> bool& = 0;
  virtual auto GetExceptionBase() const -> u32 = 0;
  virtual void SetExceptionBase(u32 exception_base) = 0;
  virtual void ClearICache() = 0;
  virtual void ClearICacheRange(u32 address_lo, u32 address_hi) = 0;
  virtual auto Run(int cycles) -> int = 0;

  virtual auto GetGPR(GPR reg) const -> u32 = 0;
  virtual auto GetGPR(GPR reg, Mode mode) const -> u32 = 0;
  virtual auto GetCPSR() const -> StatusRegister = 0;
  virtual auto GetSPSR(Mode mode) const -> StatusRegister = 0;
  virtual void SetGPR(GPR reg, u32 value) = 0;
  virtual void SetGPR(GPR reg, Mode mode, u32 value) = 0;
  virtual void SetCPSR(StatusRegister value) = 0;
  virtual void SetSPSR(Mode mode, StatusRegister value) = 0;
};

auto CreateCPU(CPU::Descriptor const& descriptor) -> std::unique_ptr<CPU>;

} // namespace lunatic
