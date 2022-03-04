# xe
io_uring event loop with c++20 coroutines

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
```