cmake_minimum_required(VERSION 3.10)

project(easy_grpc VERSION 0.0.1)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

SET(COVERAGE OFF CACHE BOOL "Coverage")


set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
if(CMAKE_COMPILER_IS_GNUCXX)
    include(CodeCoverage)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
    set(COVERAGE_LCOV_EXCLUDES 
      '/usr/*'
    )
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

include(CTest)

include(GoogleTest)
find_package(GTest REQUIRED CONFIG)

add_library(var_futures INTERFACE)
target_include_directories(var_futures INTERFACE . include)
target_compile_features(var_futures INTERFACE cxx_std_17)

add_executable(futures_test 
  tests/int.cpp
  tests/misc.cpp
  tests/void.cpp)

if(MSVC)
  target_compile_options(futures_test PUBLIC /W4 /WX)
else()
  target_compile_options(futures_test PUBLIC -Wall -Wextra -pedantic -Werror)
endif()

target_link_libraries(futures_test var_futures GTest::gtest_main GTest::gtest --coverage)


include(ProcessorCount)
ProcessorCount(PROCESSOR_COUNT)

if(CMAKE_COMPILER_IS_GNUCXX)
  setup_target_for_coverage_lcov(
    NAME coverage
    EXECUTABLE ctest -j ${PROCESSOR_COUNT}
    DEPENDENCIES futures_test)
endif()

gtest_discover_tests(futures_test)

add_subdirectory(examples)