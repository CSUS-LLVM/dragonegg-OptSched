# Check if the host compiler is new enough. LLVM requires at least GCC 4.8,
# MSVC 2013, or Clang 3.1.

include(CheckCXXSourceCompiles)

if(NOT DEFINED LLVM_COMPILER_CHECKED)
  set(LLVM_COMPILER_CHECKED ON)

  if(NOT LLVM_FORCE_USE_OLD_TOOLCHAIN)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      # FIXME: Change this to 4.8 once documentation builder bot is upgraded
      if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.7)
        message(FATAL_ERROR "Host GCC version must be at least 4.7!")
      endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.1)
        message(FATAL_ERROR "Host Clang version must be at least 3.1!")
      endif()

      if (CMAKE_CXX_SIMULATE_ID MATCHES "MSVC")
        if (CMAKE_CXX_SIMULATE_VERSION VERSION_LESS 18.0)
          message(FATAL_ERROR "Host Clang must have at least -fms-compatibility-version=18.0")
        endif()
        set(CLANG_CL 1)
      elseif(NOT LLVM_ENABLE_LIBCXX)
        # Otherwise, test that we aren't using too old of a version of libstdc++
        # with the Clang compiler. This is tricky as there is no real way to
        # check the version of libstdc++ directly. Instead we test for a known
        # bug in libstdc++4.6 that is fixed in libstdc++4.7.
        set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
        set(OLD_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
        set(CMAKE_REQUIRED_FLAGS "-std=c++0x")
        check_cxx_source_compiles("
#include <atomic>
std::atomic<float> x(0.0f);
int main() { return (float)x; }"
          LLVM_NO_OLD_LIBSTDCXX)
        if(NOT LLVM_NO_OLD_LIBSTDCXX)
          # FIXME: Change this to 4.8 once documentation builder bot is upgraded
          message(FATAL_ERROR "Host Clang must be able to find libstdc++4.7 or newer!")
        endif()
        set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
        set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQUIRED_LIBRARIES})
      endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
      if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 18.0)
        message(FATAL_ERROR "Host Visual Studio must be at least 2013")
      elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 18.0.31101)
        message(WARNING "Host Visual Studio should at least be 2013 Update 4 (MSVC 18.0.31101)"
          "  due to miscompiles from earlier versions")
      endif()
    endif()
  endif()
endif()
