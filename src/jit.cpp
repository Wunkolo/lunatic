/*
 * Copyright (C) 2021 fleroviux
 */

#include <lunatic/cpu.hpp>
#include <vector>

#include "frontend/ir_opt/constant_propagation.hpp"
#include "frontend/ir_opt/context_load_store_elision.hpp"
#include "frontend/ir_opt/dead_code_elision.hpp"
#include "frontend/ir_opt/dead_flag_elision.hpp"
#include "frontend/state.hpp"
#include "frontend/translator/translator.hpp"

#include "backend/backend.hpp"

using namespace lunatic::frontend;
using namespace lunatic::backend;

namespace lunatic {

struct JIT final : CPU {
  JIT(CPU::Descriptor const& descriptor)
      : exception_base(descriptor.exception_base)
      , memory(descriptor.memory)
      , translator(descriptor) {
    backend = Backend::CreateBackend(descriptor, state, block_cache, irq_line);
    passes.push_back(std::make_unique<IRContextLoadStoreElisionPass>());
    passes.push_back(std::make_unique<IRDeadFlagElisionPass>());
    passes.push_back(std::make_unique<IRConstantPropagationPass>());
    passes.push_back(std::make_unique<IRDeadCodeElisionPass>());
  }

  void Reset() override {
    irq_line = false;
    wait_for_irq = false;
    cycles_to_run = 0;
    state.Reset();
    SetGPR(GPR::PC, exception_base);
    block_cache.Flush();
    exception_causing_basic_blocks.clear();
  }

  auto IRQLine() -> bool& override {
    return irq_line;
  }

  auto WaitForIRQ() -> bool& override {
    return wait_for_irq;
  }

  void SetExceptionBase(u32 new_exception_base) override {
    if (new_exception_base != this->exception_base) {
      // this is expected to happen rarely, so we just invalidate all blocks that may cause an exception.
      while (!exception_causing_basic_blocks.empty()) {
        block_cache.Set(exception_causing_basic_blocks.front()->key, nullptr);
      }

      translator.SetExceptionBase(new_exception_base);
      this->exception_base = new_exception_base;
    }
  }

  void ClearICache() override {
    block_cache.Flush();
  }

  void ClearICacheRange(u32 address_lo, u32 address_hi) override {
    block_cache.Flush(address_lo, address_hi);
  }

  auto Run(int cycles) -> int override {
    if (WaitForIRQ() && !IRQLine()) {
      return 0;
    }

    cycles_to_run += cycles;

    int cycles_available = cycles_to_run;

    while (cycles_to_run > 0) {
      if (IRQLine()) {
        SignalIRQ();
      }

      auto block_key = BasicBlock::Key{state};
      auto basic_block = block_cache.Get(block_key);
      auto hash = GetBasicBlockHash(block_key);

      if (basic_block == nullptr || basic_block->hash != hash) {
        basic_block = Compile(block_key);
      }

      cycles_to_run = backend->Call(*basic_block, cycles_to_run);

      if (WaitForIRQ()) {
        int cycles_executed = cycles_available - cycles_to_run;
        cycles_to_run = 0;
        return cycles_executed;
      }
    }

    return cycles_available - cycles_to_run;
  }

  auto GetGPR(GPR reg) const -> u32 override {
    return GetGPR(reg, GetCPSR().f.mode);
  }

  auto GetGPR(GPR reg, Mode mode) const -> u32 override {
    return const_cast<JIT*>(this)->GetGPR(reg, mode);
  }

  auto GetCPSR() const -> StatusRegister override {
    return const_cast<JIT*>(this)->GetCPSR();
  }

  auto GetSPSR(Mode mode) const -> StatusRegister override {
    return const_cast<JIT*>(this)->GetSPSR(mode);
  }

  void SetGPR(GPR reg, u32 value) override {
    SetGPR(reg, state.GetCPSR().f.mode, value);
  }

  void SetGPR(GPR reg, Mode mode, u32 value) override {
    state.GetGPR(mode, reg) = value;

    if (reg == GPR::PC) {
      if (GetCPSR().f.thumb) {
        state.GetGPR(mode, GPR::PC) += sizeof(u16) * 2;
      } else {
        state.GetGPR(mode, GPR::PC) += sizeof(u32) * 2;
      }
    }
  }

  void SetCPSR(StatusRegister value) override {
    state.GetCPSR() = value;
  }

  void SetSPSR(Mode mode, StatusRegister value) override {
    *state.GetPointerToSPSR(mode) = value;
  }

private:
  auto Compile(BasicBlock::Key block_key) -> BasicBlock* {
    auto basic_block = new BasicBlock{block_key};

    basic_block->hash = GetBasicBlockHash(block_key);

    translator.Translate(*basic_block);
    Optimize(basic_block);

    if (basic_block->uses_exception_base) {
      exception_causing_basic_blocks.push_back(basic_block);

      basic_block->RegisterReleaseCallback([this](BasicBlock const& block) {
        auto match = std::find(
          exception_causing_basic_blocks.begin(), exception_causing_basic_blocks.end(), &block);

        if (match != exception_causing_basic_blocks.end()) {
          exception_causing_basic_blocks.erase(match);
        }
      });
    }

    backend->Compile(*basic_block);
    block_cache.Set(block_key, basic_block);
    basic_block->micro_blocks.clear();
    return basic_block;
  }

  void Optimize(BasicBlock* basic_block) {
    for (auto &micro_block : basic_block->micro_blocks) {
      for (auto& pass : passes) {
        pass->Run(micro_block.emitter);
      }
    }
  }

  void SignalIRQ() {
    auto& cpsr = GetCPSR();

    wait_for_irq = false;

    if (!cpsr.f.mask_irq) {
      GetSPSR(Mode::IRQ) = cpsr;

      cpsr.f.mode = Mode::IRQ;
      cpsr.f.mask_irq = 1;
      if (cpsr.f.thumb) {
        GetGPR(GPR::LR) = GetGPR(GPR::PC);
      } else {
        GetGPR(GPR::LR) = GetGPR(GPR::PC) - 4;
      }
      cpsr.f.thumb = 0;

      GetGPR(GPR::PC) = exception_base + 0x18 + sizeof(u32) * 2;
    }
  }

  auto GetBasicBlockHash(BasicBlock::Key block_key) -> u32 {
    return memory.FastRead<u32, Memory::Bus::Code>(block_key.Address());
  }

  auto GetGPR(GPR reg) -> u32& {
    return GetGPR(reg, GetCPSR().f.mode);
  }

  auto GetGPR(GPR reg, Mode mode) -> u32& {
    return state.GetGPR(mode, reg);
  }

  auto GetCPSR() -> StatusRegister& {
    return state.GetCPSR();
  }

  auto GetSPSR(Mode mode) -> StatusRegister& {
    return *state.GetPointerToSPSR(mode);
  }

  bool irq_line = false;
  bool wait_for_irq = false;
  int cycles_to_run = 0;
  u32 exception_base;
  Memory& memory;
  State state;
  Translator translator;
  BasicBlockCache block_cache;
  std::unique_ptr<Backend> backend;
  std::vector<std::unique_ptr<IRPass>> passes;
  std::vector<BasicBlock*> exception_causing_basic_blocks;
};

auto CreateCPU(CPU::Descriptor const& descriptor) -> std::unique_ptr<CPU> {
  return std::make_unique<JIT>(descriptor);
}

} // namespace lunatic
