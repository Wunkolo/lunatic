/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "frontend/basic_block_cache.hpp"

namespace lunatic {
namespace backend {

struct Backend {
  virtual ~Backend() = default;

  virtual void Compile(frontend::BasicBlock& basic_block) = 0;
  virtual int Call(frontend::BasicBlock const& basic_block, int max_cycles) = 0;

  static std::unique_ptr<Backend> CreateBackend(CPU::Descriptor const& descriptor,
                                                frontend::State& state,
                                                frontend::BasicBlockCache& block_cache,
                                                bool const& irq_line);
};

} // namespace lunatic::backend
} // namespace lunatic
