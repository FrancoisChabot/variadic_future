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
#include "var_future/future.h"

#include "gtest/gtest.h"

#include <queue>

using namespace aom;

TEST(Future_int, blank) {
  Future<int> fut;
}

TEST(Future_int, unfilled_promise_failiure) {
  Future<int> fut;
  {
    Promise<int> p;
    fut = p.get_future();
  }

  EXPECT_THROW(fut.get_std_future().get(), Unfullfilled_promise);
}


TEST(Future_int, then_success) {
  {
    Promise<int> p;
    auto f = p.get_future();

    auto two = f.then([](int i){ return i+1;});
    p.set_value(1);

    EXPECT_EQ(2, two.get_std_future().get());
  }
  {
    Promise<int> p;
    auto f = p.get_future();

    p.set_value(1);
    auto two = f.then([](int i){ return i+1;});

    EXPECT_EQ(2, two.get_std_future().get());
  }
}

TEST(Future_int, then_failure) {
  {
    Promise<int> p;
    auto f = p.get_future();

    auto one = f.then([](int){ throw std::runtime_error("nope");});
    p.set_value(1);

    EXPECT_THROW(one.get_std_future().get(), std::runtime_error);
  }
  {
    Promise<int> p;
    auto f = p.get_future();

    p.set_value(1);
    auto one = f.then([](int){ throw std::runtime_error("nope");});

    EXPECT_THROW(one.get_std_future().get(), std::runtime_error);
  }
}

TEST(Future_int, then_failure_transit) {
  {
    Promise<int> p;
    auto f = p.get_future();

    auto one = f.then([](int i){ return i+1;});
    p.set_exception(std::make_exception_ptr(std::runtime_error("nope")));

    EXPECT_THROW(one.get_std_future().get(), std::runtime_error);
  }
  {
    Promise<int> p;
    auto f = p.get_future();

    p.set_exception(std::make_exception_ptr(std::runtime_error("nope")));
    auto one = f.then([](int i){ return i+1;});

    EXPECT_THROW(one.get_std_future().get(), std::runtime_error);
  }
}


TEST(Future_int, then_expect_success) {
  {
    Promise<int> p;
    auto f = p.get_future();

    auto one = f.then_expect([](expected<int> v){ return v.value() + 1;});
    p.set_value(1);

    EXPECT_EQ(2, one.get_std_future().get());
  }
  {
    Promise<int> p;
    auto f = p.get_future();

    p.set_value(1);
    auto one = f.then_expect([](expected<int> v){ return v.value() + 1;});

    EXPECT_EQ(2, one.get_std_future().get());
  }
}

TEST(Future_int, then_expect_failure) {
  {
    Promise<int> p;
    auto f = p.get_future();

    auto one = f.then_expect([](expected<int> v){ EXPECT_TRUE(v.has_value()); throw std::runtime_error("nope");});
    p.set_value(1);

    EXPECT_THROW(one.get_std_future().get(), std::runtime_error);
  }
  {
    Promise<int> p;
    auto f = p.get_future();

    p.set_value(1);
    auto one = f.then_expect([](expected<int> v){ EXPECT_TRUE(v.has_value()); throw std::runtime_error("nope");});

    EXPECT_THROW(one.get_std_future().get(), std::runtime_error);
  }
}

TEST(Future_int, then_expect_intercept) {
  {
    Promise<int> p;
    auto f = p.get_future();

    auto one = f.then_expect([](expected<int> v){ EXPECT_FALSE(v.has_value()); return 1;});
    p.set_exception(std::make_exception_ptr(std::runtime_error("nope")));

    EXPECT_EQ(1, one.get_std_future().get());
  }
  {
    Promise<int> p;
    auto f = p.get_future();

    p.set_exception(std::make_exception_ptr(std::runtime_error("nope")));
    auto one = f.then_expect([](expected<int> v){ EXPECT_FALSE(v.has_value()); return 1;});

    EXPECT_EQ(1, one.get_std_future().get());
  }
}

TEST(Future_int, then_expect_finally_success) {
  {
    Promise<int> p;
    auto f = p.get_future();

    int dst = 0;
    f.then_finally_expect([&](expected<int> v){ EXPECT_TRUE(v.has_value()); dst = *v + 1;});
    p.set_value(1);

    EXPECT_EQ(2, dst);
  }
  {
    Promise<int> p;
    auto f = p.get_future();

    p.set_value(1);
    int dst = 0;
    f.then_finally_expect([&](expected<int> v){EXPECT_TRUE(v.has_value()); dst = *v + 1;});

    EXPECT_EQ(2, dst);
  }
}

TEST(Future_int, then_expect_finally_intercept) {
  {
    Promise<int> p;
    auto f = p.get_future();
    
    int dst = 0;
    f.then_finally_expect([&](expected<int> v){ EXPECT_FALSE(v.has_value()); dst= 1;});
    p.set_exception(std::make_exception_ptr(std::runtime_error("nope")));

    EXPECT_EQ(1, dst);
  }
  {
    Promise<int> p;
    auto f = p.get_future();

    p.set_exception(std::make_exception_ptr(std::runtime_error("nope")));
    int dst = 0;
    f.then_finally_expect([&](expected<int> v){ EXPECT_FALSE(v.has_value()); dst = 1;});

    EXPECT_EQ(1, dst);
  }
}