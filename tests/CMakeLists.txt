SET(TEST_NAMES
  allocator
  async 
  int
  join
  future_of_reference
  misc
  stream
  void
)

if(MSVC)
  SET(TEST_OPTIONS PUBLIC /W4 /WX)
else()
  SET(TEST_OPTIONS PUBLIC -Wall -Wextra -pedantic -Werror -ftemplate-backtrace-limit=0 )
endif()

add_library(doctest_main doctest_main.cpp)

foreach(TEST_NAME ${TEST_NAMES})
  SET(TEST_SRC ${TEST_NAME}.cpp)
  SET(TEST_TGT varfut_test_${TEST_NAME})

  add_executable(${TEST_TGT} ${TEST_SRC})
  target_compile_options(${TEST_TGT} PUBLIC ${TEST_OPTIONS})
  target_link_libraries(${TEST_TGT} doctest_main var_futures Threads::Threads)
  add_test(${TEST_NAME} ${TEST_TGT})
endforeach()


