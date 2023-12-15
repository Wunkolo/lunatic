/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.hpp"

namespace lunatic::backend {

void X64Backend::CompileAND(CompileContext const& context, IRBitwiseAND* op) {
  DESTRUCTURE_CONTEXT;

  auto& lhs_var = op->lhs.Get();
  auto  lhs_reg = reg_alloc.GetVariableHostReg(lhs_var);

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    if (op->result.IsNull()) {
      code.test(lhs_reg, imm);
    } else {
      auto& result_var = op->result.Unwrap();

      reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);

      auto result_reg = reg_alloc.GetVariableHostReg(result_var);

      if (result_reg != lhs_reg) {
        code.mov(result_reg, lhs_reg);
      }
      code.and_(result_reg, imm);
    }
  } else {
    auto& rhs_var = op->rhs.GetVar();
    auto  rhs_reg = reg_alloc.GetVariableHostReg(rhs_var);

    if (op->result.IsNull()) {
      code.test(lhs_reg, rhs_reg);
    } else {
      auto& result_var = op->result.Unwrap(); 

      reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);
      reg_alloc.ReleaseVarAndReuseHostReg(rhs_var, result_var);

      auto result_reg = reg_alloc.GetVariableHostReg(result_var);

      if (result_reg == lhs_reg) {
        code.and_(lhs_reg, rhs_reg);
      } else if (result_reg == rhs_reg) {
        code.and_(rhs_reg, lhs_reg);
      } else {
        code.mov(result_reg, lhs_reg);
        code.and_(result_reg, rhs_reg);
      }
    }
  }

  if (op->update_host_flags) {
    // load flags but preserve carry
    code.bt(ax, 8); // CF = value of bit8
    code.lahf();
  }
}

void X64Backend::CompileBIC(CompileContext const& context, IRBitwiseBIC* op) {
  DESTRUCTURE_CONTEXT;

  auto& result_var = op->result.Unwrap();
  auto& lhs_var = op->lhs.Get();
  auto  lhs_reg = reg_alloc.GetVariableHostReg(op->lhs.Get());

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);

    auto result_reg = reg_alloc.GetVariableHostReg(result_var);

    if (result_reg != lhs_reg) {
      code.mov(result_reg, lhs_reg);
    }
    code.and_(result_reg, ~imm);
  } else {
    auto& rhs_var = op->rhs.GetVar();
    auto  rhs_reg = reg_alloc.GetVariableHostReg(rhs_var);

    reg_alloc.ReleaseVarAndReuseHostReg(rhs_var, result_var);

    auto result_reg = reg_alloc.GetVariableHostReg(result_var);

    if (result_reg != rhs_reg) {
      code.mov(result_reg, rhs_reg);
    }
    code.not_(result_reg);
    code.and_(result_reg, lhs_reg);
  }

  if (op->update_host_flags) {
    // load flags but preserve carry
    code.bt(ax, 8); // CF = value of bit8
    code.lahf();
  }
}

void X64Backend::CompileEOR(CompileContext const& context, IRBitwiseEOR* op) {
  DESTRUCTURE_CONTEXT;

  auto& lhs_var = op->lhs.Get();
  auto  lhs_reg = reg_alloc.GetVariableHostReg(lhs_var);

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    if (op->result.IsNull()) {
      // TODO: optimize this? use a temporary register?
      code.push(lhs_reg.cvt64());
      code.xor_(lhs_reg, imm);
      code.pop(lhs_reg.cvt64());
    } else {
      auto& result_var = op->result.Unwrap();

      reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);

      auto result_reg = reg_alloc.GetVariableHostReg(result_var);

      if (result_reg != lhs_reg) {
        code.mov(result_reg, lhs_reg);
      }
      code.xor_(result_reg, imm);
    }
  } else {
    auto& rhs_var = op->rhs.GetVar();
    auto  rhs_reg = reg_alloc.GetVariableHostReg(rhs_var);

    if (op->result.IsNull()) {
      // TODO: optimize this? use a temporary register?
      code.push(lhs_reg.cvt64());
      code.xor_(lhs_reg, rhs_reg);
      code.pop(lhs_reg.cvt64());
    } else {
      auto& result_var = op->result.Unwrap();

      reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);
      reg_alloc.ReleaseVarAndReuseHostReg(rhs_var, result_var);

      auto result_reg = reg_alloc.GetVariableHostReg(result_var);

      if (result_reg == lhs_reg) {
        code.xor_(lhs_reg, rhs_reg);
      } else if (result_reg == rhs_reg) {
        code.xor_(rhs_reg, lhs_reg);
      } else {
        code.mov(result_reg, lhs_reg);
        code.xor_(result_reg, rhs_reg);
      }
    }
  }

  if (op->update_host_flags) {
    // load flags but preserve carry
    code.bt(ax, 8); // CF = value of bit8
    code.lahf();
  }
}

