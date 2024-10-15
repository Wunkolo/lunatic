cmake_minimum_required(VERSION 3.2)
project(lunatic-test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)

option(LUNATIC_NDS_ROM_TEST "Build the NDS ROM unit test" OFF)

if(LUNATIC_ROM_TEST)
	set(SOURCES
	main.cpp)

	set(HEADERS)

	include(FindSDL2)
	find_package(SDL2 REQUIRED)

	add_executable(test ${SOURCES} ${HEADERS})
	target_link_libraries(test lunatic fmt ${SDL2_LIBRARY})
	target_include_directories(test PRIVATE . ${SDL2_INCLUDE_DIR})

	# Hack to access the internal library headers for testing
	target_include_directories(test PRIVATE ../src)
	target_link_libraries(test xbyak)

	if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(test PRIVATE /clang:-fbracket-depth=4096)
	endif()
	else ()
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(test PRIVATE -fbracket-depth=4096)
	endif()
	endif()
endif()
