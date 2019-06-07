[![CircleCI](https://circleci.com/gh/FrancoisChabot/variadic_future.svg?style=svg)](https://circleci.com/gh/FrancoisChabot/variadic_future)
[![codecov](https://codecov.io/gh/FrancoisChabot/variadic_future/branch/master/graph/badge.svg)](https://codecov.io/gh/FrancoisChabot/variadic_future)

# Installation

Make the contents of the include directory available to your project.

# Usage

## Basic

```cpp
#include <iostream>
#include <thread>
#include <chrono>

#include "var_future/future.h"

// This can be any type that has a thread-safe push(std::function<void()>); method
Work_queue main_work_queue;

void foo() {
  aom::Promise<int> prom;
  aom::Future<int> fut = prom.get_future();

  // At some point in the future, the promise will be fullfilled.
  std::thread thread([p=std::move(prom)] mutable {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    p.set_value(12);
  });
  thread.detach();

  // When the promise completes successfully, perform this callback directly
  // in the detached thread. If the promise is failed, just propagate the failure.
  aom::Future<int> next_fut = fut.then([](int value) {
    if(value > 100) {
      throw std::runtime_error("too_large");
    }
    return value - 98;
  });

  // When the previous callback completes (with a success or not), queue the execution
  // of this callback in main_work_queue
  next_fut.then_finally_expect([](aom::expected<int> v, main_work_queue) {
    if(v.has_value()) {
      std::cout << *v << "\n";
    }
    else {
      std::cout << "something went wrong\n";
    }
  });

  // Notice that the futures are deleted long before the promise is fullfilled.
  // That's ok, the callbacks are still pending and valid.
}
```

## Tieing futures

You can wait on multiple futures at the same time using the 

```cpp
#include <iostream>
#include <thread>
#include <chrono>

#include "var_future/future.h"

Work_queue main_work_queue;
void foo() {
  aom::Promise<int> prom_a;
  aom::Promise<int> prom_b;

  //... Launch something that eventually fullfills the promises.

  aom::Future<int, int> combined = tie(prom_a.get_future(), prom_b.get_future());

  // This callback will execute in whichever thread completes the last promise.
  aom::Future<int> final = combined.then([](int a, int b)) {
    return a * b;
  });

  // Queue in main_work_queue
  final.then_finally_expect([](aom::expected<int> result){
    std::cout << result << "\n";
  }, main_work_queue);
}
```