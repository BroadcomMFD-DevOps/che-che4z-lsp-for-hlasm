if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
  string(REGEX REPLACE " /W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /wd4251 /wd4996")
  if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
      set(MY_CXX_WARNING_FLAGS "  /w35038 /Zc:preprocessor")
  endif()
  if (${CMAKE_SYSTEM_VERSION} VERSION_LESS "10.0.20348")
    message(STATUS "Windows SDK ${CMAKE_SYSTEM_VERSION}, suppressing warning C5105")
    set(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} /wd5105")
  endif()
else()
  set(MY_CXX_WARNING_FLAGS "  -Wall -Wextra -Wno-attributes -Wno-unknown-pragmas")
endif()

set(CMAKE_CXX_FLAGS                  "${CMAKE_CXX_FLAGS} ${MY_CXX_WARNING_FLAGS}")

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
  string(REGEX REPLACE " /Ob[0-4]" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REGEX REPLACE " /Ob[0-4]" "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
  string(REGEX REPLACE " /Ob[0-4]" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  string(REGEX REPLACE " /Ob[0-4]" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  if(WITH_STATIC_CRT)
    set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_CXX_FLAGS_RELEASE
        )
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()

    string(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  endif()

  if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
      string(REPLACE "/Zi" "/ZI" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
      string(REPLACE "/ZI" "/Zi" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
      set(MY_CXX_WARNING_FLAGS "${MY_CXX_WARNING_FLAGS} /MP")
  endif()
  
  set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} /Od ${MY_CXX_WARNING_FLAGS}")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL} /O1 /Oi /Ob2 /Gy /DNDEBUG ${MY_CXX_WARNING_FLAGS}")
  set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Oi /Ob2 /Gy /DNDEBUG ${MY_CXX_WARNING_FLAGS}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /O2 /Oi /Ob2 /Gy ${MY_CXX_WARNING_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} ")
else()
  set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${MY_CXX_WARNING_FLAGS}")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL} -Os -DNDEBUG ${MY_CXX_WARNING_FLAGS}")
  set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG ${MY_CXX_WARNING_FLGAS}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g ${MY_CXX_WARNING_FLAGS}")
endif()

if(WITH_LIBCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

if(BUILD_FUZZER)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=fuzzer-no-link")
endif()

if(APPLE)
  if(WITH_STATIC_CRT)
    file(GLOB_RECURSE libcpp_file "${LLVM_PATH}/lib/**/libc++.a")
    file(GLOB_RECURSE libcppabi_file "${LLVM_PATH}/lib/**/libc++abi.a")

    message(STATUS "Using libc++ in ${libcpp_file}" )

    file(MAKE_DIRECTORY "${GLOBAL_OUTPUT_PATH}/libcxx")
    file(COPY_FILE "${libcpp_file}" "${GLOBAL_OUTPUT_PATH}/libcxx/libc++.a")
    file(COPY_FILE "${libcppabi_file}" "${GLOBAL_OUTPUT_PATH}/libcxx/libc++abi.a")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib++ '-L${GLOBAL_OUTPUT_PATH}/libcxx' -Wl,-hidden-lc++ -Wl,-hidden-lc++abi")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -nostdlib++ '-L${GLOBAL_OUTPUT_PATH}/libcxx' -Wl,-hidden-lc++ -Wl,-hidden-lc++abi")
  endif()
endif()
