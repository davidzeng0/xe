# xe
Hyper Fast I/O Event Loop Using io_uring with C++20 Coroutine Support

## building

### prerequisites
1. [liburing](https://github.com/axboe/liburing)
2. cmake <code>sudo apt install cmake</code>
3. g++/clang++ with coroutine support

### build
```bash
1. mkdir build; cd build
2. cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="/usr/bin/g++-10" ..
   OR
   cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="/usr/bin/clang++" ..
3. cmake --build .
4. ./echoserver
```

## docs
I am considering releasing documentation but for now [<code>example/echoserver.cpp</code>](https://github.com/ilikdoge/xe/blob/main/example/echoserver.cpp) should be enough<br>
Coroutine example in [<code>example/coroutine_echoserver.cpp</code>](https://github.com/ilikdoge/xe/blob/main/example/coroutine_echoserver.cpp)

## benchmarks

Tested on an i7-8650u on WSL (Linux 5.10) using [rust_echo_bench](https://github.com/haraldh/rust_echo_bench)

### echoserver.cpp
&nbsp;&nbsp;&nbsp;&nbsp;run with <code>cargo run --release -- --address "127.0.0.1:8080"</code><br>
&nbsp;&nbsp;&nbsp;&nbsp;50 concurrent clients, 512b each: 300,000 reqs/sec<br>
&nbsp;&nbsp;&nbsp;&nbsp;similar performance achieved with <code>example/coroutine_echoserver.cpp</code>

raw performance (<code>IORING_OP_NOP</code>)
<br>&nbsp;&nbsp;&nbsp;&nbsp;
ring size 512 sqes and cqes: 13,300,000 nops/sec

Tested on a Ryzen 7 5800X on Pop OS (Linux 5.8) using [rust_echo_bench](https://github.com/haraldh/rust_echo_bench)

### echoserver.cpp
&nbsp;&nbsp;&nbsp;&nbsp;run with <code>cargo run --release -- --address "127.0.0.1:8080"</code><br>
&nbsp;&nbsp;&nbsp;&nbsp;50 concurrent clients, 512b each: 1,100,000 reqs/sec<br>
&nbsp;&nbsp;&nbsp;&nbsp;similar performance achieved with <code>example/coroutine_echoserver.cpp</code>

raw performance (<code>IORING_OP_NOP</code>)
<br>&nbsp;&nbsp;&nbsp;&nbsp;
ring size 512 sqes and cqes: 15,950,000 nops/sec