void X64Backend::CompileSUB(CompileContext const& context, IRSub* op) {
  DESTRUCTURE_CONTEXT;

  auto& lhs_var = op->lhs.Get();
  auto  lhs_reg = reg_alloc.GetVariableHostReg(lhs_var);

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    if (op->result.IsNull()) {
      code.cmp(lhs_reg, imm);
    } else {
      auto& result_var = op->result.Unwrap();

      reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);

      auto result_reg = reg_alloc.GetVariableHostReg(result_var);

      if (result_reg != lhs_reg) {
        code.mov(result_reg, lhs_reg);
      }
      code.sub(result_reg, imm);
    }
  } else {
    auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.GetVar());

    if (op->result.IsNull()) {
      code.cmp(lhs_reg, rhs_reg);
    } else {
      auto& result_var = op->result.Unwrap();

      reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);

      auto result_reg = reg_alloc.GetVariableHostReg(result_var);

      if (result_reg != lhs_reg) {
        code.mov(result_reg, lhs_reg);
      }
      code.sub(result_reg, rhs_reg);
    }
  }

  if (op->update_host_flags) {
    code.cmc();
    code.lahf();
    code.seto(al);
  }
}

void X64Backend::CompileRSB(CompileContext const& context, IRRsb* op) {
  DESTRUCTURE_CONTEXT;

  auto& result_var = op->result.Unwrap();
  auto& lhs_var = op->lhs.Get();
  auto lhs_reg = reg_alloc.GetVariableHostReg(lhs_var);

  if(op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    if(imm == 0u) {
      reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);

      const Xbyak::Reg32 result_reg = reg_alloc.GetVariableHostReg(result_var);

      if(result_reg != lhs_reg) {
        code.mov(result_reg, lhs_reg);
      }
      code.neg(result_reg);
    } else {
      const Xbyak::Reg32 result_reg = reg_alloc.GetVariableHostReg(result_var);
      code.mov(result_reg, imm);
      code.sub(result_reg, lhs_reg);
    }
  } else {
    auto& rhs_var = op->rhs.GetVar();
    auto  rhs_reg = reg_alloc.GetVariableHostReg(rhs_var);

    reg_alloc.ReleaseVarAndReuseHostReg(rhs_var, result_var);

    auto result_reg = reg_alloc.GetVariableHostReg(result_var);

    if(result_reg != rhs_reg) {
      code.mov(result_reg, rhs_reg);
    }
    code.sub(result_reg, lhs_reg);
  }

  if(op->update_host_flags) {
    code.cmc();
    code.lahf();
    code.seto(al);
  }
}

void X64Backend::CompileADD(CompileContext const& context, IRAdd* op) {
  DESTRUCTURE_CONTEXT;

  auto& lhs_var = op->lhs.Get();
  auto  lhs_reg = reg_alloc.GetVariableHostReg(lhs_var);

  if (op->result.IsNull() && !op->update_host_flags) {
    return;
  }

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    if (op->result.IsNull()) {
      // eax will be trashed by lahf anyways
      code.mov(eax, lhs_reg);
      code.add(eax, imm);
    } else {
      auto& result_var = op->result.Unwrap();

      reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);

      auto result_reg = reg_alloc.GetVariableHostReg(result_var);

      if (result_reg != lhs_reg) {
        code.mov(result_reg, lhs_reg);
      }
      code.add(result_reg, imm);
    }
  } else {
    auto& rhs_var = op->rhs.GetVar();
    auto  rhs_reg = reg_alloc.GetVariableHostReg(rhs_var);

    if (op->result.IsNull()) {
      // eax will be trashed by lahf anyways
      code.mov(eax, lhs_reg);
      code.add(eax, rhs_reg);
    } else {
      auto& result_var = op->result.Unwrap();

      reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);
      reg_alloc.ReleaseVarAndReuseHostReg(rhs_var, result_var);

      auto result_reg = reg_alloc.GetVariableHostReg(result_var);

      if (result_reg == lhs_reg) {
        code.add(lhs_reg, rhs_reg);
      } else if (result_reg == rhs_reg) {
        code.add(rhs_reg, lhs_reg);
      } else {
        code.mov(result_reg, lhs_reg);
        code.add(result_reg, rhs_reg);
      }
    }
  }

  if (op->update_host_flags) {
    code.lahf();
    code.seto(al);
  }
}

