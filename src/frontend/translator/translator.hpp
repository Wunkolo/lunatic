/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lunatic/memory.hpp>

#include "frontend/decode/arm.hpp"
#include "frontend/decode/thumb.hpp"
#include "frontend/basic_block.hpp"

namespace lunatic {
namespace frontend {

// TODO: can we move this into Translator despite the templated base class?
enum class Status {
  Continue,
  BreakBasicBlock,
  BreakMicroBlock,
  Unimplemented
};

struct Translator final : ARMDecodeClient<Status> {
  Translator(CPU::Descriptor const& descriptor);

  void SetExceptionBase(u32 new_exception_base) {
    exception_base = new_exception_base;
  }

  void Translate(BasicBlock& basic_block);

  auto Handle(ARMDataProcessing const& opcode) -> Status override;
  auto Handle(ARMMoveStatusRegister const& opcode) -> Status override;
  auto Handle(ARMMoveRegisterStatus const& opcode) -> Status override;
  auto Handle(ARMMultiply const& opcode) -> Status override;
  auto Handle(ARMMultiplyLong const& opcode) -> Status override;
  auto Handle(ARMSingleDataSwap const& opcode) -> Status override;
  auto Handle(ARMBranchExchange const& opcode) -> Status override;
  auto Handle(ARMHalfwordSignedTransfer const& opcode) -> Status override;
  auto Handle(ARMSingleDataTransfer const& opcode) -> Status override;
  auto Handle(ARMBlockDataTransfer const& opcode) -> Status override;
  auto Handle(ARMBranchRelative const& opcode) -> Status override;
  auto Handle(ARMCoprocessorRegisterTransfer const& opcode) -> Status override;
  auto Handle(ARMException const& opcode) -> Status override;
  auto Handle(ARMCountLeadingZeros const& opcode) -> Status override;
  auto Handle(ARMSaturatingAddSub const& opcode) -> Status override;
  auto Handle(ARMSignedHalfwordMultiply const& opcode) -> Status override;
  auto Handle(ARMSignedWordHalfwordMultiply const& opcode) -> Status override;
  auto Handle(ARMSignedHalfwordMultiplyAccumulateLong const& opcode) -> Status override;
  auto Handle(ThumbBranchLinkSuffix const& opcode) -> Status override;
  auto Undefined(u32 opcode) -> Status override;

private:
  Status TranslateARM(BasicBlock& basic_block);
  Status TranslateThumb(BasicBlock& basic_block);

  void EmitUpdateNZ();
  void EmitUpdateNZC();
  void EmitUpdateNZCV();
  void EmitUpdateQ();
  void EmitAdvancePC();
  void EmitFlush();
  void EmitFlushExchange(IRVariable const& address);
  void EmitFlushNoSwitch();
  void EmitLoadSPSRToCPSR();

  // TODO: deduce opcode_size from thumb_mode. this is redundant.
  u32 code_address;
  bool thumb_mode;
  u32 opcode_size;
  Mode mode;
  bool armv5te;
  int  max_block_size;
  u32  exception_base;
  Memory& memory;
  std::array<Coprocessor*, 16> coprocessors;
  IREmitter* emitter = nullptr;
  BasicBlock* basic_block = nullptr;
};

} // namespace lunatic::frontend
} // namespace lunatic
