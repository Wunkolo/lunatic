/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.hpp"

namespace lunatic::backend {

void X64Backend::CompileClearCarry(CompileContext const& context, IRClearCarry* op) {
  context.code.and_(ah, ~1);
}

void X64Backend::CompileSetCarry(CompileContext const& context, IRSetCarry* op) {
  context.code.or_(ah, 1);
}

void X64Backend::CompileUpdateFlags(CompileContext const& context, IRUpdateFlags* op) {
  DESTRUCTURE_CONTEXT;

  u32 mask = 0;
  auto& result_var = op->result.Get();
  auto& input_var = op->input.Get();
  auto input_reg  = reg_alloc.GetVariableHostReg(input_var);

  reg_alloc.ReleaseVarAndReuseHostReg(input_var, result_var);

  auto result_reg = reg_alloc.GetVariableHostReg(result_var);

  if (op->flag_n) mask |= 0x80000000;
  if (op->flag_z) mask |= 0x40000000;
  if (op->flag_c) mask |= 0x20000000;
  if (op->flag_v) mask |= 0x10000000;

  auto flags_reg = reg_alloc.GetTemporaryHostReg();

  // Convert NZCV bits from AX register into the guest format.
  // Clear the bits which are not to be updated.
  if (host_cpu.has(Xbyak::util::Cpu::tBMI1)) {
    auto pext_mask_reg = reg_alloc.GetTemporaryHostReg();

    code.mov(pext_mask_reg, 0xC101);
    code.pext(flags_reg, eax, pext_mask_reg);
    code.shl(flags_reg, 28);
  } else {
    code.mov(flags_reg, eax);
    code.and_(flags_reg, 0xC101);
    code.imul(flags_reg, flags_reg, 0x1021'0000);
  }

  code.and_(flags_reg, mask);

  if (result_reg != input_reg) {
    code.mov(result_reg, input_reg);
  }

  // Clear the bits to be updated, then OR the new values.
  code.and_(result_reg, ~mask);
  code.or_(result_reg, flags_reg);
}

void X64Backend::CompileUpdateSticky(CompileContext const& context, IRUpdateSticky* op) {
  DESTRUCTURE_CONTEXT;

  auto result_reg = reg_alloc.GetVariableHostReg(op->result.Get());
  auto input_reg  = reg_alloc.GetVariableHostReg(op->input.Get());

  code.movzx(result_reg, al);
  code.shl(result_reg, 27);
  code.or_(result_reg, input_reg);
}

} // namespace lunatic::backend
