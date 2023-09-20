#!/usr/bin/env bash
set -e
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_VSIX=Off -DUSE_PRE_GENERATED_GRAMMAR="generated_parser" ../
