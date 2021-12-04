# xe
io_uring event loop with c++20 coroutine support

## building

### prerequisites
1. [liburing](https://github.com/axboe/liburing)
2. cmake <code>sudo apt install cmake</code>
3. g++/clang++ with coroutine support

### build
```bash
1. mkdir build; cd build
2. cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="/usr/bin/g++-11" ..
   OR
   cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="/usr/bin/clang++-12" ..
3. cmake --build .
4. ./echoserver
```

## example usage
Echo Server: [<code>example/echoserver.cpp</code>](https://github.com/ilikdoge/xe/blob/master/example/echoserver.cpp)<br>
C++ 20 Coroutines: [<code>example/coroutine_echoserver.cpp</code>](https://github.com/ilikdoge/xe/blob/master/example/coroutine_echoserver.cpp)

## benchmarks

Tested on a Ryzen 7 5800X on Pop OS (Linux 5.11) using [rust_echo_bench](https://github.com/haraldh/rust_echo_bench)

### echoserver.cpp
&nbsp;&nbsp;&nbsp;&nbsp;run with <code>cargo run --release -- --address "127.0.0.1:8080"</code><br>
&nbsp;&nbsp;&nbsp;&nbsp;50 concurrent clients, 512b each: 400,000 reqs/sec<br>
&nbsp;&nbsp;&nbsp;&nbsp;similar performance achieved with <code>example/coroutine_echoserver.cpp</code>

raw performance (<code>IORING_OP_NOP</code>)
<br>&nbsp;&nbsp;&nbsp;&nbsp;
ring size 512 sqes and cqes: 15,950,000 nops/sec