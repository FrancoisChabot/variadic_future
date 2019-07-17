[![CircleCI](https://circleci.com/gh/FrancoisChabot/variadic_future.svg?style=svg)](https://circleci.com/gh/FrancoisChabot/variadic_future)
[![Build status](https://ci.appveyor.com/api/projects/status/b7ppx6xmmor89h4q/branch/master?svg=true)](https://ci.appveyor.com/project/FrancoisChabot/variadic-future/branch/master)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/862b964980034316abf5d3d02c9ee63e)](https://www.codacy.com/app/FrancoisChabot/variadic_future?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=FrancoisChabot/variadic_future&amp;utm_campaign=Badge_Grade)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/FrancoisChabot/variadic_future.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/FrancoisChabot/variadic_future/alerts/)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/FrancoisChabot/variadic_future.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/FrancoisChabot/variadic_future/context:cpp)
[![Documentation](https://img.shields.io/badge/docs-doxygen-blue.svg)](https://francoischabot.github.io/variadic_future/annotated.html)
# Variadic futures

High-performance variadic completion-based futures for C++17.

* No external dependency
* Header-only
* Lockless

## Why?

This was needed to properly implement [Easy gRPC](https://github.com/FrancoisChabot/easy_grpc), and it was an interesting exercise.

More specifically, completion-based futures are a non-blocking, callback-based, synchronization mechanism that hides the callback logic from the asynchronous code, while properly handling error conditions. 

## What

A fairly common pattern is to have some long operation perform a callback upon its completion. At first glance, this seems pretty straightforward:

```cpp
void do_something(int x, int y, std::function<int> on_complete);

void foo() {
  do_something(1, 12, [](int val) {
    std::cout << val << "\n";
  });
}
```

However, there's a few hidden complexities at play here. The code code within `do_something()` has to make decisions about what to do with `on_complete`. Should `on_complete` be called inline or put in a work pool? Can we accept a default constructed `on_complete`? What should we do with error conditions? The path of least resistance led us to writing code with no error handling whatsoever...

With Futures, these decisions are delegated to the *caller* of `do_something()`, which prevents `do_something()` from having to know much about the context within which it is operating. Error handling is also not optional, so you will never have an error dropped on the floor.

```cpp
Future<int> do_something(int x, int y);

void foo() {
  do_something(1, 12).finally([](expected<int> val) {
    if(val.has_value()) {
      std::cout << val << "\n";
    }
  });
```

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

I am assuming you are already familiar with the [expected<>](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0323r7.html) concept/syntax. `aom::expected<T>` is simply a `std::expected<T, std::exception_ptr>`. 

### Consuming futures

Let's say that you are using a function that happens to return a `Future<...>`, and you want to execute a callback when the values becomes available:

```cpp
Future<int, float> get_value_eventually();
```

The `Future<int, float>` will **eventually** be **fullfilled** with an `int` and a `float` or **failed** with one or more `std::exception_ptr`, up to one per field.

The simplest thing you can do is call `finally()` on it. This will register a callback that will be invoked when both  values are available or failed:

```cpp
auto f = get_value_eventually();

f.finally([](expected<int> v, expected<float> f) { 
  if(v.has_value() && f.has_value()) {
    std::cout << "values are " << *v << " and " << *f << "\n"; 
  }
 });
```

Alternatively, if you want to create a future that is **completed** once the callback has **completed**, you can use `then_expect()`.

Like `finally()`, `then_expect()` invokes its callback when all values are either fullfilled or failed. However, this time, the return value of the callback is used to populate a `Future<T>` (even if the callback returns `void`). If the callback happens to throw an exception (like invoking `value()` on an `expected` containing an error), then that exception becomes the result's failure.

Rules:

- if the callback returns a `Future<T>`, that produces a `Future<T>`.
- if the callback returns a `expected<T>`, that produces a `Future<T>`.
- if the callback returns `segmented(T, U)`, that produces a `Future<T, U>`.
- Otherwise if the callback returns `T`, that produces a `Future<T>`
  
```cpp
auto f = get_value_eventually();

Future<float> result = f.then_expect([](expected<int> v, expected<float> f) {
  // Reminder: expected::value() throws an exception if it contains an error.
  return f.value() * v.value(); 
 });
```

Finally, this pattern of propagating a future's failure as the failure of its callback's result is so common that a third method does that all at once: `then()`.

Here, if `f` contains one or more **failures**, then the callback is never invoked at all, and the first error is immediately propagated as the `result`'s failure.

The same return value rules as `then_expect()` apply.

```cpp
auto f = get_value_eventually();

Future<float> result = f.then([](int v, float f) {
  return f * v; 
 });
```

In short:

|                | error-handling          | error-propagating |
|----------------|-------------------------|-------------------|
| **chains**     | `then_expect()`         | `then()`          |
| **terminates** | `finally()`             | N/A               |


#### Void fields

If a callback attached to `then_expect()` or `then()` returns `void`, that produces a `Future<void>`.

`Future<void>::then()` has special handling of void fields: They are ommited entirely from the callback arguments:

```cpp
Future<void> f_a;
Future<void, int> f_b;
Future<float, void, int> f_c;

f_a.then([](){});
f_b.then([](int v){});
f_c.then([](float f, int v){});
```

#### The Executor

The callback can either

1. Be executed directly wherever the future is fullfilled (**immediate**)
2. Be posted to a work pool to be executed by some worker (**deffered**)

**immediate** mode is used by default, just pass your callback to your chosen method and you are done.

N.B. If the future is already fullfilled by the time a callback is attached in **immediate** mode, the callback will be invoked in the thread attaching the callback as the callback is being attached.

For **deferred** mode, you need to pass your queue (or an adapter) as the first parameter to the method. The queue only needs to be some type that implements `void push(T&&)` where `T` is a `Callable<void()>`.

```cpp

struct Queue {
  // In almost all cases, this needs to be thread-safe.
  void push(std::function<void()> cb);
};

void foo(Queue& queue) {
  get_value_eventually()
    .then([](int v){ return v * v;})             
    .finally(queue, [](expected<int> v) {
      if(v.has_value()) {
        std::cerr << "final value: " << *v << "\n";
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

#### get_std_future()

`Future<>` provides `get_std_future()`, as well as `get()`, which is the exact same as `get_std_future().get()` as a convenience for explicit synchronization. 

This was added primarily to simplify writing unit tests, and using it extensively in other contexts is probably a bit of a code smell. If you find yourself that a lot, then perhaps you should just be using `std::future<>` directly instead.

```cpp
Future<int> f1 = ...;
std::future<int> x_f = f1.get_std_future();

Future<int> f2 = ...;
int x = f2.get();
```

### Future Streams

**Warning:** The stream API and performance are not nearly as mature and tested as `Future<>`/`Promise<>`.

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
future before a value or error is produced, and is tuned this way. Everything
will still work if the value is produced before the callback arrives, but 
perhaps not as fast as possible.

The library also assumes that it is much more likely that a future will be 
fullfilled successfully rather than failed.

## FAQs

**Is there a std::shared_future<> equivalent?**

Not yet. If someone would use it, it can be added to the library, we just don't want to add features that would not be used anywhere.

**Why is there no terminating+error propagating method?**

We have to admit that it would be nice to just do `fut.finally([](int a, float b){ ... })`, but the problem with that is that errors would have nowhere to go. Having the path of least resistance leading to dropping errors on the ground by default is just a recipe for disaster in the long run.
