cmake_minimum_required(VERSION 3.10)

project(var_future VERSION 0.1.0)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(Threads REQUIRED)
include(GNUInstallDirs)

# Check if var_futures is being used directly or via add_subdirectory, but allow overriding
if (NOT DEFINED VAR_FUTURES_MASTER_PROJECT)
    if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(VAR_FUTURES_MASTER_PROJECT ON)
    else()
        set(VAR_FUTURES_MASTER_PROJECT OFF)
    endif()
endif ()

option(VAR_FUTURES_BUILD_EXAMPLES "var_futures examples" OFF)
option(VAR_FUTURES_BUILD_BENCHMARKS "var_futures benchmarks (requires google bench)" OFF)
option(VAR_FUTURES_BUILD_TESTS "var_futures tests" ON)
option(VAR_FUTURES_TEST_COVERAGE "var_futures Coverage" OFF)
option(VAR_FUTURES_INSTALL "Generate the install target" ${VAR_FUTURES_MASTER_PROJECT})

############################ code format ############################
if(${VAR_FUTURES_MASTER_PROJECT})
  if(NOT DEFINED CLANG_FORMAT_BIN)
    find_program(CLANG_FORMAT_BIN NAMES clang-format)
  endif()

  file(GLOB_RECURSE ALL_SOURCE_FILES *.cpp *.h *.cxx *.hxx *.hpp *.cc *.ipp)

  set(CLANG_FORMAT_EXCLUDE_PATTERNS ${CLANG_FORMAT_EXCLUDE_PATTERNS} 
    "/CMakeFiles/" 
    "cmake" 
    "nonstd" 
    "include/asio"
  )

  foreach (SOURCE_FILE ${ALL_SOURCE_FILES})
      foreach (EXCLUDE_PATTERN ${CLANG_FORMAT_EXCLUDE_PATTERNS})
          string(FIND ${SOURCE_FILE} ${EXCLUDE_PATTERN} EXCLUDE_FOUND)
          if (NOT ${EXCLUDE_FOUND} EQUAL -1)
              list(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
          endif ()
      endforeach ()
  endforeach ()


  add_custom_target(format
      COMMENT "Running clang-format to change files"
      COMMAND ${CLANG_FORMAT_BIN}
      -style=file
      -i
      ${ALL_SOURCE_FILES}
  )
endif()
#############################################################



if(${VAR_FUTURES_TEST_COVERAGE}) 
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
endif()


if(WIN32)
  add_definitions(-D_WIN32_WINNT=0x0601 -D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)
endif()


add_library(var_futures INTERFACE)
target_include_directories(var_futures INTERFACE 
  $<BUILD_INTERFACE:. include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(var_futures INTERFACE cxx_std_17)

if(${VAR_FUTURES_INSTALL})
  set(export_dest_dir "${CMAKE_INSTALL_LIBDIR}/spdlog/cmake")

  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(TARGETS var_futures
    EXPORT   var_futuresTargets
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

  #install cmake export
  install(EXPORT var_futuresTargets DESTINATION ${export_dest_dir})

  configure_file(cmake/var_futures-config.cmake.in
    "${PROJECT_BINARY_DIR}/var_futures-config.cmake" @ONLY)

  install(FILES "${PROJECT_BINARY_DIR}/var_futures-config.cmake" DESTINATION "${export_dest_dir}")
endif()


if(${VAR_FUTURES_BUILD_BENCHMARKS})
  add_subdirectory(benchmarks)
endif()

if(${VAR_FUTURES_BUILD_TESTS})

  include(CTest)
  include(GoogleTest)
  find_package(GTest CONFIG)
  if( NOT GTest_FOUND)
    add_subdirectory(gtest)
  endif()

  add_executable(futures_test 
    tests/allocator.cpp
    tests/async.cpp
    tests/int.cpp
    tests/join.cpp
    tests/future_of_reference.cpp
    tests/misc.cpp
    tests/stream.cpp
    tests/void.cpp
  )

  if(MSVC)
    target_compile_options(futures_test PUBLIC /W4 /WX)
  else()
    target_compile_options(futures_test PUBLIC -Wall -Wextra -pedantic -Werror -ftemplate-backtrace-limit=0 )
  endif()

  target_include_directories(futures_test PRIVATE gtest/include)
  target_link_libraries(futures_test var_futures gtest_main gtest Threads::Threads)

  include(ProcessorCount)
  ProcessorCount(PROCESSOR_COUNT)

  if(${VAR_FUTURES_TEST_COVERAGE}) 
    if(CMAKE_COMPILER_IS_GNUCXX)
      setup_target_for_coverage_lcov(
        NAME coverage
        EXECUTABLE ctest -j ${PROCESSOR_COUNT}
        DEPENDENCIES futures_test)
    endif()
  endif()

  gtest_discover_tests(futures_test)
endif()

if(${VAR_FUTURES_BUILD_EXAMPLES})
  add_subdirectory(examples)
endif()



