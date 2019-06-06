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

#include "variadic_future.h"

#include "gtest/gtest.h"

#include <queue>

using namespace aom;

// In order to keep the coverage testing sane, we will only instantiate a few specific types
// of futures in these tests: 
// Future<>                 : The null future
// Future<int>              : POD types
// Future<std::string>      : Non-trivial types
// Future<int, std::string> : Composed types


std::exception_ptr get_error() {
  std::exception_ptr result;

  try {
    throw std::runtime_error("error");
  }
  catch(...) {
    result = std::current_exception();
  }
  return result;
}

int add_4_to_1(int v) {
  EXPECT_EQ(v, 1);
  return v + 4;
}

void finally_4(int v) {
  EXPECT_EQ(v, 4);
}

void finally_not_called(int v) {
  EXPECT_FALSE(true);
}

void finally_expect_4(expected<int> v) {
  EXPECT_TRUE(v.has_value());
  EXPECT_EQ(*v, 4);
}

void finally_expect_fail(expected<int> v) {
  EXPECT_FALSE(v.has_value());
}

int expect_add_4_to_1(expected<int> v) {
  EXPECT_TRUE(v.has_value());
  EXPECT_EQ(*v, 1);
  return *v + 4;
}

int expect_add_4_to_1_recover(expected<int> v) {
  EXPECT_FALSE(v.has_value());
  return 5;
}

int expect_add_4_to_1_failure(expected<int> v) {
  EXPECT_FALSE(v.has_value());
  throw std::runtime_error("yo");
}

TEST(Future, get_success) {
  // Before the fact
  {
    Promise<int> prom;
    auto fut = prom.get_future();
    prom.set_value(12);
    EXPECT_EQ(fut.get(), 12);
  }

  // After the fact
  {
      Promise<int> prom;
      std::mutex m;
      auto fut = prom.get_future();
      std::unique_lock lock_1(m);

      std::thread t([&](){
        std::lock_guard lock_2(m);
        prom.set_value(12);
      });

      EXPECT_EQ(fut.get_std_future(&lock_1).get(), 12);

      t.join();
  }

}

TEST(Future, get_failure) {
  // Before the fact
  {
    Promise<int> prom;
    auto fut = prom.get_future();
    prom.set_exception(get_error());
    EXPECT_THROW(fut.get(), std::runtime_error);
  }

  // After the fact
  {
      Promise<int> prom;
      std::mutex m;
      auto fut = prom.get_future();
      std::unique_lock lock_1(m);

      std::thread t([&](){
        std::lock_guard lock_2(m);
        prom.set_exception(get_error());
      });

      EXPECT_THROW(fut.get_std_future(&lock_1).get(), std::runtime_error);

      t.join();
  }

}

TEST(Future, st_success_then) {
  // pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    prom.set_value(1);
    auto result_fut = fut.then(add_4_to_1);
    EXPECT_EQ(result_fut.get(), 5);
  }


  // post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    auto result_fut = fut.then(add_4_to_1);
    prom.set_value(1);
    EXPECT_EQ(result_fut.get(), 5);
  }

}

TEST(Future, st_success_then_expect) {
  
  // pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    prom.set_value(1);
    auto result_fut = fut.then_expect(expect_add_4_to_1);
    EXPECT_EQ(result_fut.get(), 5);
  }

  // post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    auto result_fut = fut.then_expect(expect_add_4_to_1);
    prom.set_value(1);
    EXPECT_EQ(result_fut.get(), 5);
  }

}

TEST(Future, st_success_then_finally_expect) {
  // pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    prom.set_value(4);
    fut.then_finally_expect(finally_expect_4);
  }

  // post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    fut.then_finally_expect(finally_expect_4);
    prom.set_value(4);
  }

}


