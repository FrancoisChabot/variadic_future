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

TEST(Future, pre_filled_future) {
  {
    auto fut = Future<void>::fullfilled();
    int dst = 0;
    fut.finally([&](expected<void> v) {
      if (v.has_value()) {
        dst = 1;
      }
    });

    EXPECT_EQ(1, dst);
  }

  {
    auto fut = Future<int>::fullfilled(12);

    EXPECT_EQ(12, fut.std_future().get());
  }

  {
    auto fut = Future<int, std::string>::fullfilled(12, "hi");

    EXPECT_EQ(std::make_tuple(12, "hi"), fut.std_future().get());
  }
}

TEST(Future, prom_filled_future) {
  {
    Promise<void> prom;
    auto fut = prom.get_future();
    prom.set_value();
    int dst = 0;
    fut.finally([&](expected<void> v) {
      if (v.has_value()) {
        dst = 1;
      }
    });

    EXPECT_EQ(1, dst);
  }

  {
    Promise<int> prom;
    auto fut = prom.get_future();
    prom.set_value(12);

    EXPECT_EQ(12, fut.std_future().get());
  }

  {
    Promise<int, std::string> prom;
    auto fut = prom.get_future();
    prom.set_value(12, "hi");

    EXPECT_EQ(std::make_tuple(12, "hi"), fut.std_future().get());
  }
}

TEST(Future, simple_then_expect) {
  Promise<int> p;
  auto f = p.get_future();

  auto r = f.then_expect([](expected<int> e) { return e.value() * 4; });
  p.set_value(3);

  EXPECT_EQ(r.std_future().get(), 12);
}

TEST(Future, prom_post_filled_future) {
  {
    Promise<void> prom;
    auto fut = prom.get_future();

    int dst = 0;
    fut.finally([&](expected<void> v) {
      if (v.has_value()) {
        dst = 1;
      }
    });

    prom.set_value();

    EXPECT_EQ(1, dst);
  }

  {
    Promise<int> prom;
    auto fut = prom.get_future();

    int dst = 0;
    fut.finally([&](expected<int> v) {
      if (v.has_value()) {
        dst = *v;
      }
    });
    prom.set_value(12);

    EXPECT_EQ(12, dst);
  }

  {
    Promise<int, std::string> prom;
    auto fut = prom.get_future();

    int dst = 0;
    std::string str;
    fut.finally([&](expected<int> v, expected<std::string> s) {
      if (v.has_value()) {
        dst = *v;
        str = *s;
      }
    });
    prom.set_value(12, "hi");

    EXPECT_EQ(12, dst);
    EXPECT_EQ("hi", str);
  }
}

TEST(Future, simple_then) {
  // Post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    auto res = fut.then([&](expected<int> v) { return v.value() + 4; });

    prom.set_value(3);
    EXPECT_EQ(7, res.std_future().get());
  }

  // Pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();
    prom.set_value(3);

    auto res = fut.then([&](expected<int> v) { return v.value() + 4; });

    EXPECT_EQ(7, res.std_future().get());
  }
}

TEST(Future, simple_null_then) {
  // Post-filled
  {
    Promise<void> prom;
    auto fut = prom.get_future();

    auto res = fut.then([&]() { return 4; });

    prom.set_value();
    EXPECT_EQ(4, res.std_future().get());
  }

  // Pre-filled
  {
    Promise<void> prom;
    auto fut = prom.get_future();
    prom.set_value();

    auto res = fut.then([&]() { return 4; });

    EXPECT_EQ(4, res.std_future().get());
  }
}

TEST(Future, simple_null_then_exptec) {
  // Post-filled
  {
    Promise<void> prom;
    auto fut = prom.get_future();

    auto res = fut.then_expect([&](expected<void>) { return 4; });

    prom.set_value();
    EXPECT_EQ(4, res.std_future().get());
  }

  // Pre-filled
  {
    Promise<void> prom;
    auto fut = prom.get_future();
    prom.set_value();

    auto res = fut.then_expect([&](expected<void>) { return 4; });

    EXPECT_EQ(4, res.std_future().get());
  }
}

TEST(Future, simple_then_failure) {
  // Post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    auto res = fut.then([&](expected<int> v) { return v.value() + 4; });

    prom.set_exception(std::make_exception_ptr(std::runtime_error("nope")));
    EXPECT_THROW(res.std_future().get(), std::runtime_error);
  }

  // Pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();
    prom.set_exception(std::make_exception_ptr(std::runtime_error("nope")));

    auto res = fut.then([&](expected<int> v) { return v.value() + 4; });

    EXPECT_THROW(res.std_future().get(), std::runtime_error);
  }
}

TEST(Future, Forgotten_promise) {
  Future<int> fut;
  {
    Promise<int> prom;
    fut = prom.get_future();
  }

  EXPECT_THROW(fut.std_future().get(), Unfullfilled_promise);
}

TEST(Future, simple_get) {
  Promise<int> prom;
  auto fut = prom.get_future();

  prom.set_value(3);
  EXPECT_EQ(3, fut.std_future().get());
}

TEST(Future, simple_ite) {
  Promise<int> p_a;
  Promise<std::string> p_b;

  auto f = tie(p_a.get_future(), p_b.get_future()).then([](int a, std::string) {
    return a;
  });
  p_a.set_value(3);
  p_b.set_value("yo");

  EXPECT_EQ(3, f.std_future().get());
}

TEST(Future, partial_tie_failure) {
  Promise<int> p_a;
  Promise<std::string> p_b;

  int dst = 0;
  tie(p_a.get_future(), p_b.get_future())
      .finally([&](expected<int> a, expected<std::string> b) {
        dst = a.value();
        EXPECT_FALSE(b.has_value());
      });
  EXPECT_EQ(0, dst);
  p_a.set_value(3);
  EXPECT_EQ(0, dst);
  p_b.set_exception(std::make_exception_ptr(std::runtime_error("nope")));

  EXPECT_EQ(3, dst);
}

TEST(Future, handler_returning_future) {
  Promise<int> p;
  auto f = p.get_future();

  auto f2 = f.then([](int x) {
    Promise<int> p;
    auto f = p.get_future();

    p.set_value(x);
    return f;
  });

  p.set_value(3);

  EXPECT_EQ(3, f2.std_future().get());
}

TEST(Future, void_promise) {
  Promise<void> prom;
  auto fut = prom.get_future();

  int dst = 0;
  fut.finally([&](expected<void> v) {
    EXPECT_TRUE(v.has_value());
    dst = 4;
  });

  prom.set_value();
  EXPECT_EQ(4, dst);
}
