cmake_minimum_required(VERSION 3.16)
project(xe CXX)
project(xurl CXX)
cmake_policy(SET CMP0069 NEW)
include(CheckIPOSupported)
check_ipo_supported()

option(XE_ENABLE_EXAMPLES "Enable examples" ON)
option(XE_ENABLE_XURL "Enable xurl library" OFF)

option(XE_USE_WOLFSSL "Use wolfssl" OFF)
option(XE_USE_OPENSSL "Use wolfssl" OFF)

option(XE_FLTO "Enable full program optimization on release mode" ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -Wall")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10")
		message(SEND_ERROR "gcc must be atleast version 10")
	elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "11")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
	endif()

	add_definitions(-U_FORTIFY_SOURCE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12")
		message(SEND_ERROR "clang must be atleast version 12")
	elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
else()
	message(SEND_ERROR "Compile with gcc 10, clang 12, or newer")
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -march=native -mtune=native")
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	set(XE_DEBUG TRUE)
elseif((NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug") AND XE_FLTO)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(simd)
include(cpu)
include(config)
include(ssl)

include_directories("include")

set(SOURCES "")
file(GLOB SOURCES
	"xe/*.cc"
	"xe/io/*.cc"
	"xutil/*.cc"
	"xstd/*.cc"
)

set(XURL_SOURCES "")
file(GLOB XURL_SOURCES
	"xurl/*.cc"
	"xurl/proto/*.cc"
)

set(XURL_LINK "")
if(XE_ENABLE_XURL)
	set(XURL_LINK ${XURL_LINK} cares)

	if(XE_USE_WOLFSSL)
		set(XURL_SOURCES ${XURL_SOURCES} "xurl/ssl/wolfssl.cc")
		set(XURL_LINK ${XURL_LINK} wolfssl)
	elseif(XE_USE_OPENSSL)
		set(XURL_LINK ${XURL_LINK} ssl crypto)
		set(XURL_SOURCES ${XURL_SOURCES} "xurl/ssl/openssl.cc")
	else()
		set(XURL_SOURCES ${XURL_SOURCES} "xurl/ssl/nossl.cc")
	endif()
endif()

set(ARCH "")
file(GLOB ARCH "xarch/default/*.cc")

add_library(xe ${SOURCES} ${ARCH})
target_include_directories(xe INTERFACE include)
target_link_libraries(xe uring)

if(XE_ENABLE_XURL)
	add_library(xurl ${XURL_SOURCES})
	target_include_directories(xurl INTERFACE include)
	target_link_libraries(xurl xe ${XURL_LINK})
endif()

if(XE_ENABLE_EXAMPLES)
	add_executable(echoserver "example/echoserver.cc")
	target_link_libraries(echoserver xe)

	add_executable(pollechoserver "example/pollechoserver.cc")
	target_link_libraries(pollechoserver xe)

	add_executable(client "example/client.cc")
	target_link_libraries(client xe)

	add_executable(timer "example/timer.cc")
	target_link_libraries(timer xe)

	add_executable(coroutine_echoserver "example/coroutines/echoserver.cc")
	target_link_libraries(coroutine_echoserver xe)

	add_executable(coroutine_client "example/coroutines/client.cc")
	target_link_libraries(coroutine_client xe)

	add_executable(coroutine_file "example/coroutines/file.cc")
	target_link_libraries(coroutine_file xe)

	add_executable(coroutine_hello_world "example/coroutines/hello_world.cc")
	target_link_libraries(coroutine_hello_world xe)

	if(XE_ENABLE_XURL)
		add_executable(http "example/http.cc")
		target_link_libraries(http xe xurl)
	endif()
endif()