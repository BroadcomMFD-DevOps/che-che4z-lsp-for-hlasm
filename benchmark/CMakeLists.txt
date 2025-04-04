# Copyright (c) 2019 Broadcom.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
#
# This program and the accompanying materials are made
# available under the terms of the Eclipse Public License 2.0
# which is available at https://www.eclipse.org/legal/epl-2.0/
#
# SPDX-License-Identifier: EPL-2.0
#
# Contributors:
#   Broadcom, Inc. - initial API and implementation

project(benchmark)

add_executable(benchmark
    benchmark.cpp
    diagnostic_counter.h)

target_compile_features(benchmark PRIVATE cxx_std_20)
target_compile_options(benchmark PRIVATE ${HLASM_EXTRA_FLAGS})
set_target_properties(benchmark PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(benchmark
    PRIVATE
    ../parser_library/src
)

target_link_libraries(benchmark PRIVATE nlohmann_json::nlohmann_json)

target_link_libraries(benchmark PRIVATE parser_library hlasm_utils)

target_link_libraries(benchmark PRIVATE Threads::Threads)

target_link_options(benchmark PRIVATE ${HLASM_EXTRA_LINKER_FLAGS})
