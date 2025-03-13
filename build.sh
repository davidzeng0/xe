#!/bin/bash
# For Ubuntu and similar

sudo apt install -y autoconf libtool make cmake ninja-build gcc g++-11

git clone --depth 1 https://github.com/wolfSSL/wolfssl.git
cd wolfssl
./autogen.sh
./configure --enable-fast-rsa --enable-intelasm --enable-sp --enable-sp-math --enable-sp-asm --enable-fastmath --enable-tls13 --enable-sni --enable-aesni --enable-alpn --enable-aesgcm --enable-harden --enable-opensslextra --enable-aesgcm-stream
make
sudo make install
cd ..

git clone --depth 1 https://github.com/c-ares/c-ares.git
cd c-ares
autoreconf -fi
./configure
make -j $(nproc)
sudo make install
cd ..

git clone --depth 1 https://github.com/axboe/liburing.git
cd liburing
./configure
make -j $(nproc)
sudo make install
cd ..

sudo ldconfig

git submodule update --init --recursive
mkdir build
cd build
cmake -G "Ninja" -DXE_ENABLE_XURL="ON" -DXE_USE_WOLFSSL="ON" -DCMAKE_BUILD_TYPE="Release" -DCMAKE_CXX_COMPILER="g++-11" ..
cmake --build .