set(HLASM_EXTRA_FLAGS "")
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
  list(APPEND HLASM_EXTRA_FLAGS /Zc:preprocessor /W4)
  if (${CMAKE_SYSTEM_VERSION} VERSION_LESS "10.0.20348")
    message(STATUS "Windows SDK ${CMAKE_SYSTEM_VERSION}, suppressing warning C5105")
    list(APPEND HLASM_EXTRA_FLAGS /wd5105)
  endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  list(APPEND HLASM_EXTRA_FLAGS -Wall -Wextra -Wconversion)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  list(APPEND HLASM_EXTRA_FLAGS -Wall -Wextra -Wconversion)
else()
endif()

if(BUILD_FUZZER)
  list(APPEND HLASM_EXTRA_FLAGS -fsanitize=fuzzer-no-link)
endif()

set(HLASM_EXTRA_LINKER_FLAGS "")
if(APPLE_STATIC_CRT)
  file(GLOB_RECURSE libcpp_file "${LLVM_PATH}/lib/**/libc++.a")
  file(GLOB_RECURSE libcppabi_file "${LLVM_PATH}/lib/**/libc++abi.a")

  message(STATUS "Using libc++ in ${libcpp_file}" )

  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/libcxx")
  file(COPY_FILE "${libcpp_file}" "${CMAKE_CURRENT_BINARY_DIR}/libcxx/libc++.a")
  file(COPY_FILE "${libcppabi_file}" "${CMAKE_CURRENT_BINARY_DIR}/libcxx/libc++abi.a")

  list(APPEND HLASM_EXTRA_LINKER_FLAGS -nostdlib++ "-L${CMAKE_CURRENT_BINARY_DIR}/libcxx" -Wl,-hidden-lc++ -Wl,-hidden-lc++abi)
endif()
