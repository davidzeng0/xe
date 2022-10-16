# xe

Lightweight Async I/O & Transfer Library Powered by io_uring

This library does not use exceptions

Documentation is a WIP

### features
#### xe
- high performance
- ultra lightweight (2.2% overhead for [coroutine_echoserver](https://github.com/ilikdoge/xe/blob/master/example/coroutines/echoserver.cc))
- low latency I/O
- fast high resolution timers
- error handling for ring submit queue

#### xe/io
- socket and file handles
- epoll-like [fast poll handle](https://github.com/ilikdoge/xe/blob/master/xe/io/poll.h) for io_uring

#### xurl
- async DNS resolution
- http, websocket, and file url protocols (WIP)

### examples
```c++
// Hello World Example
static task run(xe_loop& loop){
	char msg[] = "Hello World!\n";

	co_await loop.write(STDOUT_FILENO, msg, sizeof(msg) - 1, 0);
}
```
Above example from [hello_world.cc](https://github.com/ilikdoge/xe/blob/master/example/coroutines/hello_world.cc)
```c++
// Echo Server Example
static task echo(xe_socket& socket){
	byte buf[16384];
	int result;

	while(true){
		result = co_await socket.recv(buf, 16384, 0);

		if(result <= 0)
			break;
		result = co_await socket.send(buf, result, 0);

		if(result < 0)
			break;
	}
}
```
Above example from [echoserver.cc](https://github.com/ilikdoge/xe/blob/master/example/coroutines/echoserver.cc)

See [examples](https://github.com/ilikdoge/xe/tree/master/example) and [coroutine examples](https://github.com/ilikdoge/xe/tree/master/example/coroutines)

### running examples
```bash
cd build
./coroutine_hello_world
./timer
./coroutine_file
./http # only if xurl enabled with openssl or wolfssl
./coroutine_echoserver
./echoserver
```

### libraries

| name      | description                                               |
| --------- | ----------------------------------------------------------|
| xe        | io_uring event loop with support for c++ 20 coroutines    |
| xurl      | url client library (WIP)                                  |
| xstd      | structures and algorithms                                 |
| xutil     | utility library                                           |
| xarch     | architecture specific optimized subroutines               |

### using

#### prerequisites
- linux kernel 5.11 or later
- liburing ([github link](https://github.com/axboe/liburing))
- cmake <code>apt install cmake</code>
- g++ 10 or newer, or clang 12 or newer <code>apt install g++-10/clang++-12</code>

#### xurl prerequisites (if enabled, disabled by default)
- c-ares ([github link](https://github.com/c-ares/c-ares))

one of:
- OpenSSL >= 1.1.1 <code>apt install libssl-dev</code>
- wolfSSL ([github link](https://github.com/wolfSSL/wolfssl), see [build flags](https://github.com/ilikdoge/xe/blob/master/build.sh#L9))

#### use with cmake
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

#### build xe
```bash
mkdir build; cd build

# with ninja
cmake -G "Ninja" -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="/your/cxx/compiler" ..
# enable xurl and use openssl
cmake -G "Ninja" -DXE_ENABLE_XURL="ON" -DXE_USE_OPENSSL="ON" -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="/your/cxx/compiler" ..

# without ninja
cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="/your/cxx/compiler" ..

cmake --build .
```