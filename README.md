# xe

Hyper Fast io_uring Async I/O Library

Documentation is still WIP

### Features
#### xe
- High Performance
- Low Latency I/O
- Fast Nanosecond Precision Timers
- Ultra Lightweight (0.6% overhead for [echoserver](https://github.com/ilikdoge/xe/blob/master/example/echoserver.cc))

#### xe/io
- Socket and File classes and utilities
- Epoll-like [fast poll handle](https://github.com/ilikdoge/xe/blob/master/xe/io/poll.h)

#### xurl (WIP)
- Async DNS resolution
- HTTP, WS, and FILE url protocols (WIP)

### Examples
```c++
// hello world snippet
static task run(xe_loop& loop){
	char msg[] = "Hello World!\n";

	co_await loop.queue(xe_op::write(STDOUT_FILENO, msg, sizeof(msg) - 1, 0));
}
```
Above snippet from [hello_world.cc](https://github.com/ilikdoge/xe/blob/master/example/coroutines/hello_world.cc)
```c++
// echo server snippet
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
Above snippet from [echoserver.cc](https://github.com/ilikdoge/xe/blob/master/example/coroutines/echoserver.cc)

See [examples](https://github.com/ilikdoge/xe/tree/master/example) and [coroutine examples](https://github.com/ilikdoge/xe/tree/master/example/coroutines)

### Running Examples
##### See [Building xe](https://github.com/ilikdoge/xe#build-xe) below
```bash
cd build
./coroutine_hello_world
./timer
./coroutine_file
./http # only if xurl enabled with openssl or wolfssl
./coroutine_echoserver
./echoserver
```

### Using

#### Prerequisites
- Linux Kernel 5.11 or later
- liburing <code>apt install liburing-dev</code> or install from [source](https://github.com/axboe/liburing)
- cmake <code>apt install cmake</code>
- g++ 10 or newer, or clang 12 or newer <code>apt install g++-10/clang++-12</code>

#### xurl prerequisites (only if enabled, disabled by default)
- c-ares <code>apt install libc-ares-dev</code> or install from [source](https://github.com/c-ares/c-ares)

One of:
- OpenSSL >= 1.1.1 <code>apt install libssl-dev</code>
- wolfSSL (must be installed from [source](https://github.com/wolfSSL/wolfssl), see [build flags](https://github.com/ilikdoge/xe/blob/master/build.sh#L9))

#### Use with cmake
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

#### Build xe
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