[![CircleCI](https://circleci.com/gh/FrancoisChabot/variadic_future.svg?style=svg)](https://circleci.com/gh/FrancoisChabot/variadic_future)
[![codecov](https://codecov.io/gh/FrancoisChabot/variadic_future/branch/master/graph/badge.svg)](https://codecov.io/gh/FrancoisChabot/variadic_future)

# Variadic futures

Variadic, completion-based futures for C++17.

## Why?

In short, I need this to properly implement [another project](https://github.com/FrancoisChabot/easy_grpc) of mine, and it was an interesting exercise.

More specifically, completion-based futures are a non-blocking, callback-based, synchronization mechanism that hides the callback logic from the asynchronous code, while properly handling error conditions. 

## But why variadic?

Because it allows for the `tie()` function, which provides a very nice way to asynchronously wait on multiple futures at once:

```
Future<void> foo() {
  Future<int> fut_a = ...;
  Future<bool> fut_b = ...;
 
  Future<int, bool> combined_fut = tie(fut_a, fut_b);
 
  Future<void> result = combined_fut.then([](int a, bool b) {
    // This is called once both fut_a and fut_b have been successfully completed.
    std::cout << a << " - " << b;
  });
  
  // If either fut_a or fut_b fails, the result will contain that failure.
  return result;
}
```

## Installation

* Make the contents of the include directory available to your project.
* Have a look at `var_future/config.h` and make changes as needed.
* If you are from the future, you may want to use `std::expected` instead of `expected_lite`,

## Documentation

The main header `var_future/future.h` is meant to contain all of the user-facing interface, and nothing but the user-facing interface. It serves directly as the main documentation (for now).

## Usage
### Prerequisites

I am assuming you are already familiar with the [expected<>](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0323r7.html) concept/syntax.

### Consuming futures


Let's say that you are using a function that happens to return a `Future<...>`, and you want to execute a callback when the value becomes available:

```cpp
Future<int> get_value_eventually();
```

#### The Method

Which method to use depends on two factors:

1. Is your operation part of a future chain, or is it terminating?
1. Do you want to handle failures yourself, or let them propagate automatically?

You can use the following matrix to determine which method to use:

|                | error-handling          | error-propagating |
|----------------|-------------------------|-------------------|
| **chains**     | `then_expect()`         | `then()`          |
| **terminates** | `finally()`             | N/A               |

#### The Callback

The **arguments** of a callback for a `Future<T, U, V>` will be:

* In error-handling mode: `cb(expected<T>, expected<U>, expected<V>`)
* In error-propagating mode: `cb(T, U, V)` where void arguments are ommited.

```cpp
  Future<void, int, void, float> f1;
  Future<void, int, void, float> f2;
  Future<void, int, void, float> f3;

  auto f11 = f1.then([](int, float){});
  auto f22 = f2.then_expect([](expected<void>, expected<int>, expected<void>, float){});
  f3.finally([](expected<void>, expected<int>, expected<void>, float){});
```

* The **return value** of a chaining callback will become a future of the matching type. If your callback returns a future, then that future is propagated directly. 
* The **return value** of a terminating callback is ignored.

```cpp
Future<int> get_value_eventually();

void sync_proc(int v);
int sync_op(int v);
Future<int> async_op(int v);

Future<void> x = get_value_eventually().then(sync_proc);
get_value_eventually().finally(sync_proc);

Future<int> y = get_value_eventually().then(sync_op);
Future<int> z = get_value_eventually().then(async_op);
```

If a chaining callback throws an exception. That exception becomes the error associated with its result future. **Terminating callbacks must not throw exceptions.**

#### The Executor

The callback can either

1. Be executed directly wherever the future is fullfilled (**immediate**)
2. Be posted to a work pool to be executed by some worker (**deffered**)

**immediate** mode is used by default, just pass your callback to your chosen method and you are done.

<aside class="notice">
N.B. If the future is already fullfilled by the time a callback is attached in <strong>immediate</strong> mode, the callback will be invoked in the thread attaching the callback as the callback is being attached.
</aside>

For **deferred** mode, you need to pass your queue (or an adapter) as the first parameter to the method.

```cpp
get_value_eventually()
  .then([](int v){ return v * v;})             
  .finally(queue, [](expected<int> v) {
    if(v.has_value()) {
      std::cerr << "final value: " << v << "\n";
    }
  });
```

### Producing futures

Futures can be created by `Future::then()` or `Future::then_expect()`, but the chain has to start somewhere.

#### Promises

`Promise<Ts...>` is a lightweight interface you can use to create a future that will eventually be fullfilled (or failed).

```cpp
Promise<int> prom;
Future<int> fut = prom.get_future();

std::thread thread([p=std::move(prom)](){ 
  p.set_value(3); 
});
thread.detach();
```

#### async

`async()` will post the passed operation to the queue, and return a future to the value returned by that function.

```cpp
std::future<double> fut = aom::async(queue, [](){return 12.0;})
```


#### Tieing futures

You can wait on multiple futures at the same time using the `tie()` function.

```cpp

#include "var_future/future.h"

void foo() {
  aom::Future<int> fut_a = ...;
  aom::Future<int> fut_b = ...;

  aom::Future<int, int> combined = tie(fut_a, fut_b);

  combined.finally([](aom::expected<int> a, aom::expected<int> b){
    //Do something with a and/or b;
  });
}
```


## Posting callbacks to an ASIO context.

This example shows how to use [ASIO](https://think-async.com/Asio/), but the same idea can be applied to other contexts easily.

```cpp
#include "asio.hpp"
#include "var_future/future.h"

// This can be any type that has a thread-safe push(Callable<void()>); method
struct Work_queue {
  template<typename T>
  void push(T&& cb) {
    asio::post(ctx_, std::forward<T>(cb));
  }

  asio::io_context& ctx_;
};

void foo() {
  asio::io_context io_ctx;
  Work_queue asio_adapter{io_ctx};

  aom::Promise<int> prom;
  aom::Future<int> fut = prom.get_future();

  // push the execution of this callback in io_context when ready.
  fut.finally([](aom::expected<int> v) {
    //Do something with v;
  }, asio_adapter);
}
```

## Reference

### Glossary

* **Fullfilled** every member of the future has a value assigned to it.
* **Finished** every member of the future has either a value or an error assigned to it.
  * A future finished with nothing but values is considered both **fullfilled** and **finished**
  * A future finished with nothing but errors is considered both **failed** and **finished**
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

**Future::then_expect()**

Chaining callback.
```
[1] Future<U> Future<Ts...>::then_expect(cb);
[2] Future<U> Future<Ts...>::then_expect(cb, queue);
```

* `cb` is a `Callable<U(expected<Ts>...)>`
* [1] `cb` is immediately called when `this` is **finished**.
* [2] `cb(...)` is pushed in queue when `this` is **finished**.
* The result Future will eventually contain either:
  * The value returned by `cb`
  * The exception thrown by `cb`

**Future::finally()**

Simple callback.
```
[1] Future<U> Future<Ts...>::finally(cb);
[2] Future<U> Future<Ts...>::finally(cb, queue);
```
* `cb` is a `Callable<U(expected<Ts>...)>`
* [1] `cb` is immediately called when `this` is **finished**.
* [2] `cb(...)` is pushed in queue when `this` is **finished**.

**Future::std_future()**

Bridge to `std` api.
```
[1] std::future<T> Future<T>::std_future();
[2] std::future<std::tuple<see_below>> Future<Ts...>::std_future();
```

* Returns a std::future<> that completes upon Future **finishing**
* if `sizeof...(Ts) > 1`, the resulting tuple will have the types of the non-void `Ts`.

**tie()**

Wait on multiple futures.
```
[1] Future<T,U,...> tie(Future<T>, Future<U>, ...);
```
* Returns a Future that is **finished** when all passed futures are **finished**
