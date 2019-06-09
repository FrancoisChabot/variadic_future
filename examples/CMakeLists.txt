include_directories(
  include
)

add_executable(example_asio asio/main.cpp)
target_link_libraries(example_asio var_futures Threads::Threads)