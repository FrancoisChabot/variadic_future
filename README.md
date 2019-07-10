[![CircleCI](https://circleci.com/gh/FrancoisChabot/variadic_future.svg?style=svg)](https://circleci.com/gh/FrancoisChabot/variadic_future)
[![Build status](https://ci.appveyor.com/api/projects/status/b7ppx6xmmor89h4q/branch/master?svg=true)](https://ci.appveyor.com/project/FrancoisChabot/variadic-future/branch/master)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/862b964980034316abf5d3d02c9ee63e)](https://www.codacy.com/app/FrancoisChabot/variadic_future?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=FrancoisChabot/variadic_future&amp;utm_campaign=Badge_Grade)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/FrancoisChabot/variadic_future.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/FrancoisChabot/variadic_future/alerts/)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/FrancoisChabot/variadic_future.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/FrancoisChabot/variadic_future/context:cpp)
[![Documentation](https://img.shields.io/badge/docs-doxygen-blue.svg)](https://francoischabot.github.io/variadic_future/annotated.html)
# Variadic futures

High-performance variadic completion-based futures for C++17.

## Why?

In short, I need this to properly implement [another project](https://github.com/FrancoisChabot/easy_grpc) of mine, and it was an interesting exercise.

More specifically, completion-based futures are a non-blocking, callback-based, synchronization mechanism that hides the callback logic from the asynchronous code, while properly handling error conditions. 

## But why variadic?

Because it allows for the `join()` function, which provides a very nice way to asynchronously wait on multiple futures at once:

```cpp
Future<void> foo() {
  Future<int> fut_a = ...;
  Future<bool> fut_b = ...;
 
  Future<int, bool> combined_fut = join(fut_a, fut_b);
 
  Future<void> result = combined_fut.then([](int a, bool b) {
    // This is called once both fut_a and fut_b have been successfully completed.
    std::cout << a << " - " << b;
  });
  
  // If either fut_a or fut_b fails, the result will contain that failure.
  return result;
}
```

## Documentation

You can find the auto-generated API reference [here](https://francoischabot.github.io/variadic_future/annotated.html).

## Installation

* Make the contents of the include directory available to your project.
* Have a look at `var_future/config.h` and make changes as needed.
* If you are from the future, you may want to use `std::expected` instead of `expected_lite`,

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
  Future<void> f1;

  auto f11 = f1.then([](){});
  f1.finally([](expected<void>){});


  Future<int, float> f2;

  auto f22 = f2.then([](int, float){});
  f.finally([](expected<int>, expected<int>){});


  Future<void, int> f3;

  auto f33 = f3.then([](int){});
  f3.finally([](expected<void>, expected<int>){});
```

* The **return value** of a chaining callback will become a future of the matching type. 
  * If your callback returns a `Future<T>`, then the result is a `Future<T>` that propagates directly.
  * If your callback returns an `expected<T>`, then the result is a `Future<T>` that gets set or failed appropriately. 
  * if your callback returns `aom::segmented(T, U, ...)`, then the result is a `Future<T, U, ...>`
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

Future<int, int> w = get_value_eventually().then([](int x){ return aom::segmented(x+x, x*x); });
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

For **deferred** mode, you need to pass your queue (or an adapter) as the first parameter to the method. The queue only needs to be some type that implements `void push(T&&)` where `T` is a `Callable<void()>`.

```cpp

struct Queue {
  void push(std::function<void()> cb);
};

void foo(Queue& queue) {
  get_value_eventually()
    .then([](int v){ return v * v;})             
    .finally(queue, [](expected<int> v) {
      if(v.has_value()) {
        std::cerr << "final value: " << v << "\n";
      }
    });
}
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
```

#### async

`async()` will post the passed operation to the queue, and return a future to the value returned by that function.

```cpp
aom::Future<double> fut = aom::async(queue, [](){return 12.0;})
```

#### Joining futures

You can wait on multiple futures at the same time using the `join()` function.

```cpp

#include "var_future/future.h"

void foo() {
  aom::Future<int> fut_a = ...;
  aom::Future<int> fut_b = ...;

  aom::Future<int, int> combined = join(fut_a, fut_b);

  combined.finally([](aom::expected<int> a, aom::expected<int> b){
    //Do something with a and/or b;
  });
}
```

#### Posting callbacks to an ASIO context.

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

int int_generating_operation();

void foo() {
  asio::io_context io_ctx;
  Work_queue asio_adapter{io_ctx};

  // Queue the operation in the asio context, and get a future to the result.
  aom::Future<int> fut = aom::async(asio_adapter, int_generating_operation);

  // push the execution of this callback in io_context when ready.
  fut.finally(asio_adapter, [](aom::expected<int> v) {
    //Do something with v;
  });
}
```

### Future Streams

#### Producing Future streams
```cpp
aom::Stream_future<int> get_stream() {
   aom::Stream_promise<int> prom;
   auto result = prom.get_future();
   
   std::thread worker([p = std::move(prom)]() mutable {
     p.push(1);
     p.push(2);
     p.push(3);
     p.push(4);
     
     // If p is destroyed, the stream is implicitely failed.
     p.complete();
   });

   worker.detach();

   return result;
}
```

#### Consuming Future streams
```cpp
 auto all_done = get_stream().for_each([](int v) {
   std::cout << v << "\n";
 }).then([](){
   std::cout << "all done!\n";
 });
 
 all_done.get();
```

## Performance notes

The library assumes that, more often than not, a callback is attached to the
future before a value or error is, produced and is tuned this way. Everything
will still work if the value is produced before the callback arrives, but 
perhaps not as fast as possible.

The library also assumes that it is much more likely that a future will be 
fullfilled successfully rather than failed.

## FAQs

**Is there a std::shared_future<> equivalent?**

Not yet. If someone would use it, it can be added to the library, we just don't want to add features that would not be used anywhere.

**Why is there no terminating+error propagating method?**

We have to admit that it would be nice to just do `fut.finally([](int a, float b){ ... })`, but the problem with that is that errors would have nowhere to go. Having the path of least resistance leading to dropping errors on the ground by default is just a recipe for disaster in the long run.
