/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "frontend/translator/translator.hpp"

namespace lunatic {
namespace frontend {

auto Translator::Handle(ARMException const& opcode) -> Status {
  Mode new_mode;
  auto exception = opcode.exception;
  u32 branch_address = exception_base + static_cast<u32>(exception) + sizeof(u32) * 2;

  basic_block->uses_exception_base = true;

  switch (exception) {
    case Exception::Supervisor:
      new_mode = Mode::Supervisor;
      break;
    default:
      throw std::runtime_error(fmt::format("unhandled exception vector: 0x{:X}", static_cast<int>(exception)));
  }

  auto& cpsr_old = emitter->CreateVar(IRDataType::UInt32, "cpsr_old");

  // Save current PSR in the saved PSR.
  emitter->LoadCPSR(cpsr_old);
  emitter->StoreSPSR(cpsr_old, new_mode);

  // Enter supervisor mode and disable IRQs.
  auto& tmp = emitter->CreateVar(IRDataType::UInt32);
  auto& cpsr_new = emitter->CreateVar(IRDataType::UInt32, "cpsr_new");
  emitter->AND(tmp, cpsr_old, IRConstant{~0x3FU}, false);
  emitter->ORR(cpsr_new, tmp, IRConstant{static_cast<u32>(new_mode) | 0x80}, false);
  emitter->StoreCPSR(cpsr_new);

  // Save next PC in LR
  emitter->StoreGPR(IRGuestReg{GPR::LR, new_mode}, IRConstant{code_address + opcode_size});

  // Set PC to the exception vector.
  emitter->StoreGPR(IRGuestReg{GPR::PC, new_mode}, IRConstant{branch_address});

  if (opcode.condition == Condition::AL && !thumb_mode) {
    code_address = branch_address - 3 * opcode_size;
    mode = new_mode;
    basic_block->branch_target.key = {};
    return Status::Continue;
  } else {
    basic_block->branch_target.key = BasicBlock::Key{
      branch_address,
      new_mode,
      false
    };
    basic_block->branch_target.condition = opcode.condition;
  }

  return Status::BreakBasicBlock;
}

} // namespace lunatic::frontend
} // namespace lunatic
