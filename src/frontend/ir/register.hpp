/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <fmt/format.h>

#include "frontend/state.hpp"

namespace lunatic {
namespace frontend {

/// References an ARM guest register with respect to the processor mode.
struct IRGuestReg {
  IRGuestReg(State::GPR reg, State::Mode mode) : reg(reg), mode(mode) {}

  /// The ARM general purpose register
  const State::GPR reg;

  /// The ARM processor mode
  const State::Mode mode;
};

} // namespace lunatic::frontend
} // namespace lunatic

namespace std {

inline auto to_string(lunatic::frontend::IRGuestReg const& guest_reg) -> std::string {
  using Mode = lunatic::frontend::State::Mode;

  auto id = static_cast<uint>(guest_reg.reg);
  auto mode = guest_reg.mode;

  if (id <= 7 || (id <= 12 && mode != Mode::FIQ) || id == 15) {
    return fmt::format("r{0}", id);
  }

  return fmt::format("r{0}_{1}", id, std::to_string(mode));
}

} // namespace std
