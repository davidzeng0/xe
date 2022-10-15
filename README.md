# xe

Lightweight Async I/O & Transfer Library Powered by io_uring

This library does not use exceptions

### libraries

| name      | description                                               |
| --------- | ----------------------------------------------------------|
| xe        | io_uring event loop with support for c++ 20 coroutines    |
| xurl      | url client library (WIP)                                  |
| xstd      | structures and algorithms                                 |
| xutil     | utility library                                           |
| xarch     | architecture specific optimized subroutines               |

## using

### prerequisites
- linux kernel 5.11 or later
- liburing ([github link](https://github.com/axboe/liburing))
- cmake <code>apt install cmake</code>
- g++ 10 or newer, or clang 12 or newer <code>apt install g++-10/clang++-12</code>

### xurl prerequisites (if enabled, disabled by default)
- c-ares ([github link](https://github.com/c-ares/c-ares))

one of:
- OpenSSL >= 1.1.1 <code>apt install libssl-dev</code>
- wolfSSL ([github link](https://github.com/wolfSSL/wolfssl), see [build flags](https://github.com/ilikdoge/xe/blob/master/build.sh#L9))

### use with cmake
```cmake
project(sample CXX)

# remove two lines below to disable xurl
set(XE_ENABLE_XURL ON)
set(XE_USE_OPENSSL ON) # alternatively set(XE_USE_WOLFSSL ON)

FetchContent_Declare(xe GIT_REPOSITORY https://github.com/ilikdoge/xe.git GIT_TAG master)
FetchContent_MakeAvailable(xe)

...

target_link_libraries(sample xe)

# with xurl
target_link_libraries(sample xe xurl)
```

### build xe
```bash
mkdir build; cd build

# with ninja
cmake -G "Ninja" -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="/your/cxx/compiler" ..
# enable xurl
cmake -G "Ninja" -DXE_ENABLE_XURL="ON" -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="/your/cxx/compiler" ..

# without ninja
cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="/your/cxx/compiler" ..

cmake --build .
```