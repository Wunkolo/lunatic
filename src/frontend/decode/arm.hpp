/*
 * Copyright (C) 2021 fleroviux. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lunatic/integer.hpp>

#include "common/bit.hpp"
#include "definition/block_data_transfer.hpp"
#include "definition/branch_exchange.hpp"
#include "definition/branch_relative.hpp"
#include "definition/coprocessor_register_transfer.hpp"
#include "definition/count_leading_zeros.hpp"
#include "definition/data_processing.hpp"
#include "definition/exception.hpp"
#include "definition/halfword_signed_transfer.hpp"
#include "definition/multiply.hpp"
#include "definition/multiply_long.hpp"
#include "definition/saturating_add_sub.hpp"
#include "definition/signed_halfword_multiply.hpp"
#include "definition/single_data_swap.hpp"
#include "definition/single_data_transfer.hpp"
#include "definition/status_transfer.hpp"
#include "definition/thumb_bl_suffix.hpp"

namespace lunatic {
namespace frontend {

/// Receives decoded opcode data
template<typename T>
struct ARMDecodeClient {
  /// Return type for Handle() methods.
  /// Utilized by decode_arm to inter its return type.
  using return_type = T;

  virtual auto Handle(ARMDataProcessing const& opcode) -> T = 0;
  virtual auto Handle(ARMMoveStatusRegister const& opcode) -> T = 0;
  virtual auto Handle(ARMMoveRegisterStatus const& opcode) -> T = 0;
  virtual auto Handle(ARMMultiply const& opcode) -> T = 0;
  virtual auto Handle(ARMMultiplyLong const& opcode) -> T = 0;
  virtual auto Handle(ARMSingleDataSwap const& opcode) -> T = 0;
  virtual auto Handle(ARMBranchExchange const& opcode) -> T = 0;
  virtual auto Handle(ARMHalfwordSignedTransfer const& opcode) -> T = 0;
  virtual auto Handle(ARMSingleDataTransfer const& opcode) -> T = 0;
  virtual auto Handle(ARMBlockDataTransfer const& opcode) -> T = 0;
  virtual auto Handle(ARMBranchRelative const& opcode) -> T = 0;
  virtual auto Handle(ARMCoprocessorRegisterTransfer const& opcode) -> T = 0;
  virtual auto Handle(ARMException const& opcode) -> T = 0;
  virtual auto Handle(ARMCountLeadingZeros const& opcode) -> T = 0;
  virtual auto Handle(ARMSaturatingAddSub const& opcode) -> T = 0;
  virtual auto Handle(ARMSignedHalfwordMultiply const& opcode) -> T = 0;
  virtual auto Handle(ARMSignedWordHalfwordMultiply const& opcode) -> T = 0;
  virtual auto Handle(ARMSignedHalfwordMultiplyAccumulateLong const& opcode) -> T = 0;
  virtual auto Handle(ThumbBranchLinkSuffix const& opcode) -> T = 0;
  virtual auto Undefined(u32 opcode) -> T = 0;
};

namespace detail {

template<typename T, typename U = typename T::return_type>
inline auto decode_data_processing(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMDataProcessing{};
  
  info.condition = condition;
  info.opcode = bit::get_field<u32, ARMDataProcessing::Opcode>(opcode, 21, 4);
  info.immediate = bit::get_bit<u32, bool>(opcode, 25);
  info.set_flags = bit::get_bit<u32, bool>(opcode, 20);
  info.reg_dst = bit::get_field<u32, GPR>(opcode, 12, 4);
  info.reg_op1 = bit::get_field<u32, GPR>(opcode, 16, 4);
  info.op2_reg.reg = bit::get_field<u32, GPR>(opcode, 0, 4);
  info.op2_reg.shift.type = bit::get_field<u32, Shift>(opcode, 5, 2);
  info.op2_reg.shift.immediate = !bit::get_bit<u32, bool>(opcode, 4);
  info.op2_reg.shift.amount_reg = bit::get_field<u32, GPR>(opcode, 8, 4);
  info.op2_reg.shift.amount_imm = bit::get_field(opcode, 7, 5);
  info.op2_imm.value = bit::get_field(opcode, 0, 8);
  info.op2_imm.shift = bit::get_field(opcode, 8, 4) * 2;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_move_status_register(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMMoveStatusRegister{};

  info.condition = condition;
  info.immediate = bit::get_bit<u32, bool>(opcode, 25);
  info.spsr = bit::get_bit<u32, bool>(opcode, 22);
  info.fsxc = bit::get_field<u32, int>(opcode, 16, 4);
  info.reg = bit::get_field<u32, GPR>(opcode, 0, 4);
  info.imm = bit::rotate_right(u32(u8(opcode)), bit::get_field(opcode, 8, 4) * 2);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_move_register_status(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMMoveRegisterStatus{};

  info.condition = condition;
  info.spsr = bit::get_bit<u32, bool>(opcode, 22);
  info.reg = bit::get_field<u32, GPR>(opcode, 12, 4);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_multiply(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMMultiply{};

  info.condition = condition;
  info.accumulate = bit::get_bit<u32, bool>(opcode, 21);
  info.set_flags = bit::get_bit<u32, bool>(opcode, 20);
  info.reg_op1 = bit::get_field<u32, GPR>(opcode,  0, 4);
  info.reg_op2 = bit::get_field<u32, GPR>(opcode,  8, 4);
  info.reg_op3 = bit::get_field<u32, GPR>(opcode, 12, 4);
  info.reg_dst = bit::get_field<u32, GPR>(opcode, 16, 4);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_multiply_long(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMMultiplyLong{};

  info.condition = condition;
  info.sign_extend = bit::get_bit<u32, bool>(opcode, 22);
  info.accumulate = bit::get_bit<u32, bool>(opcode, 21);
  info.set_flags = bit::get_bit<u32, bool>(opcode, 20);
  info.reg_op1 = bit::get_field<u32, GPR>(opcode,  0, 4);
  info.reg_op2 = bit::get_field<u32, GPR>(opcode,  8, 4);
  info.reg_dst_lo = bit::get_field<u32, GPR>(opcode, 12, 4);
  info.reg_dst_hi = bit::get_field<u32, GPR>(opcode, 16, 4);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_single_data_swap(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMSingleDataSwap{};

  info.condition = condition;
  info.byte = bit::get_bit<u32, bool>(opcode, 22);
  info.reg_src  = bit::get_field<u32, GPR>(opcode,  0, 4);
  info.reg_dst  = bit::get_field<u32, GPR>(opcode, 12, 4);
  info.reg_base = bit::get_field<u32, GPR>(opcode, 16, 4);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_branch_exchange(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMBranchExchange{};

  info.condition = condition;
  info.reg = bit::get_field<u32, GPR>(opcode, 0, 4);
  info.link = bit::get_bit<u32, bool>(opcode, 5);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_halfword_signed_transfer(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMHalfwordSignedTransfer{};

  info.condition = condition;
  info.pre_increment = bit::get_bit<u32, bool>(opcode, 24);
  info.add = bit::get_bit<u32, bool>(opcode, 23);
  info.immediate = bit::get_bit<u32, bool>(opcode, 22);
  info.writeback = bit::get_bit<u32, bool>(opcode, 21);
  info.load = bit::get_bit<u32, bool>(opcode, 20);
  info.opcode = bit::get_field<u32, int>(opcode, 5, 2);
  info.reg_dst = bit::get_field<u32, GPR>(opcode, 12, 4);
  info.reg_base = bit::get_field<u32, GPR>(opcode, 16, 4);
  info.offset_imm = (opcode & 0xF) | ((opcode >> 4) & 0xF0);
  info.offset_reg = bit::get_field<u32, GPR>(opcode, 0, 4);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_single_data_transfer(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMSingleDataTransfer{};

  info.condition = condition;
  info.immediate = !bit::get_bit<u32, bool>(opcode, 25);
  info.pre_increment = bit::get_bit<u32, bool>(opcode, 24);
  info.add = bit::get_bit<u32, bool>(opcode, 23);
  info.byte = bit::get_bit<u32, bool>(opcode, 22);
  info.writeback = bit::get_bit<u32, bool>(opcode, 21);
  info.load = bit::get_bit<u32, bool>(opcode, 20);
  info.reg_dst = bit::get_field<u32, GPR>(opcode, 12, 4);
  info.reg_base = bit::get_field<u32, GPR>(opcode, 16, 4);
  info.offset_imm = bit::get_field(opcode, 0, 12);
  info.offset_reg.reg = bit::get_field<u32, GPR>(opcode, 0, 4);
  info.offset_reg.shift = bit::get_field<u32, Shift>(opcode, 5, 2);
  info.offset_reg.amount = bit::get_field(opcode, 7, 5);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_block_data_transfer(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMBlockDataTransfer{};

  info.condition = condition;
  info.pre_increment = bit::get_bit<u32, bool>(opcode, 24);
  info.add = bit::get_bit<u32, bool>(opcode, 23);
  info.user_mode = bit::get_bit<u32, bool>(opcode, 22);
  info.writeback = bit::get_bit<u32, bool>(opcode, 21);
  info.load = bit::get_bit<u32, bool>(opcode, 20);
  info.reg_base = bit::get_field<u32, GPR>(opcode, 16, 4);
  info.reg_list = bit::get_field<u32, u16>(opcode, 0, 16);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_branch_relative(Condition condition, u32 opcode, T& client) -> U {
  auto offset = opcode & 0xFFFFFF;

  if (offset & 0x800000) {
    offset |= 0xFF000000;
  }

  offset *= sizeof(u32);

  auto info = ARMBranchRelative{};

  info.condition = condition;
  info.offset = s32(offset);
  info.link = bit::get_bit<u32, bool>(opcode, 24);
  info.exchange = false;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_coprocessor_register_transfer(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMCoprocessorRegisterTransfer{};

  info.condition = condition;
  info.load = bit::get_bit<u32, bool>(opcode, 20);
  info.reg_dst = bit::get_field<u32, GPR>(opcode, 12, 4);
  info.coprocessor_id = bit::get_field(opcode, 8, 4);
  info.opcode1 = bit::get_field(opcode, 21, 3);
  info.cn = bit::get_field(opcode, 16, 4);
  info.cm = bit::get_field(opcode, 0, 4);
  info.opcode2 = bit::get_field(opcode, 5, 3);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_svc(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMException{};
    
  info.condition = condition;
  info.exception = Exception::Supervisor;
  info.svc_comment = opcode & 0x00FFFFFF;
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_count_leading_zeros(Condition condition, u32 opcode, T& client) -> U {
  auto info = ARMCountLeadingZeros{};

  info.condition = condition;
  info.reg_src = bit::get_field<u32, GPR>(opcode, 0, 4);
  info.reg_dst = bit::get_field<u32, GPR>(opcode, 12, 4);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_saturating_add_sub(Condition condition, u32 opcode, T& client) -> U {
  auto op = bit::get_field(opcode, 20, 4);

  if ((op & 0b1001) != 0) {
    return client.Undefined(opcode);
  }

  auto info = ARMSaturatingAddSub{};

  info.condition = condition;
  info.subtract = bit::get_bit<u32, bool>(op, 1);
  info.double_rhs = bit::get_bit<u32, bool>(op, 2);
  info.reg_dst = bit::get_field<u32, GPR>(opcode, 12, 4);
  info.reg_lhs = bit::get_field<u32, GPR>(opcode, 0, 4);
  info.reg_rhs = bit::get_field<u32, GPR>(opcode, 16, 4);
  return client.Handle(info);
}

template<typename T, typename U = typename T::return_type>
inline auto decode_signed_halfword_multiply(Condition condition, u32 opcode, T& client) -> U {
  auto op = bit::get_field(opcode, 21, 4);

  auto x = bit::get_bit<u32, bool>(opcode, 5);
  auto y = bit::get_bit<u32, bool>(opcode, 6);
  auto dst = bit::get_field<u32, GPR>(opcode, 16, 4);
  auto lhs = bit::get_field<u32, GPR>(opcode, 0, 4);
  auto rhs = bit::get_field<u32, GPR>(opcode, 8, 4);
  auto op3 = bit::get_field<u32, GPR>(opcode, 12, 4);

  switch (op) {
    // SMLAxy, SMULxy
    case 0b1000:
    case 0b1011: {
      auto info = ARMSignedHalfwordMultiply{};

      info.condition = condition;
      info.accumulate = op == 0b1000;
      info.x = x;
      info.y = y;
      info.reg_dst = dst;
      info.reg_lhs = lhs;
      info.reg_rhs = rhs;
      info.reg_op3 = op3;
      return client.Handle(info);
    }
    case 0b1001: {
      auto info = ARMSignedWordHalfwordMultiply{};

      info.condition = condition;
      info.accumulate = !x;
      info.y = y;
      info.reg_dst = dst;
      info.reg_lhs = lhs;
      info.reg_rhs = rhs;
      info.reg_op3 = op3;
      return client.Handle(info);
    }
    // SMLALxy
    case 0b1010: {
      auto info = ARMSignedHalfwordMultiplyAccumulateLong{};

      info.condition = condition;
      info.x = x;
      info.y = y;
      info.reg_dst_hi = dst;
      info.reg_dst_lo = op3;
      info.reg_lhs = lhs;
      info.reg_rhs = rhs;
      return client.Handle(info);
    }
  }

  return client.Undefined(opcode);
}

// unconditional opcodes:

template<typename T, typename U = typename T::return_type>
inline auto decode_branch_link_exchange_relative(u32 opcode, T& client) -> U {
  auto offset = opcode & 0xFFFFFF;

  if (offset & 0x800000) {
    offset |= 0xFF000000;
  }

  offset *= sizeof(u32);
  offset += bit::get_field(opcode, 23, 2) & 2;

  auto info = ARMBranchRelative{};

  info.condition = Condition::AL;
  info.offset = s32(offset);
  info.link = true;
  info.exchange = true;
  return client.Handle(info);
}

} // namespace lunatic::frontend::detail

/// Decodes an ARM opcode into one of multiple structures,
/// passes the resulting structure to a client and returns the client's return value.
template<typename T, typename U = typename T::return_type>
inline auto decode_arm(u32 instruction, T& client) -> U {
  auto opcode = instruction & 0x0FFFFFFF;
  auto condition = bit::get_field<u32, Condition>(instruction, 28, 4);

  using namespace detail;

  // TODO: do not decode unconditional opcodes on ARMv4T
  if (condition == Condition::NV) {
    // NOTE: PLD is stubbed and treated like mov r0, r0
    switch(opcode >> 25) {
      case 0b010: return decode_data_processing(Condition::AL, 0x01A00000u, client); // PLD #imm
      case 0b011: return decode_data_processing(Condition::AL, 0x01A00000u, client); // PLD reg
      case 0b101: return decode_branch_link_exchange_relative(opcode, client);
    }

    return client.Undefined(instruction);
  }

  switch (opcode >> 25) {
    case 0b000: {
      // Data processing immediate shift
      // Miscellaneous instructions (A3-4)
      // Data processing register shift
      // Miscellaneous instructions (A3-4)
      // Multiplies (A3-3)
      // Extra load/stores (A3-5)
      bool set_flags = opcode & (1 << 20);
      auto opcode2 = (opcode >> 21) & 0xF;

      if ((opcode & 0x90) == 0x90) {
        // Multiplies (A3-3)
        // Extra load/stores (A3-5)
        if ((opcode & 0x60) != 0) {
          return decode_halfword_signed_transfer(condition, opcode, client);
        } else {
          switch ((opcode >> 23) & 3) {
            case 0b00:
            case 0b01: {
              auto op = bit::get_field(opcode, 21, 4);
              switch (op) {
                case 0b000:
                case 0b001:
                  // MUL, MLA
                  return decode_multiply(condition, opcode, client);
                case 0b100:
                case 0b101:
                case 0b110:
                case 0b111:
                  // UMULL, UMLAL, SMULL, SMLAL
                  return decode_multiply_long(condition, opcode, client);
                default:
                  return client.Undefined(instruction);
              }
            }
            case 0b10:
              return decode_single_data_swap(condition, opcode, client);
            case 0b11:
              // return ARMInstrType::LoadStoreExclusive;
              return client.Undefined(instruction);
          }
        }
      } else if (!set_flags && opcode2 >= 0b1000 && opcode2 <= 0b1011) {
        // Miscellaneous instructions (A3-4)
        if ((opcode & 0xF0) == 0) {
          // return ARMInstrType::StatusTransfer;
          if (bit::get_bit(opcode, 21)) {
            return decode_move_status_register(condition, opcode, client);
          }
          return decode_move_register_status(condition, opcode, client);
        }

        if ((opcode & 0x6000F0) == 0x200010) {
          // Branch and exchange (no link)
          return decode_branch_exchange(condition, opcode, client);
        }

        if ((opcode & 0x6000F0) == 0x200020) {
          // return ARMInstrType::BranchAndExchangeJazelle;
          return client.Undefined(instruction);
        }

        if ((opcode & 0x6000F0) == 0x600010) {
          // TODO: do not decode this instruction on ARMv4T
          return decode_count_leading_zeros(condition, opcode, client);
        }

        if ((opcode & 0x6000F0) == 0x200030) {
          // Branch and exchange with link
          return decode_branch_exchange(condition, opcode, client);
        }

        if ((opcode & 0xF0) == 0x50) {
          return decode_saturating_add_sub(condition, opcode, client);
        }

        if ((opcode & 0x6000F0) == 0x200070) {
          // return ARMInstrType::Breakpoint;
          return client.Undefined(instruction);
        }

        if ((opcode & 0x90) == 0x80) {
          // Signed halfword multiply (ARMv5 upwards):
          // SMLAxy, SMLAWy, SMULWy, SMLALxy, SMULxy
          return decode_signed_halfword_multiply(condition, opcode, client);
        }
      }

      // Data processing immediate shift
      // Data processing register shift
      return decode_data_processing(condition, opcode, client);
    }
    case 0b001: {
      // Data processing immediate
      // Undefined instruction
      // Move immediate to status register
      bool set_flags = opcode & (1 << 20);

      if (!set_flags) {
        auto opcode2 = (opcode >> 21) & 0xF;

        switch (opcode2) {
          case 0b1000:
          case 0b1010:
            // return ARMInstrType::Undefined;
            return client.Undefined(instruction);
          case 0b1001:
          case 0b1011:
            // return ARMInstrType::StatusTransfer;
            return decode_move_status_register(condition, opcode, client);
        }
      }

      return decode_data_processing(condition, opcode, client);
    }
    case 0b010: {
      // Load/store immediate offset
      return decode_single_data_transfer(condition, opcode, client);
    }
    case 0b011: {
      // Load/store register offset
      // Media instructions
      // Architecturally Undefined
      if (opcode & 0x10) {
        // return ARMInstrType::Media;
        return client.Undefined(instruction);
      }

      return decode_single_data_transfer(condition, opcode, client);
    }
    case 0b100: {
      // Load/store multiple
      return decode_block_data_transfer(condition, opcode, client);
    }
    case 0b101: {
      // Branch and branch with link
      return decode_branch_relative(condition, opcode, client);
    }
    case 0b110: {
      // Coprocessor load/store and double register transfers
      // TODO: differentiate between load/store and double reg transfer instructions.
      // return ARMInstrType::CoprocessorLoadStoreAndDoubleRegXfer;
      return client.Undefined(instruction);
    }
    case 0b111: {
      // Coprocessor data processing
      // Coprocessor register transfers
      // Software interrupt
      if ((opcode & 0x1000010) == 0) {
        // return ARMInstrType::CoprocessorDataProcessing;
        return client.Undefined(instruction);
      }

      if ((opcode & 0x1000010) == 0x10) {
        return decode_coprocessor_register_transfer(condition, opcode, client);
      }

      return decode_svc(condition, opcode, client);
    }
  }

  return client.Undefined(instruction);
}

} // namespace lunatic::frontend
} // namespace lunatic
