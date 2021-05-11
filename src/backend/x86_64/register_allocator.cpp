/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <iterator>

#include "register_allocator.hpp"

using namespace lunatic::frontend;
using namespace Xbyak::util;

namespace lunatic {
namespace backend {

X64RegisterAllocator::X64RegisterAllocator(
  IREmitter const& emitter,
  Xbyak::CodeGenerator& code
) : emitter(emitter), code(code) {
  // Static allocation:
  //   - rax: host flags via lahf (overflow flag in al)
  //   - rcx: pointer to guest state (lunatic::frontend::State)
  //   - rbp: pointer to stack frame / spill area.
  free_host_regs = {
    edx,
    ebx,
    esi,
    edi,
    r8d,
    r9d,
    r10d,
    r11d,
    r12d,
    r13d,
    r14d,
    r15d
  };

  number_of_vars = emitter.Vars().size();
  var_id_to_host_reg.resize(number_of_vars);
  var_id_to_point_of_last_use.resize(number_of_vars);
  free_spill_bitmap.reset();
  var_id_to_spill_slot.resize(number_of_vars);

  EvaluateVariableLifetimes();
}

auto X64RegisterAllocator::GetVariableHostReg(IRVariable const& var, int location) -> Xbyak::Reg32 {
  // Check if the variable is already allocated to a register at the moment.
  auto maybe_reg = var_id_to_host_reg[var.id];
  if (maybe_reg.HasValue()) {
    return maybe_reg.Unwrap();
  }

  // Release any registers that are allocated to expired variables first.
  ReleaseDeadVariables(location);

  auto reg = FindFreeHostReg(location);
  
  // If the variable was spilled previously then restore its previous value.
  auto maybe_spill = var_id_to_spill_slot[var.id];
  if (maybe_spill.HasValue()) {
    auto slot = maybe_spill.Unwrap();
    code.mov(reg, dword[rbp + slot * sizeof(u32)]);
    free_spill_bitmap[slot] = false;
    var_id_to_spill_slot[var.id] = {};
  }

  var_id_to_host_reg[var.id] = reg;
  return reg;
}

void X64RegisterAllocator::EvaluateVariableLifetimes() {
  for (auto const& var : emitter.Vars()) {
    int expiration_point = -1;
    int location = 0;

    for (auto const& op : emitter.Code()) {
      if (op->Writes(*var) || op->Reads(*var)) {
        expiration_point = location;
      }

      location++;
    }

    if (expiration_point != -1) {
      var_id_to_point_of_last_use[var->id] = expiration_point;
    }
  }
}

void X64RegisterAllocator::ReleaseDeadVariables(int location) {
  for (auto const& var : emitter.Vars()) {
    auto expiration_point = var_id_to_point_of_last_use[var->id];

    if (location > expiration_point) {
      auto maybe_reg = var_id_to_host_reg[var->id];
      if (maybe_reg.HasValue()) {
        free_host_regs.push_back(maybe_reg.Unwrap());
        var_id_to_host_reg[var->id] = {};
      }
    }
  }
}

auto X64RegisterAllocator::FindFreeHostReg(int location) -> Xbyak::Reg32 {
  if (free_host_regs.size() != 0) {
    auto reg = free_host_regs.back();
    free_host_regs.pop_back();
    return reg;
  }

  auto reg = Xbyak::Reg32{};
  auto var_id = 0;

  // Find a variable to be spilled and deallocate it.
  // TODO: think of a smart way to pick which variable/register to spill.
  for (int i = 0; i < number_of_vars; i++) {
    // TODO: this is sloooooooooow!!!
    auto it = emitter.Code().begin();
    std::advance(it, location);
    auto const& op = *it;    
    auto const& var = *emitter.Vars()[i];

    // Do not spill a variable that we will need during the current operation.
    if (op->Reads(var)) {
      continue;
    }

    if (var_id_to_host_reg[i].HasValue()) {
      reg = var_id_to_host_reg[i].Unwrap();
      var_id_to_host_reg[i] = {};
      var_id = i;
      break;
    }
  }

  // Spill the variable into one of the free slots.
  for (int i = 0; i < kSpillAreaSize; i++) {
    if (!free_spill_bitmap[i]) {
      free_spill_bitmap[i] = true;
      var_id_to_spill_slot[var_id] = i;
      code.mov(dword[rbp + i * sizeof(u32)], reg);
      break;
    }
  }

  throw std::runtime_error("X64RegisterAllocator: out of registers and spill space.");
}

} // namespace lunatic::backend
} // namespace lunatic
