/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lunatic/cpu.hpp>
#include <fmt/format.h>
#include <unordered_map>
#include <vector>

#include "backend/backend.hpp"
#include "common/aligned_memory.hpp"
#include "frontend/basic_block_cache.hpp"
#include "frontend/state.hpp"
#include "register_allocator.hpp"

using namespace lunatic::frontend;

// TODO: Add a runtime check for that.
#ifndef __APPLE__
  #define LUNATIC_SUPPORT_BMI 1
#endif

namespace lunatic {
namespace backend {

struct X64Backend : Backend {
  X64Backend(
    CPU::Descriptor const& descriptor,
    State& state,
    BasicBlockCache& block_cache,
    bool const& irq_line
  );

 ~X64Backend();

  void Compile(BasicBlock& basic_block) override;
  int Call(frontend::BasicBlock const& basic_block, int max_cycles) override;

private:
  static constexpr size_t kCodeBufferSize = 32 * 1024 * 1024;

  struct CompileContext {
    Xbyak::CodeGenerator& code;
    X64RegisterAllocator& reg_alloc;
    State& state;
  };

  void CreateCodeGenerator();
  void EmitCallBlock();

  void EmitConditionalBranch(Condition condition, Xbyak::Label& label_skip);

  void EmitReturnToDispatchIfNeeded(BasicBlock& basic_block, Xbyak::Label& label_return_to_dispatch);
  void EmitBasicBlockDispatch(Xbyak::Label& label_cache_miss);
  void EmitBlockLinkingEpilogue(BasicBlock& basic_block);

  void Link(BasicBlock& basic_block);

  void OnBasicBlockToBeDeleted(BasicBlock const& basic_block);

  void CompileIROp(
    CompileContext const& context,
    std::unique_ptr<IROpcode> const& op
  );

  void Push(
    Xbyak::CodeGenerator& code,
    std::vector<Xbyak::Reg64> const& regs
  );

  void Pop(
    Xbyak::CodeGenerator& code,
    std::vector<Xbyak::Reg64> const& regs
  );

  auto GetUsedHostRegsFromList(
    X64RegisterAllocator const& reg_alloc,
    std::vector<Xbyak::Reg64> const& regs
  ) -> std::vector<Xbyak::Reg64>;

  void CompileLoadGPR(CompileContext const& context, IRLoadGPR* op);
  void CompileStoreGPR(CompileContext const& context, IRStoreGPR* op);
  void CompileLoadSPSR(CompileContext const& context, IRLoadSPSR* op);
  void CompileStoreSPSR(CompileContext const& context, IRStoreSPSR* op);
  void CompileLoadCPSR(CompileContext const& context, IRLoadCPSR* op);
  void CompileStoreCPSR(CompileContext const& context, IRStoreCPSR* op);
  void CompileClearCarry(CompileContext const& context, IRClearCarry* op);
  void CompileSetCarry(CompileContext const& context, IRSetCarry* op);
  void CompileUpdateFlags(CompileContext const& context, IRUpdateFlags* op);
  void CompileUpdateSticky(CompileContext const& context, IRUpdateSticky* op);
  void CompileLSL(CompileContext const& context, IRLogicalShiftLeft* op);
  void CompileLSR(CompileContext const& context, IRLogicalShiftRight* op);
  void CompileASR(CompileContext const& context, IRArithmeticShiftRight* op);
  void CompileROR(CompileContext const& context, IRRotateRight* op);
  void CompileAND(CompileContext const& context, IRBitwiseAND* op);
  void CompileBIC(CompileContext const& context, IRBitwiseBIC* op);
  void CompileEOR(CompileContext const& context, IRBitwiseEOR* op);
  void CompileSUB(CompileContext const& context, IRSub* op);
  void CompileRSB(CompileContext const& context, IRRsb* op);
  void CompileADD(CompileContext const& context, IRAdd* op);
  void CompileADC(CompileContext const& context, IRAdc* op);
  void CompileSBC(CompileContext const& context, IRSbc* op);
  void CompileRSC(CompileContext const& context, IRRsc* op);
  void CompileORR(CompileContext const& context, IRBitwiseORR* op);
  void CompileMOV(CompileContext const& context, IRMov* op);
  void CompileMVN(CompileContext const& context, IRMvn* op);
  void CompileCLZ(CompileContext const& context, IRCountLeadingZeros* op);
  void CompileQADD(CompileContext const& context, IRSaturatingAdd* op);
  void CompileQSUB(CompileContext const& context, IRSaturatingSub* op);
  void CompileMUL(CompileContext const& context, IRMultiply* op);
  void CompileADD64(CompileContext const& context, IRAdd64* op);
  void CompileMemoryRead(CompileContext const& context, IRMemoryRead* op);
  void CompileMemoryWrite(CompileContext const& context, IRMemoryWrite* op);
  void CompileFlush(CompileContext const& context, IRFlush* op);
  void CompileFlushExchange(CompileContext const& context, IRFlushExchange* op);
  void CompileMRC(CompileContext const& context, IRReadCoprocessorRegister* op);
  void CompileMCR(CompileContext const& context, IRWriteCoprocessorRegister* op);

  Memory& memory;
  State& state;
  std::array<Coprocessor*, 16> coprocessors;
  BasicBlockCache& block_cache;
  bool const& irq_line;
  int (*CallBlock)(BasicBlock::CompiledFn, int);

  memory::CodeBlockMemory *code_memory_block;
  bool is_writeable;
  Xbyak::CodeGenerator* code;

  std::unordered_map<BasicBlock::Key, std::vector<BasicBlock*>> block_linking_table;
};

} // namespace lunatic::backend
} // namespace lunatic
