/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "translator.hpp"

namespace lunatic {
namespace frontend {

Translator::Translator(CPU::Descriptor const& descriptor)
    : armv5te(descriptor.model == CPU::Descriptor::Model::ARM9)
    , max_block_size(descriptor.block_size)
    , exception_base(descriptor.exception_base)
    , memory(descriptor.memory)
    , coprocessors(descriptor.coprocessors) {
}

void Translator::Translate(BasicBlock& basic_block) {
  mode = basic_block.key.Mode();
  thumb_mode = basic_block.key.Thumb();
  opcode_size = thumb_mode ? sizeof(u16) : sizeof(u32);
  code_address = basic_block.key.Address() - 2 * opcode_size;
  this->basic_block = &basic_block;

  const Status status = thumb_mode ? TranslateThumb(basic_block) : TranslateARM(basic_block);

  /**
   * If we did not branch and execution can continue as normal,
   * then set the branch target to the sequentially next instruction to be executed.
   */
  if (status == Status::Continue && basic_block.branch_target.key.value == 0u) {
    const u32 next_pc = code_address + 2 * opcode_size;

    basic_block.branch_target.key = {next_pc, mode, thumb_mode};
    basic_block.branch_target.condition = Condition::AL;
  }
}

Status Translator::TranslateARM(BasicBlock& basic_block) {
  auto micro_block = BasicBlock::MicroBlock{};

  auto add_micro_block = [&]() {
    basic_block.micro_blocks.push_back(std::move(micro_block));
  };

  auto break_micro_block = [&](Condition condition) {
    add_micro_block();
    micro_block = {condition};
    emitter = &micro_block.emitter;
  };

  emitter = &micro_block.emitter;

  Status status = Status::Continue;

  for (int i = 0; i < max_block_size; i++) {
    auto instruction = memory.FastRead<u32, Memory::Bus::Code>(code_address);
    auto condition = bit::get_field<u32, Condition>(instruction, 28, 4);

    // ARMv5TE+ treats condition code 'NV' as a separate
    // encoding space for unpredicated instructions.
    if (armv5te && condition == Condition::NV) {
      condition = Condition::AL;
    }

    if (i == 0) {
      micro_block.condition = condition;
    } else if (condition != micro_block.condition) {
      break_micro_block(condition);
    }

    status = decode_arm(instruction, *this);

    if (status == Status::Unimplemented) {
      throw std::runtime_error(
        fmt::format("lunatic: unknown opcode 0x{:08X} @ 0x{:08X} (thumb={})",
          instruction, code_address, 0)
      );
    }

    basic_block.length++;
    micro_block.length++;

    if (status == Status::BreakMicroBlock && condition != Condition::AL) {
      break_micro_block(condition);
    }

    if (status == Status::BreakBasicBlock) {
      break;
    }

    code_address += sizeof(u32);
  }

  add_micro_block();

  return status;
}

Status Translator::TranslateThumb(BasicBlock& basic_block) {
  auto micro_block = BasicBlock::MicroBlock{Condition::AL};

  emitter = &micro_block.emitter;

  auto add_micro_block = [&]() {
    basic_block.micro_blocks.push_back(std::move(micro_block));
  };

  Status status = Status::Continue;

  for (int i = 0; i < max_block_size; i++) {
    u32 instruction;

    if (code_address & 2) {
      instruction  = memory.FastRead<u16, Memory::Bus::Code>(code_address + 0);
      instruction |= memory.FastRead<u16, Memory::Bus::Code>(code_address + 2) << 16;
    } else {
      instruction = memory.FastRead<u32, Memory::Bus::Code>(code_address);
    }

    // HACK: detect conditional branches and break the micro block early.
    if ((instruction & 0xF000) == 0xD000 && (instruction & 0xF00) != 0xF00) {
      auto condition = bit::get_field<u16, Condition>(instruction, 8, 4);

      if (i == 0) {
        micro_block.condition = condition;
      } else {
        add_micro_block();
        micro_block = {condition};
        emitter = &micro_block.emitter;
      }
    }

    status = decode_thumb(instruction, *this);

    if (status == Status::Unimplemented) {
      throw std::runtime_error(
        fmt::format("lunatic: unknown opcode 0x{:04X} @ 0x{:08X} (thumb={})",
          instruction, code_address, 1)
      );
    }

    basic_block.length++;
    micro_block.length++;

    if (status == Status::BreakBasicBlock) {
      break;
    }

    code_address += sizeof(u16);
  }

  add_micro_block();

  return status;
}

auto Translator::Undefined(u32 opcode) -> Status {
  return Status::Unimplemented;
}

void Translator::EmitUpdateNZ() {
  auto& cpsr_in  = emitter->CreateVar(IRDataType::UInt32, "cpsr_in");
  auto& cpsr_out = emitter->CreateVar(IRDataType::UInt32, "cpsr_out");

  emitter->LoadCPSR(cpsr_in);
  emitter->UpdateNZ(cpsr_out, cpsr_in);
  emitter->StoreCPSR(cpsr_out);
}

void Translator::EmitUpdateNZC() {
  auto& cpsr_in  = emitter->CreateVar(IRDataType::UInt32, "cpsr_in");
  auto& cpsr_out = emitter->CreateVar(IRDataType::UInt32, "cpsr_out");

  emitter->LoadCPSR(cpsr_in);
  emitter->UpdateNZC(cpsr_out, cpsr_in);
  emitter->StoreCPSR(cpsr_out);
}

void Translator::EmitUpdateNZCV() {
  auto& cpsr_in  = emitter->CreateVar(IRDataType::UInt32, "cpsr_in");
  auto& cpsr_out = emitter->CreateVar(IRDataType::UInt32, "cpsr_out");

  emitter->LoadCPSR(cpsr_in);
  emitter->UpdateNZCV(cpsr_out, cpsr_in);
  emitter->StoreCPSR(cpsr_out);
}

void Translator::EmitUpdateQ() {
  auto& cpsr_in  = emitter->CreateVar(IRDataType::UInt32, "cpsr_in");
  auto& cpsr_out = emitter->CreateVar(IRDataType::UInt32, "cpsr_out");

  emitter->LoadCPSR(cpsr_in);
  emitter->UpdateQ(cpsr_out, cpsr_in);
  emitter->StoreCPSR(cpsr_out);
}

void Translator::EmitAdvancePC() {
  emitter->StoreGPR(IRGuestReg{GPR::PC, mode}, IRConstant{code_address + opcode_size * 3});
}

void Translator::EmitFlush() {
  auto& cpsr_in = emitter->CreateVar(IRDataType::UInt32, "cpsr_in");
  auto& address_in  = emitter->CreateVar(IRDataType::UInt32, "address_in");
  auto& address_out = emitter->CreateVar(IRDataType::UInt32, "address_out");

  emitter->LoadCPSR(cpsr_in);
  emitter->LoadGPR(IRGuestReg{GPR::PC, mode}, address_in);
  emitter->Flush(address_out, address_in, cpsr_in);
  emitter->StoreGPR(IRGuestReg{GPR::PC, mode}, address_out);
}

void Translator::EmitFlushExchange(const IRVariable& address) {
  auto& address_out = emitter->CreateVar(IRDataType::UInt32, "address_out");
  auto& cpsr_in  = emitter->CreateVar(IRDataType::UInt32, "cpsr_in");
  auto& cpsr_out = emitter->CreateVar(IRDataType::UInt32, "cpsr_out");

  emitter->LoadCPSR(cpsr_in);
  emitter->FlushExchange(address_out, cpsr_out, address, cpsr_in);
  emitter->StoreGPR(IRGuestReg{GPR::PC, mode}, address_out);
  emitter->StoreCPSR(cpsr_out);
}

void Translator::EmitFlushNoSwitch() {
  auto& address_in  = emitter->CreateVar(IRDataType::UInt32, "address_in");
  auto& address_out = emitter->CreateVar(IRDataType::UInt32, "address_out");

  emitter->LoadGPR(IRGuestReg{GPR::PC, mode}, address_in);
  emitter->ADD(address_out, address_in, IRConstant{opcode_size * 2}, false);
  emitter->StoreGPR(IRGuestReg{GPR::PC, mode}, address_out);
}

void Translator::EmitLoadSPSRToCPSR() {
  auto& spsr = emitter->CreateVar(IRDataType::UInt32, "spsr");
  emitter->LoadSPSR(spsr, mode);
  emitter->StoreCPSR(spsr);
}

} // namespace lunatic::frontend
} // namespace lunatic
