if(NOT LUNATIC_USE_EXTERNAL_FMT)
  add_subdirectory(fmt)
endif()

if (NOT TARGET xbyak)
  add_subdirectory(xbyak)
endif()

add_subdirectory(dynarmic)