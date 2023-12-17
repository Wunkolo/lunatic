/*
 * Copyright (C) 2022 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "backend.hpp"

#define DESTRUCTURE_CONTEXT auto& [code, reg_alloc, state] = context;

using namespace Xbyak::util;

#ifdef _WIN64
  #define ABI_MSVC
#else
  #define ABI_SYSV
#endif

#ifdef ABI_MSVC
  static constexpr Xbyak::Reg64 kRegArg0 = rcx;
  static constexpr Xbyak::Reg64 kRegArg1 = rdx;
  static constexpr Xbyak::Reg64 kRegArg2 = r8;
  static constexpr Xbyak::Reg64 kRegArg3 = r9;
#else
  static constexpr Xbyak::Reg64 kRegArg0 = rdi;
  static constexpr Xbyak::Reg64 kRegArg1 = rsi;
  static constexpr Xbyak::Reg64 kRegArg2 = rdx;
  static constexpr Xbyak::Reg64 kRegArg3 = rcx;
  static constexpr Xbyak::Reg64 kRegArg4 = r8;
  static constexpr Xbyak::Reg64 kRegArg5 = r9;
#endif
