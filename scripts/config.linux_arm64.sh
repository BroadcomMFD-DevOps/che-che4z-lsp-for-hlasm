#!/usr/bin/env sh
set -e
cmake -G Ninja -DCMAKE_SYSTEM_NAME="Linux" -DCMAKE_C_COMPILER="$PWD/../toolchain/bin/aarch64-linux-musl-gcc" -DCMAKE_CXX_COMPILER="$PWD/../toolchain/bin/aarch64-linux-musl-g++" -DCMAKE_BUILD_TYPE=Release -DDISCOVER_TESTS=Off -DCMAKE_EXE_LINKER_FLAGS="-static" -DBUILD_VSIX=Off -DUSE_PRE_GENERATED_GRAMMAR="generated_parser" ../
