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

#include "doctest.h"

#include <queue>

using namespace aom;

TEST_CASE("misc futures tests") {
SUBCASE("ignored_promise") {
  Promise<int> prom;

  (void)prom;
}

SUBCASE("prom_filled_future") {
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

    REQUIRE_EQ(1, dst);
  }

  {
    Promise<int> prom;
    auto fut = prom.get_future();
    prom.set_value(12);

    REQUIRE_EQ(12, fut.std_future().get());
  }

  {
    Promise<int, std::string> prom;
    auto fut = prom.get_future();
    prom.set_value(12, "hi");

    REQUIRE_EQ(std::make_tuple(12, "hi"), fut.std_future().get());
  }
}

SUBCASE("simple_then_expect") {
  Promise<int> p;
  auto f = p.get_future();

  auto r = f.then_expect([](expected<int> e) { return e.value() * 4; });
  p.set_value(3);

  REQUIRE_EQ(r.std_future().get(), 12);
}

SUBCASE("prom_post_filled_future") {
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

    REQUIRE_EQ(1, dst);
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

    REQUIRE_EQ(12, dst);
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

    REQUIRE_EQ(12, dst);
    REQUIRE_EQ("hi", str);
  }
}

SUBCASE("simple_then") {
  // Post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    auto res = fut.then([&](expected<int> v) { return v.value() + 4; });

    prom.set_value(3);
    REQUIRE_EQ(7, res.std_future().get());
  }

  // Pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();
    prom.set_value(3);

    auto res = fut.then([&](expected<int> v) { return v.value() + 4; });

    REQUIRE_EQ(7, res.std_future().get());
  }
}

SUBCASE("simple_null_then") {
  // Post-filled
  {
    Promise<void> prom;
    auto fut = prom.get_future();

    auto res = fut.then([&]() { return 4; });

    prom.set_value();
    REQUIRE_EQ(4, res.std_future().get());
  }

  // Pre-filled
  {
    Promise<void> prom;
    auto fut = prom.get_future();
    prom.set_value();

    auto res = fut.then([&]() { return 4; });

    REQUIRE_EQ(4, res.std_future().get());
  }
}

SUBCASE("simple_null_then_exptec") {
  // Post-filled
  {
    Promise<void> prom;
    auto fut = prom.get_future();

    auto res = fut.then_expect([&](expected<void>) { return 4; });

    prom.set_value();
    REQUIRE_EQ(4, res.std_future().get());
  }

  // Pre-filled
  {
    Promise<void> prom;
    auto fut = prom.get_future();
    prom.set_value();

    auto res = fut.then_expect([&](expected<void>) { return 4; });

    REQUIRE_EQ(4, res.std_future().get());
  }
}

SUBCASE("simple_then_failure") {
  // Post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    auto res = fut.then([&](expected<int> v) { return v.value() + 4; });

    prom.set_exception(std::make_exception_ptr(std::runtime_error("nope")));
    REQUIRE_THROWS_AS(res.std_future().get(), std::runtime_error);
  }

  // Pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();
    prom.set_exception(std::make_exception_ptr(std::runtime_error("nope")));

    auto res = fut.then([&](expected<int> v) { return v.value() + 4; });

    REQUIRE_THROWS_AS(res.std_future().get(), std::runtime_error);
  }
}

SUBCASE("Forgotten_promise") {
  Future<int> fut;
  {
    Promise<int> prom;
    fut = prom.get_future();
  }

  REQUIRE_THROWS_AS(fut.std_future().get(), Unfullfilled_promise);
}

SUBCASE("simple_get") {
  Promise<int> prom;
  auto fut = prom.get_future();

  prom.set_value(3);
  REQUIRE_EQ(3, fut.std_future().get());
}

SUBCASE("simple_join") {
  Promise<int> p_a;
  Promise<std::string> p_b;

  auto f =
      join(p_a.get_future(), p_b.get_future()).then([](int a, std::string) {
        return a;
      });
  p_a.set_value(3);
  p_b.set_value("yo");

  REQUIRE_EQ(3, f.std_future().get());
}

SUBCASE("partial_join_failure") {
  Promise<int> p_a;
  Promise<std::string> p_b;

  int dst = 0;
  join(p_a.get_future(), p_b.get_future())
      .finally([&](expected<int> a, expected<std::string> b) {
        dst = a.value();
        REQUIRE_FALSE(b.has_value());
      });
  REQUIRE_EQ(0, dst);
  p_a.set_value(3);
  REQUIRE_EQ(0, dst);
  p_b.set_exception(std::make_exception_ptr(std::runtime_error("nope")));

  REQUIRE_EQ(3, dst);
}

SUBCASE("handler_returning_future") {
  Promise<int> p;
  auto f = p.get_future();

  auto f2 = f.then([](int x) {
    Promise<int> p;
    auto f = p.get_future();

    p.set_value(x);
    return f;
  });

  p.set_value(3);

  REQUIRE_EQ(3, f2.std_future().get());
}

SUBCASE("void_promise") {
  Promise<void> prom;
  auto fut = prom.get_future();

  int dst = 0;
  fut.finally([&](expected<void> v) {
    REQUIRE(v.has_value());
    dst = 4;
  });

  prom.set_value();
  REQUIRE_EQ(4, dst);
}

SUBCASE("variadic_get") {
  static_assert(std::is_same_v<Future<void>::value_type, void>);
  static_assert(std::is_same_v<Future<int>::value_type, int>);
  static_assert(std::is_same_v<Future<void, void>::value_type, void>);
  static_assert(
      std::is_same_v<Future<int, int>::value_type, std::tuple<int, int>>);
  static_assert(std::is_same_v<Future<int, void>::value_type, int>);
  static_assert(std::is_same_v<Future<void, int>::value_type, int>);
  static_assert(std::is_same_v<Future<void, int, void>::value_type, int>);
  static_assert(
      std::is_same_v<Future<int, void, int>::value_type, std::tuple<int, int>>);
}

SUBCASE("variadic_get_failure") {
  Promise<void, void> p;
  auto f = p.get_future();

  p.set_exception(std::make_exception_ptr(std::runtime_error("dead")));

  REQUIRE_THROWS_AS(f.get(), std::runtime_error);
}

SUBCASE("segmented_callback") {
  Promise<void> p;

  auto f = p.get_future()
               .then([]() { return segmented(12, 12); })
               .then([](int a, int b) { return a + b; });

  p.set_value();

  REQUIRE_EQ(24, f.get());
}

SUBCASE("defered_returned_future") {
  Promise<int> p;

  auto f = p.get_future().then([](int) {
    Promise<int> final_p;

    auto result = final_p.get_future();
    std::thread w(
        [final_p = std::move(final_p)]() mutable { final_p.set_value(15); });
    w.detach();
    return result;
  });

  p.set_value(1);
  REQUIRE_EQ(f.get(), 15);
}
}