TEST(Future, st_failure_then) {
  
  // pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    prom.set_exception(get_error());
    auto result_fut = fut.then(add_4_to_1);
    EXPECT_THROW(result_fut.get(), std::runtime_error);
  }

  // post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    auto result_fut = fut.then(add_4_to_1);
    prom.set_exception(get_error());
    EXPECT_THROW(result_fut.get(), std::runtime_error);
  }

}

TEST(Future, st_recover_then_expect) {
  
  // pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    prom.set_exception(get_error());
    auto result_fut = fut.then_expect(expect_add_4_to_1_recover);
    EXPECT_EQ(result_fut.get(), 5);
  }

  // post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    auto result_fut = fut.then_expect(expect_add_4_to_1_recover);
    prom.set_exception(get_error());
    EXPECT_EQ(result_fut.get(), 5);
  }

}

TEST(Future, st_failure_then_expect) {
  
  // pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    prom.set_exception(get_error());
    auto result_fut = fut.then_expect(expect_add_4_to_1_failure);
    EXPECT_THROW(result_fut.get(), std::runtime_error);
  }

  // post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    auto result_fut = fut.then_expect(expect_add_4_to_1_failure);
    prom.set_exception(get_error());
    EXPECT_THROW(result_fut.get(), std::runtime_error);
  }

}

TEST(Future, st_failure_then_finally_expect) {
  
  // pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    prom.set_exception(get_error());
    fut.then_finally_expect(finally_expect_fail);
  }

  // post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    fut.then_finally_expect(finally_expect_fail);
    prom.set_exception(get_error());
  }
}


TEST(Future, chain_with_future) {
  
  Promise<int> prom;
  auto fut = prom.get_future();

  auto result = fut.then([&](int v){
    Promise<int> tmp;
    auto tmp_fut = tmp.get_future();
    tmp.set_value(4);
    return tmp_fut;
  });


  prom.set_value(12);

  EXPECT_EQ(4, result.get());
}


struct Some_struct {
  int t;
};

TEST(Future, get_user_type) {
  
  Promise<Some_struct> prom;
  auto fut = prom.get_future();

  prom.set_value({4});

  EXPECT_EQ(4, fut.get().t);
}


TEST(Future, then_in_queue) {

  std::queue<std::function<void()>> queue;

  int dst = 0;
  // pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    prom.set_value(1);
    fut.then_finally_expect([&](aom::expected<int> v){ dst += *v;}, queue);
  }

  EXPECT_EQ(1, queue.size());

  // post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    fut.then_finally_expect([&](aom::expected<int> v){ dst += *v;}, queue);
    prom.set_value(2);
  }

  EXPECT_EQ(2, queue.size());
  EXPECT_EQ(0, dst);

  while(!queue.empty()) {
    queue.front()();
    queue.pop();
  }

  EXPECT_EQ(3, dst);
}

TEST(Future, multiplex_voids) {

  Promise<> prom_a;
  Promise<> prom_b;

  auto fut = tie(prom_a.get_future(), prom_b.get_future());


  int dst = 0;
  fut.then_finally_expect([&](expected<void>a, expected<void>b){
    dst = 5;
    EXPECT_TRUE(a.has_value());
    EXPECT_TRUE(b.has_value());
  });


  EXPECT_EQ(0, dst);
  prom_a.set_value();
  
  EXPECT_EQ(0, dst);
  
  prom_b.set_value();
  EXPECT_EQ(5, dst);

}

TEST(Future, partial_completion) {

  Promise<int> prom_a;
  Promise<int> prom_b;

  auto fut = tie(prom_a.get_future(), prom_b.get_future());


  int dst = 0;
  fut.then_finally_expect([&](expected<int>a, expected<int>b){
    dst += a.value();
    EXPECT_FALSE(b.has_value());
  });


  EXPECT_EQ(0, dst);
  prom_a.set_value(4);
  
  EXPECT_EQ(0, dst);
  
  prom_b.set_exception(get_error());
  EXPECT_EQ(4, dst);

}