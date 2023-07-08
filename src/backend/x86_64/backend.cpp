/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <algorithm>
#include <cstdlib>
#include <list>
#include <stdexcept>

#include "backend.hpp"
#include "common.hpp"
#include "common/aligned_memory.hpp"
#include "common/bit.hpp"
#include "vtune.hpp"

/**
 * TODO:
 * - think of a way to merge memory operands into X64 opcodes across IR opcodes.
 * - ...
 */

namespace lunatic {
namespace backend {

X64Backend::X64Backend(
  CPU::Descriptor const& descriptor,
  State& state,
  BasicBlockCache& block_cache,
  bool const& irq_line
)   : memory(descriptor.memory)
    , state(state)
    , coprocessors(descriptor.coprocessors)
    , block_cache(block_cache)
    , irq_line(irq_line) {
  CreateCodeGenerator();
  EmitCallBlock();
}

X64Backend::~X64Backend() {
  delete code;
  delete code_memory_block;
}

void X64Backend::CreateCodeGenerator() {
  code_memory_block = new memory::CodeBlockMemory(kCodeBufferSize);
  is_writeable = true;
  code = new Xbyak::CodeGenerator{kCodeBufferSize, code_memory_block->GetPointer()};
}

void X64Backend::EmitCallBlock() {
  auto stack_displacement = sizeof(u64) + X64RegisterAllocator::kSpillAreaSize * sizeof(u32);

  CallBlock = (int (*)(BasicBlock::CompiledFn, int))code->getCurr();

  Push(*code, {rbx, rbp, r12, r13, r14, r15});
#ifdef ABI_MSVC
  Push(*code, {rsi, rdi});
#endif
  code->sub(rsp, stack_displacement);
  code->mov(rbp, rsp);

  code->mov(r12, kRegArg0); // r12 = function pointer
  code->mov(rbx, kRegArg1); // rbx = cycle counter

  // Load carry flag into AH
  code->mov(rcx, uintptr(&state));
  code->mov(edx, dword[rcx + state.GetOffsetToCPSR()]);
  code->bt(edx, 29); // CF = value of bit 29
  code->lahf();
  
  code->call(r12);

  // Return remaining number of cycles
  code->mov(rax, rbx);

  code->add(rsp, stack_displacement);
#ifdef ABI_MSVC
  Pop(*code, {rsi, rdi});
#endif
  Pop(*code, {rbx, rbp, r12, r13, r14, r15});
  code->ret();

#if LUNATIC_USE_VTUNE
  vtune::ReportCallBlock(reinterpret_cast<u8*>(CallBlock), code->getCurr());
#endif
}

void X64Backend::Compile(BasicBlock& basic_block) {
  if (!is_writeable) {
    code_memory_block->ProtectForWrite();
    is_writeable = true;
  }

  try {
    auto label_return_to_dispatch = Xbyak::Label{};
    auto opcode_size = basic_block.key.Thumb() ? sizeof(u16) : sizeof(u32);
    auto number_of_micro_blocks = basic_block.micro_blocks.size();

    basic_block.function = (BasicBlock::CompiledFn)code->getCurr();

    for (size_t i = 0; i < number_of_micro_blocks; i++) {
      auto const& micro_block = basic_block.micro_blocks[i];
      auto& emitter  = micro_block.emitter;
      auto condition = micro_block.condition;
      auto reg_alloc = X64RegisterAllocator{emitter, *code};
      auto context   = CompileContext{*code, reg_alloc, state};

      auto label_skip = Xbyak::Label{};
      auto label_done = Xbyak::Label{};

      // Skip past the micro block if its condition is not met
      EmitConditionalBranch(condition, label_skip);

      // Compile each IR opcode inside the micro block
      for (auto const& op : emitter.Code()) {
        CompileIROp(context, op);
        reg_alloc.AdvanceLocation();
      }

      /* Once we reached the end of the basic block,
       * check if we can emit a jump to an already compiled basic block.
       * Also update the cycle counter in that case and return to the dispatcher
       * in the case that we ran out of cycles.
       */
      if (basic_block.enable_fast_dispatch && i == number_of_micro_blocks - 1) {
        auto& branch_target = basic_block.branch_target;

        if (branch_target.key.value != 0) {
          BasicBlock* target_block;

          if (branch_target.key == basic_block.key) {
            target_block = &basic_block;
          } else {
            target_block = block_cache.Get(branch_target.key);
          }

          // Return to the dispatcher if we ran out of cycles.
          code->sub(rbx, basic_block.length);
          code->jle(label_return_to_dispatch, Xbyak::CodeGenerator::T_NEAR);

          // Return to the dispatcher if there is an IRQ to handle
          code->mov(rdx, uintptr(&irq_line));
          code->cmp(byte[rdx], 0);
          code->jnz(label_return_to_dispatch);

          if (target_block) {
            // The branch target is already compiled, emit a relative jump to it now.
            code->jmp((const void*)target_block->function);

            target_block->linking_blocks.push_back(&basic_block);
          } else {
            /* The branch target has not been compiled yet.
             * Create a padding of 5 NOPs and memorize its address, so that a relative jump
             * can be patched in once the branch target has been compiled.
             */
            branch_target.patch_location = code->getCurr<u8*>();
            code->nop(5);

            /* Memorize that this basic block should link to the branch target,
             * so that we know which blocks to patch once the branch target has been compiled.
             */
            block_linking_table[branch_target.key].push_back(&basic_block);
          }
        }
      }

      /* The program counter is normally updated via IR opcodes.
       * But if we skipped past the code which'd do that, we need to manually
       * update the program counter.
       */
      if (condition != Condition::AL) {
        code->jmp(label_done);

        code->L(label_skip);
        code->add(
          dword[rcx + state.GetOffsetToGPR(Mode::User, GPR::PC)],
          micro_block.length * opcode_size
        );

        code->L(label_done);
      }
    }

    if (basic_block.enable_fast_dispatch) {
      // Return to the dispatcher if we ran out of cycles.
      code->sub(rbx, basic_block.length);
      code->jle(label_return_to_dispatch);

      // Return to the dispatcher if there is an IRQ to handle
      code->mov(rdx, uintptr(&irq_line));
      code->cmp(byte[rdx], 0);
      code->jnz(label_return_to_dispatch);

      // If the next basic block already is compiled then jump to it.
      EmitBasicBlockDispatch(label_return_to_dispatch);

      code->L(label_return_to_dispatch);
      code->ret();
    } else {
      code->sub(rbx, basic_block.length);
      code->ret();
    }

    Link(basic_block);

    basic_block.RegisterReleaseCallback([this](BasicBlock const& basic_block) {
      OnBasicBlockToBeDeleted(basic_block);
    });

#if LUNATIC_USE_VTUNE
    vtune::ReportBasicBlock(basic_block, code->getCurr());
#endif
  } catch (Xbyak::Error error) {
    if (int(error) == Xbyak::ERR_CODE_IS_TOO_BIG) {
      fmt::print("FLUSH\n");
      block_cache.Flush();
      code->resetSize();
      EmitCallBlock();
      Compile(basic_block);
    } else {
      throw;
    }
  }
}

int X64Backend::Call(BasicBlock const& basic_block, int max_cycles) {
  if (is_writeable) {
    code_memory_block->ProtectForExecute();
    code_memory_block->Invalidate();
    is_writeable = false;
  }

  return CallBlock(basic_block.function, max_cycles);
}

void X64Backend::EmitConditionalBranch(Condition condition, Xbyak::Label& label_skip) {
  if (condition == Condition::AL) {
    return;
  }

  // TODO: Keep decompressed flags in eax?
  code->mov(eax, dword[rcx + state.GetOffsetToCPSR()]);
  code->shr(eax, 28);

#ifdef LUNATIC_SUPPORT_BMI
  code->mov(edx, 0xC101);
  code->pdep(eax, eax, edx);
#else
  code->imul(eax, eax, 0x1081);
  code->and_(eax, 0xC101);
#endif

  switch (condition) {
    case Condition::EQ:
      code->sahf();
      code->jnz(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::NE:
      code->sahf();
      code->jz(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::CS:
      code->sahf();
      code->jnc(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::CC:
      code->sahf();
      code->jc(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::MI:
      code->sahf();
      code->jns(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::PL:
      code->sahf();
      code->js(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::VS:
      code->cmp(al, 0x81);
      code->jno(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::VC:
      code->cmp(al, 0x81);
      code->jo(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::HI:
      code->sahf();
      code->cmc();
      code->jna(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::LS:
      code->sahf();
      code->cmc();
      code->ja(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::GE:
      code->cmp(al, 0x81);
      code->sahf();
      code->jnge(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::LT:
      code->cmp(al, 0x81);
      code->sahf();
      code->jnl(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::GT:
      code->cmp(al, 0x81);
      code->sahf();
      code->jng(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::LE:
      code->cmp(al, 0x81);
      code->sahf();
      code->jnle(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    case Condition::NV:
      code->jmp(label_skip, Xbyak::CodeGenerator::T_NEAR);
      break;
    default:
      fmt::print("Unsupported Condition {}\n", condition);
      std::abort();
  }
}

void X64Backend::EmitBasicBlockDispatch(Xbyak::Label& label_cache_miss) {
  // Build the block key from R15 and CPSR.
  // See frontend/basic_block.hpp
  code->mov(edx, dword[rcx + state.GetOffsetToGPR(Mode::User, GPR::PC)]);
  code->mov(esi, dword[rcx + state.GetOffsetToCPSR()]);
  code->shr(edx, 1);
  code->and_(esi, 0x3F);
  code->shl(rsi, 31);
  code->or_(rdx, rsi);

  // Hash0 lookup (first level)
  code->mov(rsi, rdx);
  code->shr(rsi, 19);
  code->mov(rdi, uintptr(block_cache.data));
  code->mov(rdi, qword[rdi + rsi * sizeof(uintptr)]);
  code->test(rdi, rdi);
  code->jz(label_cache_miss);

  // Hash1 lookup (second level)
  code->and_(edx, 0x7FFFF);
  code->mov(rdi, qword[rdi + rdx * sizeof(uintptr)]);
  code->test(rdi, rdi);
  code->jz(label_cache_miss);
  code->mov(rdi, qword[rdi + offsetof(BasicBlock, function)]);

  // Load carry flag into AH
  code->mov(edx, dword[rcx + state.GetOffsetToCPSR()]);
  code->bt(edx, 29); // CF = value of bit 29
  code->lahf();

  code->jmp(rdi);
}

void X64Backend::Link(BasicBlock& basic_block) {
  auto iterator = block_linking_table.find(basic_block.key);

  if (iterator == block_linking_table.end()) {
    return;
  }

  for (auto linking_block : iterator->second) {
    u8* patch = linking_block->branch_target.patch_location;
    u32 relative_address = (u32)((s64)basic_block.function - (s64)patch - 5LL);

    patch[0] = 0xE9;
    patch[1] = (u8)(relative_address >>  0);
    patch[2] = (u8)(relative_address >>  8);
    patch[3] = (u8)(relative_address >> 16);
    patch[4] = (u8)(relative_address >> 24);

    basic_block.linking_blocks.push_back(linking_block);
  }

  block_linking_table.erase(iterator);
}

void X64Backend::OnBasicBlockToBeDeleted(BasicBlock const& basic_block) {
  // TODO: release the allocated JIT buffer memory.

  auto const& branch_target = basic_block.branch_target;

  // Do not leave a dangling pointer to the block in the block linking table or the target block.
  if (!branch_target.key.IsEmpty()) {
    auto iterator = block_linking_table.find(branch_target.key);

    if (iterator != block_linking_table.end()) {
      auto& linking_blocks = iterator->second;

      linking_blocks.erase(std::find(
        linking_blocks.begin(), linking_blocks.end(), &basic_block));
    }

    auto target_block = block_cache.Get(branch_target.key);

    if (target_block) {
      auto& linking_blocks = target_block->linking_blocks;

      auto iterator = std::find(
        linking_blocks.begin(), linking_blocks.end(), &basic_block);

      if (iterator != linking_blocks.end()) {
        linking_blocks.erase(iterator);
      }
    }
  }
}

void X64Backend::CompileIROp(
  CompileContext const& context,
  std::unique_ptr<IROpcode> const& op
) {
  switch (op->GetClass()) {
    case IROpcodeClass::NOP: break;

    // Context access (compile_context.cpp)
    case IROpcodeClass::LoadGPR: CompileLoadGPR(context, lunatic_cast<IRLoadGPR>(op.get())); break;
    case IROpcodeClass::StoreGPR: CompileStoreGPR(context, lunatic_cast<IRStoreGPR>(op.get())); break;
    case IROpcodeClass::LoadSPSR: CompileLoadSPSR(context, lunatic_cast<IRLoadSPSR>(op.get())); break;
    case IROpcodeClass::StoreSPSR: CompileStoreSPSR(context, lunatic_cast<IRStoreSPSR>(op.get())); break;
    case IROpcodeClass::LoadCPSR: CompileLoadCPSR(context, lunatic_cast<IRLoadCPSR>(op.get())); break;
    case IROpcodeClass::StoreCPSR: CompileStoreCPSR(context, lunatic_cast<IRStoreCPSR>(op.get())); break;
    case IROpcodeClass::ClearCarry: CompileClearCarry(context, lunatic_cast<IRClearCarry>(op.get())); break;
    case IROpcodeClass::SetCarry:   CompileSetCarry(context, lunatic_cast<IRSetCarry>(op.get())); break;
    case IROpcodeClass::UpdateFlags: CompileUpdateFlags(context, lunatic_cast<IRUpdateFlags>(op.get())); break;
    case IROpcodeClass::UpdateSticky: CompileUpdateSticky(context, lunatic_cast<IRUpdateSticky>(op.get())); break;
    
    // Barrel shifter (compile_shift.cpp)
    case IROpcodeClass::LSL: CompileLSL(context, lunatic_cast<IRLogicalShiftLeft>(op.get())); break;
    case IROpcodeClass::LSR: CompileLSR(context, lunatic_cast<IRLogicalShiftRight>(op.get())); break;
    case IROpcodeClass::ASR: CompileASR(context, lunatic_cast<IRArithmeticShiftRight>(op.get())); break;
    case IROpcodeClass::ROR: CompileROR(context, lunatic_cast<IRRotateRight>(op.get())); break;
    
    // ALU (compile_alu.cpp)
    case IROpcodeClass::AND: CompileAND(context, lunatic_cast<IRBitwiseAND>(op.get())); break;
    case IROpcodeClass::BIC: CompileBIC(context, lunatic_cast<IRBitwiseBIC>(op.get())); break;
    case IROpcodeClass::EOR: CompileEOR(context, lunatic_cast<IRBitwiseEOR>(op.get())); break;
    case IROpcodeClass::SUB: CompileSUB(context, lunatic_cast<IRSub>(op.get())); break;
    case IROpcodeClass::RSB: CompileRSB(context, lunatic_cast<IRRsb>(op.get())); break;
    case IROpcodeClass::ADD: CompileADD(context, lunatic_cast<IRAdd>(op.get())); break;
    case IROpcodeClass::ADC: CompileADC(context, lunatic_cast<IRAdc>(op.get())); break;
    case IROpcodeClass::SBC: CompileSBC(context, lunatic_cast<IRSbc>(op.get())); break;
    case IROpcodeClass::RSC: CompileRSC(context, lunatic_cast<IRRsc>(op.get())); break;
    case IROpcodeClass::ORR: CompileORR(context, lunatic_cast<IRBitwiseORR>(op.get())); break;
    case IROpcodeClass::MOV: CompileMOV(context, lunatic_cast<IRMov>(op.get())); break;
    case IROpcodeClass::MVN: CompileMVN(context, lunatic_cast<IRMvn>(op.get())); break;
    case IROpcodeClass::CLZ: CompileCLZ(context, lunatic_cast<IRCountLeadingZeros>(op.get())); break;
    case IROpcodeClass::QADD: CompileQADD(context, lunatic_cast<IRSaturatingAdd>(op.get())); break;
    case IROpcodeClass::QSUB: CompileQSUB(context, lunatic_cast<IRSaturatingSub>(op.get())); break;

    // Multiply (and accumulate) (compile_multiply.cpp)
    case IROpcodeClass::MUL: CompileMUL(context, lunatic_cast<IRMultiply>(op.get())); break;
    case IROpcodeClass::ADD64: CompileADD64(context, lunatic_cast<IRAdd64>(op.get())); break;
   
    // Memory read/write (compile_memory.cpp)
    case IROpcodeClass::MemoryRead: CompileMemoryRead(context, lunatic_cast<IRMemoryRead>(op.get())); break;
    case IROpcodeClass::MemoryWrite: CompileMemoryWrite(context, lunatic_cast<IRMemoryWrite>(op.get())); break;
    
    // Pipeline flush (compile_flush.cpp)
    case IROpcodeClass::Flush: CompileFlush(context, lunatic_cast<IRFlush>(op.get())); break;
    case IROpcodeClass::FlushExchange: CompileFlushExchange(context, lunatic_cast<IRFlushExchange>(op.get())); break;

    // Coprocessor access (compile_coprocessor.cpp)
    case IROpcodeClass::MRC: CompileMRC(context, lunatic_cast<IRReadCoprocessorRegister>(op.get())); break;
    case IROpcodeClass::MCR: CompileMCR(context, lunatic_cast<IRWriteCoprocessorRegister>(op.get())); break;

    default: {
      throw std::runtime_error(
        fmt::format("lunatic: unhandled IR opcode: {}", op->ToString())
      );
    }
  }
}

void X64Backend::Push(
  Xbyak::CodeGenerator& code,
  std::vector<Xbyak::Reg64> const& regs
) {
  for (auto reg : regs) {
    code.push(reg);
  }
}

void X64Backend::Pop(
  Xbyak::CodeGenerator& code,
  std::vector<Xbyak::Reg64> const& regs
) {
  auto end = regs.rend();

  for (auto it = regs.rbegin(); it != end; ++it) {
    code.pop(*it);
  }
}

auto X64Backend::GetUsedHostRegsFromList(
  X64RegisterAllocator const& reg_alloc,
  std::vector<Xbyak::Reg64> const& regs
) -> std::vector<Xbyak::Reg64> {
  auto regs_used = std::vector<Xbyak::Reg64>{};

  for (auto reg : regs) {
    if (!reg_alloc.IsHostRegFree(reg)) {
      regs_used.push_back(reg);
    }
  }

  return regs_used;
}

std::unique_ptr<Backend> Backend::CreateBackend(CPU::Descriptor const& descriptor,
                                                State& state,
                                                BasicBlockCache& block_cache,
                                                bool const& irq_line) {
  return std::make_unique<X64Backend>(descriptor, state, block_cache, irq_line);
}

} // namespace lunatic::backend
} // namespace lunatic
