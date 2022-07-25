cmake_minimum_required(VERSION 3.16)
project(xe CXX)
cmake_policy(SET CMP0069 NEW)
include(CheckIPOSupported)
check_ipo_supported()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(simd)
include(cpu)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines -fno-pie -no-pie")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts -stdlib=libc++")
else()
	message(SEND_ERROR "Unsupported Compiler")
endif()

set(XE_FLTO TRUE)
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	set(XE_DEBUG TRUE)
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Release")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -march=native -mtune=native")
	if(XE_FLTO)
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
	endif()
endif()

include(debug)

include_directories(".")
include_directories("include")
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(SOURCES "")
file(GLOB SOURCES
	"xe/*.cc"
	"xe/io/*.cc"
	"xurl/*.cc"
	"xurl/proto/*.cc"
	"xutil/*.cc"
)

set(ARCH "")
file(GLOB ARCH "xe/arch/default/*.cc")

add_library(xe ${SOURCES} ${ARCH})
target_link_libraries(xe uring cares wolfssl)
target_include_directories(xe INTERFACE include)

add_executable(echoserver "example/echoserver.cc")
target_link_libraries(echoserver xe)

add_executable(coroutine_echoserver "example/coroutine_echoserver.cc")
target_link_libraries(coroutine_echoserver xe)

add_executable(client "example/client.cc")
target_link_libraries(client xe)

add_executable(timer "example/timer.cc")
target_link_libraries(timer xe)