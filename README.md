[![CircleCI](https://circleci.com/gh/FrancoisChabot/variadic_future.svg?style=svg)](https://circleci.com/gh/FrancoisChabot/variadic_future)
[![codecov](https://codecov.io/gh/FrancoisChabot/variadic_future/branch/master/graph/badge.svg)](https://codecov.io/gh/FrancoisChabot/variadic_future)

# Variadic futures

Variadic, callback-based futures for C++17.

## Installation

Make the contents of the include directory available to your project.

## Usage

### Basic

```cpp

#include "var_future/future.h"

void foo() {
  aom::Promise<int> prom;
  aom::Future<int> fut = prom.get_future();
  
  fut.then_finally_expect([](aom::expected<int> v){
    //Do something with v;
  });
  
  prom.set_value(2);
}
```

## Choosing where the callback executes

```cpp
#include <iostream>
#include <thread>
#include <chrono>

#include "var_future/future.h"

// This can be any type that has a thread-safe push(Callable<void()>); method
Work_queue main_work_queue;

void foo() {
  aom::Promise<int> prom;
  aom::Future<int> fut = prom.get_future();

  // When the previous callback completes (with a success or not), push the execution
  // of this callback in main_work_queue. 
  fut.then_finally_expect([](aom::expected<int> v, main_work_queue) {
    //Do something with v;
  });
}
```

## Chaining Futures

```cpp
#include <iostream>
#include <thread>
#include <chrono>

#include "var_future/future.h"

// This can be any type that has a thread-safe push(Callable<void()>); method
Work_queue main_work_queue;

std::Future<int> complete(std::string str) {
   aom::Promise<int> p;
   auto result = p.get_future();
   
   std::async(std::launch::async, [str, p=std::move(p)] mutable { 
      std::cout << str ;
      p.set_value(4); 
   });
   
   return result;
}

void foo() {
  aom::Promise<int> prom;
  aom::Future<int> fut = prom.get_future();

  fut.then([](int v){
    return std::string("hi") + std::to_string(v);
  })
  .then(complete)
  .then_finally_expect([](aom::expected<int> v, main_work_queue) {
    //Do something with v;
  });
}
```

### Tieing futures

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
