[![CircleCI](https://circleci.com/gh/FrancoisChabot/variadic_future.svg?style=svg)](https://circleci.com/gh/FrancoisChabot/variadic_future)
[![codecov](https://codecov.io/gh/FrancoisChabot/variadic_future/branch/master/graph/badge.svg)](https://codecov.io/gh/FrancoisChabot/variadic_future)

# Variadic futures

Variadic, callback-based futures for C++17.

## Why?

In short, I need this to properly implement [another project](https://github.com/FrancoisChabot/easy_grpc) of mine, and it was an interesting exercise.

## Installation

* Make the contents of the include directory available to your project.
* Have a look at `var_future/config.h` and make changes as needed.
* If you are from the future, you may want to use `std::expected` instead of `expected_lite`,

## Documentation

The main header `var_future/future.h` is meant to contain all of the user-facing interface, and nothing but the user-facing interface. It serves directly as the main documentation (for now).

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


### Tieing futures

You can wait on multiple futures at the same time using the `tie()` function.

```cpp

#include "var_future/future.h"

void foo() {
  aom::Promise<int> prom_a;
  aom::Promise<int> prom_b;

  //... Launch something that eventually fullfills the promises.

  aom::Future<int, int> combined = tie(prom_a.get_future(), prom_b.get_future());

  combined.then_finally_expect([](aom::expected<int> a, aom::expected<int> b){
    //Do something with a and/or b;
  });
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

  // push the execution of this callback in main_work_queue when ready.
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

## Reference

### Glossary

* **Fullfilled** every member of the future has a value assigned to it.
* **Finished** every member of the future has either a value or an error assigned to it.
  * A future finished with nothing but values is considered **fullfilled**
* **Failed** every member of the future has an error assigned to it

### Methods

**Future::then()**

Chaining callback with failure propagation.
```
[1] Future<U> Future<Ts...>::then(cb);
[2] Future<U> Future<Ts...>::then(cb, queue);
```
* `cb` is a `Callable<U(Ts...)>`
* [1] `cb` is immediately called when `this` is **fullfilled**.
* [2] `cb(...)` is pushed in queue when `this` is **fullfilled**.
* If `this` is failed, `cb` is NOT called
* If `sizeof...(Ts)>1` any failure causes the complete failure of the future.  
* The result Future will eventually contain either:
  * The value returned by `cb`
  * The error that caused `this` to fail
  * The exception thrown by `cb`

**Future::then_except()**

Chaining callback.
```
[1] Future<U> Future<Ts...>::then_except(cb);
[2] Future<U> Future<Ts...>::then_except(cb, queue);
```

* `cb` is a `Callable<U(expected<Ts>...)>`
* [1] `cb` is immediately called when `this` is **finished**.
* [2] `cb(...)` is pushed in queue when `this` is **finished**.
* The result Future will eventually contain either:
  * The value returned by `cb`
  * The exception thrown by `cb`

**Future::then_finally_except()**

Simple callback.
```
[1] Future<U> Future<Ts...>::then_finally_except(cb);
[2] Future<U> Future<Ts...>::then_finally_except(cb, queue);
```
* `cb` is a `Callable<U(expected<Ts>...)>`
* [1] `cb` is immediately called when `this` is **finished**.
* [2] `cb(...)` is pushed in queue when `this` is **finished**.

**Future::get_std_future()**

Bridge to `std` api.
```
[1] std::future<T> Future<T>::get_std_future();
[2] std::future<std::tuple<see_below>> Future<Ts...>::get_std_future();
```

* Returns a std::future<> that completes upon Future **finishing**
* if `sizeof...(Ts) > 1`, the resulting tuple will have the types of the non-void `Ts`.

**tie()**

Wait on multiple futures.
```
[1] Future<T,U,...> tie(Future<T>, Future<U>, ...);
```
* Returns a Future that is **finished** when all passed futures are **finished**
