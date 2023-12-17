
cmake_minimum_required(VERSION 3.5)

project(dynarmic CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HEADERS_PUBLIC
  include/dynarmic/devirtualize_x64.hpp
  include/mcl/bit_cast.hpp
  include/mcl/mp/typelist/list.hpp
  include/mcl/type_traits/function_info.hpp
)

add_library(dynarmic INTERFACE ${HEADERS_PUBLIC})
target_include_directories(dynarmic INTERFACE include)