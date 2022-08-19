# libraries
This project is a WIP.

## xarch
architecture specific optimized subroutines

## xutil
utility library

## xstd
structures and algorithms

## xe
- io_uring event loop
- event completions with c++20 coroutines

## xurl
url client library
<br>
protocols:
- file
- http
- websocket

# limitations
- linux kernel 5.11 or newer

# compiling

## prerequisites
1. liburing ([github](https://github.com/axboe/liburing))
2. c-ares ([github](https://github.com/c-ares/c-ares))
3. wolfSSL ([github](https://github.com/wolfSSL/wolfssl))
4. cmake <code>apt install cmake</code>
5. g++/clang++ with coroutine support <code>apt install g++-11/clang++-12</code>

## build
```bash
mkdir build; cd build
cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="/your/cxx/compiler" ..
cmake --build .
```
