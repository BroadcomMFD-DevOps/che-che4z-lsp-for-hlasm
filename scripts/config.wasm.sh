#!/usr/bin/env bash
set -e
emcmake cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DDISCOVER_TESTS=Off -DWITH_LIBCXX=Off -DWITH_STATIC_CRT=Off -DCMAKE_CXX_FLAGS="-s USE_PTHREADS=1 -fexceptions" -DUSE_PRE_GENERATED_GRAMMAR="generated_parser" -DCMAKE_EXE_LINKER_FLAGS="-s PTHREAD_POOL_SIZE=8 -s TOTAL_MEMORY=268435456 -s PROXY_TO_PTHREAD=1 -s EXPORT_ES6=1 -s EXIT_RUNTIME=1 --bind" -DCMAKE_CROSSCOMPILING_EMULATOR="node;--experimental-wasm-threads;--experimental-wasm-bulk-memory" -DCMAKE_EXECUTABLE_SUFFIX_CXX=.mjs -Dgtest_disable_pthreads=On -DBUILD_VSIX=Off ../
