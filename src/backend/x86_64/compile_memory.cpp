/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <dynarmic/devirtualize_x64.hpp>

#include "common.hpp"

namespace lunatic::backend {

void X64Backend::CompileMemoryRead(CompileContext const& context, IRMemoryRead* op) {
  DESTRUCTURE_CONTEXT;

  Xbyak::Reg32 address_reg;
  auto& address = op->address;

  if (address.IsVariable()) {
    address_reg = reg_alloc.GetVariableHostReg(address.GetVar());
  } else {
    address_reg = reg_alloc.GetTemporaryHostReg();
    code.mov(address_reg, address.GetConst().value);
  }

  auto result_reg = reg_alloc.GetVariableHostReg(op->result.Get());
  auto flags = op->flags;

  auto label_slowmem = Xbyak::Label{};
  auto label_final = Xbyak::Label{};
  auto pagetable = memory.pagetable.get();

  code.push(rcx);

  auto& itcm = memory.itcm;
  auto& dtcm = memory.dtcm;

  auto itcm_reg = Xbyak::Reg64{};

  if (itcm.data != nullptr || dtcm.data != nullptr) {
    itcm_reg = reg_alloc.GetTemporaryHostReg().cvt64();
  }

  // TODO: deduplicate and clean this up in general.

  if (itcm.data != nullptr) {
    auto label_not_itcm = Xbyak::Label{};

    code.mov(itcm_reg, u64(&itcm));

    code.cmp(byte[itcm_reg + offsetof(Memory::TCM, config.enable_read)], 0);
    code.jz(label_not_itcm);

    code.cmp(address_reg, dword[itcm_reg + offsetof(Memory::TCM, config.base)]);
    code.jb(label_not_itcm);

    code.cmp(address_reg, dword[itcm_reg + offsetof(Memory::TCM, config.limit)]);
    code.ja(label_not_itcm);

    code.mov(rcx, u64(itcm.data));
    code.mov(result_reg, address_reg);
    code.sub(result_reg, dword[itcm_reg + offsetof(Memory::TCM, config.base)]);

    if (flags & Word) {
      code.and_(result_reg, itcm.mask & ~3);
      code.mov(result_reg, dword[rcx + result_reg.cvt64()]);
    } else if (flags & Half) {
      code.and_(result_reg, itcm.mask & ~1);
      if (flags & Signed) {
        code.movsx(result_reg, word[rcx + result_reg.cvt64()]);
      } else {
        code.movzx(result_reg, word[rcx + result_reg.cvt64()]);
      }
    } else if (flags & Byte) {
      code.and_(result_reg, itcm.mask);
      if (flags & Signed) {
        code.movsx(result_reg, byte[rcx + result_reg.cvt64()]);
      } else {
        code.movzx(result_reg, byte[rcx + result_reg.cvt64()]);
      }
    }

    code.jmp(label_final, Xbyak::CodeGenerator::T_NEAR);
    code.L(label_not_itcm);
  }

  auto dtcm_reg = itcm_reg;

  if (dtcm.data != nullptr) {
    auto label_not_dtcm = Xbyak::Label{};

    code.mov(dtcm_reg, u64(&dtcm));

    code.cmp(byte[dtcm_reg + offsetof(Memory::TCM, config.enable_read)], 0);
    code.jz(label_not_dtcm);

    code.cmp(address_reg, dword[dtcm_reg + offsetof(Memory::TCM, config.base)]);
    code.jb(label_not_dtcm);

    code.cmp(address_reg, dword[dtcm_reg + offsetof(Memory::TCM, config.limit)]);
    code.ja(label_not_dtcm);

    code.mov(rcx, u64(dtcm.data));
    code.mov(result_reg, address_reg);
    code.sub(result_reg, dword[dtcm_reg + offsetof(Memory::TCM, config.base)]);

    if (flags & Word) {
      code.and_(result_reg, dtcm.mask & ~3);
      code.mov(result_reg, dword[rcx + result_reg.cvt64()]);
    } else if (flags & Half) {
      code.and_(result_reg, dtcm.mask & ~1);
      if (flags & Signed) {
        code.movsx(result_reg, word[rcx + result_reg.cvt64()]);
      } else {
        code.movzx(result_reg, word[rcx + result_reg.cvt64()]);
      }
    } else if (flags & Byte) {
      code.and_(result_reg, dtcm.mask);
      if (flags & Signed) {
        code.movsx(result_reg, byte[rcx + result_reg.cvt64()]);
      } else {
        code.movzx(result_reg, byte[rcx + result_reg.cvt64()]);
      }
    }

    code.jmp(label_final);
    code.L(label_not_dtcm);
  }

  if (pagetable != nullptr) {
    code.mov(rcx, u64(pagetable));

    // Get the page table entry
    code.mov(result_reg, address_reg);
    code.shr(result_reg, Memory::kPageShift);
    code.mov(rcx, qword[rcx + result_reg.cvt64() * sizeof(uintptr)]);

    // Check if the entry is a null pointer.
    code.test(rcx, rcx);
    code.jz(label_slowmem);

    code.mov(result_reg, address_reg);

    if (flags & Word) {
      code.and_(result_reg, Memory::kPageMask & ~3);
      code.mov(result_reg, dword[rcx + result_reg.cvt64()]);
    } else if (flags & Half) {
      code.and_(result_reg, Memory::kPageMask & ~1);
      if (flags & Signed) {
        code.movsx(result_reg, word[rcx + result_reg.cvt64()]);
      } else {
        code.movzx(result_reg, word[rcx + result_reg.cvt64()]);
      }
    } else if (flags & Byte) {
      code.and_(result_reg, Memory::kPageMask);
      if (flags & Signed) {
        code.movsx(result_reg, byte[rcx + result_reg.cvt64()]);
      } else {
        code.movzx(result_reg, byte[rcx + result_reg.cvt64()]);
      }
    }

    code.jmp(label_final);
  }

  code.L(label_slowmem);

  auto stack_offset = 0x20U;

  code.push(rax);

  /**
   * Get caller-saved registers that need to be saved.
   * RCX already has been saved earlier.
   * RAX is handled separately, because we must read the 
   * return value of the called function from it later.
   */
  auto regs_saved = GetUsedHostRegsFromList(reg_alloc, {
    rdx, r8, r9, r10, r11,

    #ifdef ABI_SYSV
    rsi, rdi
    #endif
  });

  if ((regs_saved.size() % 2) == 0) stack_offset += sizeof(u64);

  Push(code, regs_saved);

  code.mov(kRegArg1.cvt32(), address_reg);

  if (flags & Word) {
    code.and_(kRegArg1.cvt32(), ~3);
    code.mov(rax, read_word_call.fn);
    code.mov(kRegArg0, read_word_call.arg);
  } else if (flags & Half) {
    code.and_(kRegArg1.cvt32(), ~1);
    code.mov(rax, read_half_call.fn);
    code.mov(kRegArg0, read_half_call.arg);
  } else if (flags & Byte) {
    code.mov(rax, read_byte_call.fn);
    code.mov(kRegArg0, read_byte_call.arg);
  }

  code.mov(kRegArg2.cvt32(), u32(Memory::Bus::Data));
  code.sub(rsp, stack_offset);
  code.call(rax);
  code.add(rsp, stack_offset);

  Pop(code, regs_saved);

  if (flags & Word) {
    code.mov(result_reg, eax);
  } else if (flags & Half) {
    if (flags & Signed) {
      code.movsx(result_reg, ax);
    } else {
      code.movzx(result_reg, ax);
    }
  } else if (flags & Byte) {
    if (flags & Signed) {
      code.movsx(result_reg, al);
    } else {
      code.movzx(result_reg, al);
    }
  }

  code.pop(rax);

  code.L(label_final);

  if (flags & Rotate) {
    if (flags & Word) {
      code.mov(ecx, address_reg);
      code.and_(cl, 3);
      code.shl(cl, 3);
      code.ror(result_reg, cl);
    } else if (flags & Half) {
      code.mov(ecx, address_reg);
      code.and_(cl, 1);
      code.shl(cl, 3);
      code.ror(result_reg, cl);
    }
  }

  static constexpr auto kHalfSignedARMv4T = Half | Signed | ARMv4T;

  /* ARM7TDMI/ARMv4T special case: unaligned LDRSH is effectively LDRSB.
   * TODO: this can probably be optimized by checking for misalignment early.
   */
  if ((flags & kHalfSignedARMv4T) == kHalfSignedARMv4T) {
    auto label_aligned = Xbyak::Label{};

    code.bt(address_reg, 0);
    code.jnc(label_aligned);
    code.shr(result_reg, 8);
    code.movsx(result_reg, result_reg.cvt8());
    code.L(label_aligned);
  }

  code.pop(rcx);
}

void X64Backend::CompileMemoryWrite(CompileContext const& context, IRMemoryWrite* op) {
  DESTRUCTURE_CONTEXT;

  Xbyak::Reg32 source_reg;
  auto& source = op->source;

  if (source.IsVariable()) {
    source_reg = reg_alloc.GetVariableHostReg(source.GetVar());
  } else {
    source_reg = reg_alloc.GetTemporaryHostReg();
    code.mov(source_reg, source.GetConst().value);
  }

  Xbyak::Reg32 address_reg;
  auto& address = op->address;

  if (address.IsVariable()) {
    address_reg = reg_alloc.GetVariableHostReg(address.GetVar());
  } else {
    address_reg = reg_alloc.GetTemporaryHostReg();
    code.mov(address_reg, address.GetConst().value);
  }

  auto scratch_reg = reg_alloc.GetTemporaryHostReg();
  auto flags = op->flags;

  auto label_slowmem = Xbyak::Label{};
  auto label_final = Xbyak::Label{};
  auto pagetable = memory.pagetable.get();

  code.push(rcx);

  auto& itcm = memory.itcm;
  auto& dtcm = memory.dtcm;

  auto itcm_reg = Xbyak::Reg64{};

  if (itcm.data != nullptr || dtcm.data != nullptr) {
    itcm_reg = reg_alloc.GetTemporaryHostReg().cvt64();
  }

  // TODO: deduplicate and clean this up in general.

  if (itcm.data != nullptr) {
    auto label_not_itcm = Xbyak::Label{};

    code.mov(itcm_reg, u64(&itcm));

    code.cmp(byte[itcm_reg + offsetof(Memory::TCM, config.enable)], 0);
    code.jz(label_not_itcm);

    code.cmp(address_reg, dword[itcm_reg + offsetof(Memory::TCM, config.base)]);
    code.jb(label_not_itcm);

    code.cmp(address_reg, dword[itcm_reg + offsetof(Memory::TCM, config.limit)]);
    code.ja(label_not_itcm);

    code.mov(rcx, u64(itcm.data));
    code.mov(scratch_reg, address_reg);
    code.sub(scratch_reg, dword[itcm_reg + offsetof(Memory::TCM, config.base)]);

    if (flags & Word) {
      code.and_(scratch_reg, itcm.mask & ~3);
      code.mov(dword[rcx + scratch_reg.cvt64()], source_reg);
    } else if (flags & Half) {
      code.and_(scratch_reg, itcm.mask & ~1);
      code.mov(word[rcx + scratch_reg.cvt64()], source_reg.cvt16());
    } else if (flags & Byte) {
      code.and_(scratch_reg, itcm.mask);
      code.mov(byte[rcx + scratch_reg.cvt64()], source_reg.cvt8());
    }

    code.jmp(label_final, Xbyak::CodeGenerator::T_NEAR);
    code.L(label_not_itcm);
  }

  auto dtcm_reg = itcm_reg;

  if (dtcm.data != nullptr) {
    auto label_not_dtcm = Xbyak::Label{};

    code.mov(dtcm_reg, u64(&dtcm));

    code.cmp(byte[dtcm_reg + offsetof(Memory::TCM, config.enable)], 0);
    code.jz(label_not_dtcm);

    code.cmp(address_reg, dword[dtcm_reg + offsetof(Memory::TCM, config.base)]);
    code.jb(label_not_dtcm);

    code.cmp(address_reg, dword[dtcm_reg + offsetof(Memory::TCM, config.limit)]);
    code.ja(label_not_dtcm);

    code.mov(rcx, u64(dtcm.data));
    code.mov(scratch_reg, address_reg);
    code.sub(scratch_reg, dword[dtcm_reg + offsetof(Memory::TCM, config.base)]);

    if (flags & Word) {
      code.and_(scratch_reg, dtcm.mask & ~3);
      code.mov(dword[rcx + scratch_reg.cvt64()], source_reg);
    } else if (flags & Half) {
      code.and_(scratch_reg, dtcm.mask & ~1);
      code.mov(word[rcx + scratch_reg.cvt64()], source_reg.cvt16());
    } else if (flags & Byte) {
      code.and_(scratch_reg, dtcm.mask);
      code.mov(byte[rcx + scratch_reg.cvt64()], source_reg.cvt8());
    }

    code.jmp(label_final);
    code.L(label_not_dtcm);
  }

  if (pagetable != nullptr) {
    code.mov(rcx, u64(pagetable));

    // Get the page table entry
    code.mov(scratch_reg, address_reg);
    code.shr(scratch_reg, Memory::kPageShift);
    code.mov(rcx, qword[rcx + scratch_reg.cvt64() * sizeof(uintptr)]);

    // Check if the entry is a null pointer.
    code.test(rcx, rcx);
    code.jz(label_slowmem);

    code.mov(scratch_reg, address_reg);

    if (flags & Word) {
      code.and_(scratch_reg, Memory::kPageMask & ~3);
      code.mov(dword[rcx + scratch_reg.cvt64()], source_reg);
    } else if (flags & Half) {
      code.and_(scratch_reg, Memory::kPageMask & ~1);
      code.mov(word[rcx + scratch_reg.cvt64()], source_reg.cvt16());
    } else if (flags & Byte) {
      code.and_(scratch_reg, Memory::kPageMask);
      code.mov(byte[rcx + scratch_reg.cvt64()], source_reg.cvt8());
    }

    code.jmp(label_final);
  }

  code.L(label_slowmem);

  auto stack_offset = 0x20U;

  // Get caller-saved registers that need to be saved.
  // RCX already has been saved earlier.
  auto regs_saved = GetUsedHostRegsFromList(reg_alloc, {
    rax, rdx, r8, r9, r10, r11,

    #ifdef ABI_SYSV
    rsi, rdi
    #endif
  });

  if ((regs_saved.size() % 2) == 1) stack_offset += sizeof(u64);

  Push(code, regs_saved);

  if (kRegArg1.cvt32() == source_reg) {
    code.mov(kRegArg2.cvt32(), address_reg);
    code.xchg(kRegArg1.cvt32(), kRegArg2.cvt32());

    if (flags & Half) {
      code.movzx(kRegArg2.cvt32(), kRegArg2.cvt16());
    } else if (flags & Byte) {
      code.movzx(kRegArg2.cvt32(), kRegArg2.cvt8());
    }
  } else {
    code.mov(kRegArg1.cvt32(), address_reg);

    if (flags & Word) {
      code.mov(kRegArg2.cvt32(), source_reg);
    } else if (flags & Half) {
      code.movzx(kRegArg2.cvt32(), source_reg.cvt16());
    } else if (flags & Byte) {
      code.movzx(kRegArg2.cvt32(), source_reg.cvt8());
    }
  }

  if (flags & Word) {
    code.and_(kRegArg1.cvt32(), ~3);
    code.mov(rax, write_word_call.fn);
    code.mov(kRegArg0, write_word_call.arg);
  } else if (flags & Half) {
    code.and_(kRegArg1.cvt32(), ~1);
    code.mov(rax, write_half_call.fn);
    code.mov(kRegArg0, write_half_call.arg);
  } else if (flags & Byte) {
    code.mov(rax, write_byte_call.fn);
    code.mov(kRegArg0, write_byte_call.arg);
  }

  code.mov(kRegArg3.cvt32(), u32(Memory::Bus::Data));
  code.sub(rsp, stack_offset);
  code.call(rax);
  code.add(rsp, stack_offset);

  Pop(code, regs_saved);

  code.L(label_final);
  code.pop(rcx);
}

} // namespace lunatic::backend
