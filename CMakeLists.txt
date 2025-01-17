cmake_minimum_required( VERSION 3.2.2 )
project( base2 )

### Verbosity
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

set( CMAKE_COLOR_MAKEFILE ON )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

### Optimizations
if( MSVC )
	add_compile_options( /W4 )
elseif( CMAKE_COMPILER_IS_GNUCXX )
	add_compile_options( -march=native )
	add_compile_options( -Wall )
	add_compile_options( -Wextra )
endif()

### libbase2
add_library(
	base2
	source/Base2.cpp
)
target_include_directories(
	base2
	PUBLIC
	include
)

## base2
add_executable(
	base2-bin
	source/main.cpp
)
set_target_properties(
	base2-bin PROPERTIES
	OUTPUT_NAME "base2"
)
target_include_directories(
	base2-bin
	PRIVATE
	include
)
target_link_libraries(
	base2-bin
	PRIVATE
	base2
)

### Tests
enable_testing()

find_package(Catch2 3 REQUIRED)
add_library(catch ALIAS Catch2::Catch2)

add_executable(
	base2-test
	tests/base2-enc.cpp
)
target_include_directories(
	base2-test
	PRIVATE
	include
)
target_link_libraries(
	base2-test
	PRIVATE
	base2
	Catch2::Catch2WithMain
)

add_test(base2-test base2-test)

