// Copyright 2019 Age of Minds inc.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include "var_future/stream_future.h"

#include "doctest.h"

#include <queue>
#include <random>

using namespace aom;

TEST_CASE("Stream of futures") {
SUBCASE("ignored_promise") {
  Stream_promise<int> prom;

  (void)prom;
}

SUBCASE("ignored_future") {
  Stream_future<int> prom;

  (void)prom;
}

SUBCASE("forgotten_promise") {
  Stream_promise<int> prom;

  auto fut = prom.get_future();

  { auto killer = std::move(prom); }

  auto all_done = fut.for_each([](int) {});

  REQUIRE_THROWS_AS(all_done.get(), Unfullfilled_promise);
}

SUBCASE("forgotten_promise_post_bind") {
  Stream_promise<int> prom;

  auto fut = prom.get_future();
  auto all_done = fut.for_each([](int) {});

  { auto killer = std::move(prom); }

  REQUIRE_THROWS_AS(all_done.get(), Unfullfilled_promise);
}

SUBCASE("forgotten_promise_async") {
  Stream_promise<int> prom;

  auto fut = prom.get_future();
  auto all_done = fut.for_each([](int) {});

  std::thread worker([p = std::move(prom)]() {
    std::this_thread::sleep_for(
        std::chrono::duration<double, std::milli>(10.0));
  });

  REQUIRE_THROWS_AS(all_done.get(), Unfullfilled_promise);

  worker.join();
}

SUBCASE("simple_stream") {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  int total = 0;

  fut.for_each([&](int v) { total += v; }).finally([&](expected<void>) {
    total = -1;
  });

  REQUIRE_EQ(total, 0);

  prom.push(1);
  REQUIRE_EQ(total, 1);

  prom.push(2);
  REQUIRE_EQ(total, 3);

  prom.push(3);
  REQUIRE_EQ(total, 6);

  prom.complete();
  REQUIRE_EQ(total, -1);
}

SUBCASE("no_data_completed_stream") {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  int total = 0;

  auto done = fut.for_each([&](int v) { total += v; });

  prom.complete();
  done.get();

  REQUIRE_EQ(total, 0);
}

SUBCASE("no_data_failed_stream") {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  int total = 0;

  auto done = fut.for_each([&](int v) { total += v; });

  prom.set_exception(std::make_exception_ptr(std::runtime_error("")));

  REQUIRE_THROWS_AS(done.get(), std::runtime_error);

  REQUIRE_EQ(total, 0);
}

SUBCASE("pre_fill_failure") {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  prom.push(1);
  prom.push(1);
  prom.set_exception(std::make_exception_ptr(std::runtime_error("")));

  int total = 0;
  auto done = fut.for_each([&](int v) { total += v; });

  REQUIRE_EQ(total, 2);
  REQUIRE_THROWS_AS(done.get(), std::runtime_error);
}

SUBCASE("partially_failed_stream") {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  int total = 0;
  prom.push(1);
  auto done = fut.for_each([&](int v) { total += v; });

  prom.push(1);
  prom.push(2);
  prom.set_exception(std::make_exception_ptr(std::runtime_error("")));

  REQUIRE_THROWS_AS(done.get(), std::runtime_error);

  REQUIRE_EQ(total, 4);
}

SUBCASE("string_stream") {
  Stream_promise<std::string> prom;
  auto fut = prom.get_future();

  prom.push("");
  prom.push("");
  prom.push("");

  int total = 0;
  auto done = fut.for_each([&](std::string) -> void { total += 1; });
  prom.push("");
  prom.push("");
  prom.push("");
  REQUIRE_EQ(total, 6);

  prom.complete();
  done.get();
}

SUBCASE("dynamic_mem_stream") {
  Stream_promise<std::unique_ptr<int>> prom;
  auto fut = prom.get_future();

  prom.push(std::make_unique<int>(1));
  prom.push(std::make_unique<int>(1));
  prom.push(std::make_unique<int>(1));

  int total = 0;
  auto done = fut.for_each([&](std::unique_ptr<int> v) { total += *v; });

  prom.push(std::make_unique<int>(1));
  prom.push(std::make_unique<int>(1));
  prom.push(std::make_unique<int>(1));
  REQUIRE_EQ(total, 6);

  prom.complete();
  done.get();
}

SUBCASE("dynamic_mem_dropped") {
  Stream_promise<std::unique_ptr<int>> prom;
  auto fut = prom.get_future();

  prom.push(std::make_unique<int>(1));
  prom.push(std::make_unique<int>(1));
  prom.push(std::make_unique<int>(1));

  int total = 0;
  auto done = fut.for_each([&](auto v) { total += *v; });
  prom.push(std::make_unique<int>(1));
  prom.push(std::make_unique<int>(1));
  prom.push(std::make_unique<int>(1));
  REQUIRE_EQ(total, 6);

  { auto killer = std::move(prom); }

  REQUIRE_THROWS_AS(done.get(), Unfullfilled_promise);
}

SUBCASE("multiple_pre_filled") {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  prom.push(1);
  prom.push(2);
  int total = 0;
  auto done = fut.for_each([&](int v) { total += v; });

  REQUIRE_EQ(total, 3);

  prom.complete();
  done.get();
}

SUBCASE("uncompleted_stream") {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  int total = 0;

  auto done = fut.for_each([&](int v) { total += v; });

  {
    Stream_promise<int> destroyer = std::move(prom);
    REQUIRE_EQ(total, 0);
    destroyer.push(1);
    REQUIRE_EQ(total, 1);
    destroyer.push(2);
    REQUIRE_EQ(total, 3);
  }

  REQUIRE_THROWS_AS(done.get(), Unfullfilled_promise);
}

SUBCASE("mt_random_timing") {
  std::random_device rd;
  std::mt19937 e2(rd());
  std::uniform_real_distribution<> dist(0, 0.002);

  Stream_promise<int> prom;
  auto fut = prom.get_future();

  std::thread worker([&]() {
    for (int i = 0; i < 10000; ++i) {
      std::this_thread::sleep_for(
          std::chrono::duration<double, std::milli>(dist(e2)));
      prom.push(1);
    }
    prom.complete();
  });

  int total = 0;

  auto done = fut.for_each([&](int v) { total += v; });

  done.get();
  REQUIRE_EQ(total, 10000);
  worker.join();
}

SUBCASE("delayed_assignment") {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  int total = 0;

  {
    Stream_promise<int> destroyer = std::move(prom);
    REQUIRE_EQ(total, 0);
    destroyer.push(1);
    REQUIRE_EQ(total, 0);
    destroyer.push(2);
    REQUIRE_EQ(total, 0);
    destroyer.complete();
    REQUIRE_EQ(total, 0);
  }

  auto done = fut.for_each([&](int v) { total += v; });

  done.get();
  REQUIRE_EQ(total, 3);
}

SUBCASE("stream_to_queue") {
  std::queue<std::function<void()>> queue;

  Stream_promise<int> prom;
  auto fut = prom.get_future();
  int total = 0;
  bool all_done = false;

  fut.for_each(queue, [&](int v) { total += v; }).finally([&](expected<void>) {
    all_done = true;
  });

  prom.push(1);
  prom.push(1);
  prom.push(1);
  prom.complete();

  REQUIRE_EQ(0, total);
  REQUIRE_EQ(queue.size(), 4);
  REQUIRE_FALSE(all_done);

  while (!queue.empty()) {
    queue.front()();
    queue.pop();
  }

  REQUIRE_EQ(3, total);
  REQUIRE(all_done);
}

SUBCASE("stream_to_queue_alt") {
  std::queue<std::function<void()>> queue;

  Stream_promise<int> prom;
  auto fut = prom.get_future();
  int total = 0;
  bool all_done = false;

  prom.push(1);
  prom.push(1);
  prom.push(1);

  REQUIRE_EQ(queue.size(), 0);

  fut.for_each(queue, [&](int v) { total += v; }).finally([&](expected<void>) {
    all_done = true;
  });

  REQUIRE_EQ(queue.size(), 3);

  prom.complete();

  REQUIRE_EQ(0, total);
  REQUIRE_EQ(queue.size(), 4);
  REQUIRE_FALSE(all_done);

  while (!queue.empty()) {
    queue.front()();
    queue.pop();
  }

  REQUIRE_EQ(3, total);
  REQUIRE(all_done);
}

struct Synced_queue {
  void push(std::function<void()> p) {
    std::lock_guard l(mtx_);
    queue.push(std::move(p));
  }

  bool pop() {
    std::lock_guard l(mtx_);
    if (queue.empty()) {
      return true;
    }
    queue.front()();
    queue.pop();
    return false;
  }

  std::queue<std::function<void()>> queue;
  std::mutex mtx_;
};

SUBCASE("stream_to_queue_random_timing") {
  std::random_device rd;
  std::mt19937 e2(rd());
  std::uniform_real_distribution<> dist(0, 0.002);

  Synced_queue queue;

  Stream_promise<int> prom;
  auto fut = prom.get_future();

  int total = 0;
  bool all_done = false;

  std::thread pusher([&]() {
    for (int i = 0; i < 10000; ++i) {
      std::this_thread::sleep_for(
          std::chrono::duration<double, std::milli>(dist(e2)));
      prom.push(1);
    }
    prom.complete();
  });

  std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(50));
  fut.for_each(queue, [&](int v) { total += v; }).finally([&](expected<void>) {
    all_done = true;
  });

  std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(50));
  while (!queue.pop()) {
  }

  pusher.join();
  while (!queue.pop()) {
  }

  REQUIRE_EQ(10000, total);
  REQUIRE(all_done);
}
}