void X64Backend::CompileADC(CompileContext const& context, IRAdc* op) {
  DESTRUCTURE_CONTEXT;

  auto& result_var = op->result.Unwrap();
  auto& lhs_var = op->lhs.Get();
  auto  lhs_reg = reg_alloc.GetVariableHostReg(lhs_var);

  code.sahf();

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);

    auto result_reg = reg_alloc.GetVariableHostReg(result_var);

    if (result_reg != lhs_reg) {
      code.mov(result_reg, lhs_reg);
    }
    code.adc(result_reg, imm);
  } else {
    auto& rhs_var = op->rhs.GetVar();
    auto  rhs_reg = reg_alloc.GetVariableHostReg(rhs_var);

    reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);
    reg_alloc.ReleaseVarAndReuseHostReg(rhs_var, result_var);

    auto result_reg = reg_alloc.GetVariableHostReg(result_var);

    if (result_reg == lhs_reg) {
      code.adc(lhs_reg, rhs_reg);
    } else if (result_reg == rhs_reg) {
      code.adc(rhs_reg, lhs_reg);
    } else {
      code.mov(result_reg, lhs_reg);
      code.adc(result_reg, rhs_reg);
    }
  }

  if (op->update_host_flags) {
    code.lahf();
    code.seto(al);
  }
}

void X64Backend::CompileSBC(CompileContext const& context, IRSbc* op) {
  DESTRUCTURE_CONTEXT;

  auto& result_var = op->result.Unwrap();
  auto& lhs_var = op->lhs.Get();
  auto  lhs_reg = reg_alloc.GetVariableHostReg(lhs_var);

  code.sahf();
  code.cmc();

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);

    auto result_reg = reg_alloc.GetVariableHostReg(result_var);

    if (result_reg != lhs_reg) {
      code.mov(result_reg, lhs_reg);
    }
    code.sbb(result_reg, imm);
  } else {
    auto rhs_reg = reg_alloc.GetVariableHostReg(op->rhs.GetVar());

    reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);

    auto result_reg = reg_alloc.GetVariableHostReg(result_var);

    if (result_reg != lhs_reg) {
      code.mov(result_reg, lhs_reg);
    }
    code.sbb(result_reg, rhs_reg);
  }

  if (op->update_host_flags) {
    code.cmc();
    code.lahf();
    code.seto(al);
  }
}

void X64Backend::CompileRSC(CompileContext const& context, IRRsc* op) {
  DESTRUCTURE_CONTEXT;

  auto& result_var = op->result.Unwrap();
  auto lhs_reg = reg_alloc.GetVariableHostReg(op->lhs.Get());

  code.sahf();
  code.cmc();

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;
    auto result_reg = reg_alloc.GetVariableHostReg(result_var);

    code.mov(result_reg, imm);
    code.sbb(result_reg, lhs_reg);
  } else {
    auto& rhs_var = op->rhs.GetVar();
    auto  rhs_reg = reg_alloc.GetVariableHostReg(rhs_var);

    reg_alloc.ReleaseVarAndReuseHostReg(rhs_var, result_var);

    auto result_reg = reg_alloc.GetVariableHostReg(result_var);

    if (result_reg != rhs_reg) {
      code.mov(result_reg, rhs_reg);
    }
    code.sbb(result_reg, lhs_reg);
  }

  if (op->update_host_flags) {
    code.cmc();
    code.lahf();
    code.seto(al);
  }
}

void X64Backend::CompileORR(CompileContext const& context, IRBitwiseORR* op) {
  DESTRUCTURE_CONTEXT;

  auto& result_var = op->result.Unwrap();
  auto& lhs_var = op->lhs.Get();
  auto  lhs_reg = reg_alloc.GetVariableHostReg(lhs_var);

  if (op->rhs.IsConstant()) {
    auto imm = op->rhs.GetConst().value;

    reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);

    auto result_reg = reg_alloc.GetVariableHostReg(result_var);

    if (result_reg != lhs_reg) {
      code.mov(result_reg, lhs_reg);
    }
    code.or_(result_reg, imm);
  } else {
    auto& rhs_var = op->rhs.GetVar();
    auto  rhs_reg = reg_alloc.GetVariableHostReg(rhs_var);

    reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);
    reg_alloc.ReleaseVarAndReuseHostReg(rhs_var, result_var);

    auto result_reg = reg_alloc.GetVariableHostReg(result_var);

    if (result_reg == lhs_reg) {
      code.or_(lhs_reg, rhs_reg);
    } else if (result_reg == rhs_reg) {
      code.or_(rhs_reg, lhs_reg);
    } else {
      code.mov(result_reg, lhs_reg);
      code.or_(result_reg, rhs_reg);
    }
  }

  if (op->update_host_flags) {
    // load flags but preserve carry
    code.bt(ax, 8); // CF = value of bit8
    code.lahf();
  }
}

