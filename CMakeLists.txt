cmake_minimum_required(VERSION 3.0.0)
project(xe CXX)

set(XE_OPTFLAGS "")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "-fcoroutines")
	set(XE_OPTFLAGS "-flto")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "-std=c++2a -fcoroutines-ts -stdlib=libc++")
	add_definitions(-DXE_COROUTINES_EXPERIMENTAL)
else()
	message(SEND_ERROR "unsupported compiler")
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	add_definitions(-DXE_DEBUG)
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${XE_OPTFLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "${XE_OPTFLAGS}")
endif()

include_directories(".")
include_directories("ev")
include_directories("arch")

set(SOURCES "")
file(GLOB SOURCES
	"util/*.cpp"
	"string/*.cpp"
	"ev/*.cpp"
	"net/*.cpp"
	"arch/default/*.cpp")

add_library(xe ${SOURCES})
target_link_libraries(xe uring pthread)

add_executable(echoserver "example/echoserver.cpp")
target_link_libraries(echoserver xe)

add_executable(coroutine_echoserver "example/coroutine_echoserver.cpp")
target_link_libraries(coroutine_echoserver xe)

add_executable(client "example/client.cpp")
target_link_libraries(client xe)

add_executable(timer "example/timer.cpp")
target_link_libraries(timer xe)