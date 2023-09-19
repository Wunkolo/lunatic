/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "frontend/translator/translator.hpp"

namespace lunatic {
namespace frontend {

auto Translator::Handle(ARMBranchRelative const& opcode) -> Status {
  auto branch_address = code_address + opcode_size * 2 + opcode.offset;

  if (opcode.link) {
    // Note: thumb BL consists of two 16-bit opcodes.
    u32 link_address = code_address + sizeof(u32);
    if (thumb_mode) {
      link_address |= 1;
    }
    emitter->StoreGPR(IRGuestReg{GPR::LR, mode}, IRConstant{link_address});
  }

  if (opcode.exchange) {
    auto& cpsr_in  = emitter->CreateVar(IRDataType::UInt32, "cpsr_in");
    auto& cpsr_out = emitter->CreateVar(IRDataType::UInt32, "cpsr_out");
  
    emitter->LoadCPSR(cpsr_in);

    if (thumb_mode) {
      branch_address &= ~3;
      branch_address += sizeof(u32) * 2;
      emitter->BIC(cpsr_out, cpsr_in, IRConstant{32}, false);
    } else {
      branch_address += sizeof(u16) * 2;
      emitter->ORR(cpsr_out, cpsr_in, IRConstant{32}, false);
    }

    emitter->StoreCPSR(cpsr_out);
  } else {
    branch_address += opcode_size * 2;
  }

  emitter->StoreGPR(IRGuestReg{GPR::PC, mode}, IRConstant{branch_address});

  if (!opcode.exchange && opcode.condition == Condition::AL) {
    code_address = branch_address - opcode_size * 3;
    basic_block->branch_target.key = {};
    return Status::Continue;
  } else {
    if (opcode.exchange) {
      thumb_mode = !thumb_mode;
    }
    basic_block->branch_target.key = BasicBlock::Key{
      branch_address,
      mode,
      thumb_mode
    };
    basic_block->branch_target.condition = opcode.condition;
  }

  return Status::BreakBasicBlock;
}

} // namespace lunatic::frontend
} // namespace lunatic