void X64Backend::CompileMOV(CompileContext const& context, IRMov* op) {
  DESTRUCTURE_CONTEXT;

  auto& result_var = op->result.Get();
  auto  result_reg = Xbyak::Reg32{};

  if (op->source.IsConstant()) {
    result_reg = reg_alloc.GetVariableHostReg(result_var);
    code.mov(result_reg, op->source.GetConst().value);
  } else {
    auto& source_var = op->source.GetVar();
    auto  source_reg = reg_alloc.GetVariableHostReg(source_var);

    reg_alloc.ReleaseVarAndReuseHostReg(source_var, result_var);
    result_reg = reg_alloc.GetVariableHostReg(result_var);

    if (result_reg != source_reg) {
      code.mov(result_reg, source_reg);
    }
  }

  if (op->update_host_flags) {
    code.test(result_reg, result_reg);
    // load flags but preserve carry
    code.bt(ax, 8); // CF = value of bit8
    code.lahf();
  }
}

void X64Backend::CompileMVN(CompileContext const& context, IRMvn* op) {
  DESTRUCTURE_CONTEXT;

  auto& result_var = op->result.Get();
  auto  result_reg = Xbyak::Reg32{};

  if (op->source.IsConstant()) {
    result_reg = reg_alloc.GetVariableHostReg(result_var);
    code.mov(result_reg, op->source.GetConst().value);
  } else {
    auto& source_var = op->source.GetVar();
    auto  source_reg = reg_alloc.GetVariableHostReg(source_var);

    reg_alloc.ReleaseVarAndReuseHostReg(source_var, result_var);
    result_reg = reg_alloc.GetVariableHostReg(result_var);

    if (result_reg != source_reg) {
      code.mov(result_reg, source_reg);
    }
  }

  code.not_(result_reg);

  if (op->update_host_flags) {
    code.test(result_reg, result_reg);
    // load flags but preserve carry
    code.bt(ax, 8); // CF = value of bit8
    code.lahf();
  }
}

void X64Backend::CompileCLZ(CompileContext const& context, IRCountLeadingZeros* op) {
  DESTRUCTURE_CONTEXT;

  auto& result_var = op->result.Get();
  auto& operand_var = op->operand.Get();
  auto  operand_reg = reg_alloc.GetVariableHostReg(operand_var);

  reg_alloc.ReleaseVarAndReuseHostReg(operand_var, result_var);

  code.lzcnt(reg_alloc.GetVariableHostReg(op->result.Get()), operand_reg);
}

void X64Backend::CompileQADD(CompileContext const& context, IRSaturatingAdd* op) {
  DESTRUCTURE_CONTEXT;

  auto& result_var = op->result.Get();
  auto& lhs_var = op->lhs.Get();
  auto& rhs_var = op->rhs.Get();

  auto lhs_reg = reg_alloc.GetVariableHostReg(lhs_var);
  auto rhs_reg = reg_alloc.GetVariableHostReg(rhs_var);
  auto temp_reg = reg_alloc.GetTemporaryHostReg();
  auto label_skip_saturate = Xbyak::Label{};

  reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);
  reg_alloc.ReleaseVarAndReuseHostReg(rhs_var, result_var);

  auto result_reg = reg_alloc.GetVariableHostReg(result_var);

  if (result_reg == lhs_reg) {
    code.add(lhs_reg, rhs_reg);
  } else if (result_reg == rhs_reg) {
    code.add(rhs_reg, lhs_reg);
  } else {
    code.mov(result_reg, lhs_reg);
    code.add(result_reg, rhs_reg);
  }

  code.jno(label_skip_saturate);
  code.mov(temp_reg, 0x7FFF'FFFF);
  code.mov(result_reg, 0x8000'0000);
  code.cmovs(result_reg, temp_reg);

  code.L(label_skip_saturate);
  code.seto(al);
}

void X64Backend::CompileQSUB(CompileContext const& context, IRSaturatingSub* op) {
  DESTRUCTURE_CONTEXT;

  auto& result_var = op->result.Get();
  auto& lhs_var = op->lhs.Get();
  auto& rhs_var = op->rhs.Get();

  auto lhs_reg = reg_alloc.GetVariableHostReg(lhs_var);
  auto rhs_reg = reg_alloc.GetVariableHostReg(rhs_var);
  auto temp_reg = reg_alloc.GetTemporaryHostReg();
  auto label_skip_saturate = Xbyak::Label{};

  reg_alloc.ReleaseVarAndReuseHostReg(lhs_var, result_var);

  auto result_reg = reg_alloc.GetVariableHostReg(result_var);

  if (result_reg != lhs_reg) {
    code.mov(result_reg, lhs_reg);
  }
  code.sub(result_reg, rhs_reg);

  code.jno(label_skip_saturate);
  code.mov(temp_reg, 0x7FFF'FFFF);
  code.mov(result_reg, 0x8000'0000);
  code.cmovs(result_reg, temp_reg);

  code.L(label_skip_saturate);
  code.seto(al);
}

} // namespace lunatic::backend
