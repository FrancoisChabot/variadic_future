#include "var_future/future.h"
#include "asio.hpp"

#include <iostream>

// A simple aom::Future queue that posts the passed call to an 
// asio service.
struct Asio_bridge {
  template<typename T>
  void push(T&& cb) {
    asio::post(ctx, std::forward<T>(cb));
  }

  asio::io_context& ctx;
};

int main() {
  asio::io_context io_ctx;
  Asio_bridge bridge{io_ctx};

  aom::Promise<int> promise;
  auto result = promise.get_future();

  // Passing the bridge to the future handler methods will cause 
  // the callback to be posted in the asio context.
  result.then_finally_expect([](aom::expected<int> v) {
    std::cout << v.value() << "\n";
  }, bridge);

  
  promise.set_value(14);

  io_ctx.run();

  return 0;
}