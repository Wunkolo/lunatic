/*
 * Copyright (C) 2024 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fmt/format.h>
#include <lunatic/cpu.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace lunatic;

struct TestMemory final : public lunatic::Memory {
  auto ReadByte(u32 address, Bus bus) -> u8 override { return b8[address]; }
  auto ReadHalf(u32 address, Bus bus) -> u16 override {
    assert(address % 2 == 0);
    return b16.at(address);
  }
  auto ReadWord(u32 address, Bus bus) -> u32 override {
    assert(address % 4 == 0);
    return b32.at(address);
  }

  void WriteByte(u32 address, u8 value, Bus bus) override {
    b8.at(address) = value;
  }
  void WriteHalf(u32 address, u16 value, Bus bus) override {
    assert(address % 2 == 0);
    b16.at(address) = value;
  }
  void WriteWord(u32 address, u32 value, Bus bus) override {
    assert(address % 4 == 0);
    b32.at(address) = value;
  }

  union {
    std::array<u8, 128> b8;
    std::array<u16, 64> b16;
    std::array<u32, 32> b32;
  };
};

TEST_CASE("Add", "[ALU]") {
  TestMemory test_memory;
  auto jit = CreateCPU(CPU::Descriptor{test_memory});

  jit->SetGPR(GPR::PC, 0);
  jit->Run(8);
}