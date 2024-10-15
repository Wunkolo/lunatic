cmake_minimum_required(VERSION 3.2)

project(lunatic CXX)

include(../CMakeModules/FindVTune.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (${CMAKE_CXX_COMPILER_ARCHITECTURE_ID} MATCHES "x64|x86_64|AMD64")
  set(ARCH_SPECIFIC_SOURCES
    backend/x86_64/backend.cpp
    backend/x86_64/compile_alu.cpp
    backend/x86_64/compile_context.cpp
    backend/x86_64/compile_coprocessor.cpp
    backend/x86_64/compile_flags.cpp
    backend/x86_64/compile_flush.cpp
    backend/x86_64/compile_memory.cpp
    backend/x86_64/compile_multiply.cpp
    backend/x86_64/compile_shift.cpp
    backend/x86_64/register_allocator.cpp
  )

  set(ARCH_SPECIFIC_HEADERS
    backend/x86_64/backend.hpp
    backend/x86_64/common.hpp
    backend/x86_64/register_allocator.hpp
    backend/x86_64/vtune.hpp
  )
else()
  message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(SOURCES
  common/pool_allocator.cpp
  frontend/ir/emitter.cpp
  frontend/ir_opt/constant_propagation.cpp
  frontend/ir_opt/context_load_store_elision.cpp
  frontend/ir_opt/dead_code_elision.cpp
  frontend/ir_opt/dead_flag_elision.cpp
  frontend/translator/handle/block_data_transfer.cpp
  frontend/translator/handle/branch_exchange.cpp
  frontend/translator/handle/branch_relative.cpp
  frontend/translator/handle/coprocessor_register_transfer.cpp
  frontend/translator/handle/count_leading_zeros.cpp
  frontend/translator/handle/data_processing.cpp
  frontend/translator/handle/exception.cpp
  frontend/translator/handle/halfword_signed_transfer.cpp
  frontend/translator/handle/multiply.cpp
  frontend/translator/handle/multiply_long.cpp
  frontend/translator/handle/saturating_add_sub.cpp
  frontend/translator/handle/signed_halfword_multiply.cpp
  frontend/translator/handle/single_data_swap.cpp
  frontend/translator/handle/single_data_transfer.cpp
  frontend/translator/handle/status_transfer.cpp
  frontend/translator/handle/thumb_bl_suffix.cpp
  frontend/translator/translator.cpp
  frontend/state.cpp
  jit.cpp
)

set(HEADERS
  backend/backend.hpp
  common/bit.hpp
  common/compiler.hpp
  common/aligned_memory.hpp
  common/meta.hpp
  common/optional.hpp
  common/pool_allocator.hpp
  frontend/decode/definition/block_data_transfer.hpp
  frontend/decode/definition/branch_relative.hpp
  frontend/decode/definition/coprocessor_register_transfer.hpp
  frontend/decode/definition/count_leading_zeros.hpp
  frontend/decode/definition/branch_exchange.hpp
  frontend/decode/definition/common.hpp
  frontend/decode/definition/data_processing.hpp
  frontend/decode/definition/exception.hpp
  frontend/decode/definition/halfword_signed_transfer.hpp
  frontend/decode/definition/multiply.hpp
  frontend/decode/definition/multiply_long.hpp
  frontend/decode/definition/saturating_add_sub.hpp
  frontend/decode/definition/signed_halfword_multiply.hpp
  frontend/decode/definition/single_data_swap.hpp
  frontend/decode/definition/single_data_transfer.hpp
  frontend/decode/definition/status_transfer.hpp
  frontend/decode/definition/thumb_bl_suffix.hpp
  frontend/decode/arm.hpp
  frontend/decode/thumb.hpp
  frontend/ir/emitter.hpp
  frontend/ir/opcode.hpp
  frontend/ir/register.hpp
  frontend/ir/value.hpp
  frontend/ir_opt/constant_propagation.hpp
  frontend/ir_opt/context_load_store_elision.hpp
  frontend/ir_opt/dead_code_elision.hpp
  frontend/ir_opt/dead_flag_elision.hpp
  frontend/ir_opt/pass.hpp
  frontend/translator/translator.hpp
  frontend/basic_block.hpp
  frontend/basic_block_cache.hpp
  frontend/state.hpp
)

set(HEADERS_PUBLIC
  ../include/lunatic/detail/meta.hpp
  ../include/lunatic/detail/punning.hpp
  ../include/lunatic/coprocessor.hpp
  ../include/lunatic/cpu.hpp
  ../include/lunatic/integer.hpp
  ../include/lunatic/memory.hpp
)

add_library(lunatic STATIC ${SOURCES} ${HEADERS} ${ARCH_SPECIFIC_SOURCES} ${ARCH_SPECIFIC_HEADERS} ${HEADERS_PUBLIC})

target_include_directories(lunatic PRIVATE .)
target_include_directories(lunatic PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(lunatic PRIVATE fmt)

if (${CMAKE_CXX_COMPILER_ARCHITECTURE_ID} MATCHES "x64|x86_64|AMD64")
  target_link_libraries(lunatic PRIVATE xbyak dynarmic)

  if (LUNATIC_INCLUDE_XBYAK_FROM_DIRECTORY)
    target_compile_definitions(lunatic PRIVATE LUNATIC_INCLUDE_XBYAK_FROM_DIRECTORY)
  endif()

  if (VTune_FOUND AND LUNATIC_USE_VTUNE)
    message(STATUS "lunatic: Adding VTune JIT Profiling API from ${VTune_LIBRARIES}")
    target_include_directories(lunatic PRIVATE ${VTune_INCLUDE_DIRS})
    target_link_libraries(lunatic PRIVATE ${VTune_LIBRARIES})
    target_compile_definitions(lunatic PRIVATE LUNATIC_USE_VTUNE=1)
  endif()
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_compile_definitions(lunatic PRIVATE NOMINMAX)
endif()
