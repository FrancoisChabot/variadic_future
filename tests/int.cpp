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
#include <random>

using namespace aom;

namespace {

struct prom_fut {
  prom_fut() { f = p.get_future(); }

  int get() { return f.std_future().get(); }

  Promise<int> p;
  Future<int> f;
};

struct pf_set {
  prom_fut pf[4];

  prom_fut& operator[](std::size_t i) { return pf[i]; }

  void complete() {
    pf[0].p.set_value(1);
    pf[1].p.set_exception(std::make_exception_ptr(std::logic_error("nope")));
    pf[2].p.finish(expected<int>(1));
    pf[3].p.finish(
        aom::unexpected(std::make_exception_ptr(std::logic_error(""))));
  }
};

void no_op(int i) { REQUIRE_EQ(i, 1); }

void failure(int i) {
  REQUIRE_EQ(i, 1);
  throw std::runtime_error("dead");
}

int expect_noop_count = 0;
int expected_noop(expected<int>) {
  ++expect_noop_count;
  return 1;
}

void expected_noop_fail(expected<int>) { throw std::runtime_error("dead"); }


expected<int> generate_expected_value(int) { return 3; }

expected<int> generate_expected_value_fail(int) {
  return aom::unexpected{std::make_exception_ptr(std::runtime_error("yo"))};
}

expected<int> generate_expected_value_throw(int) {
  throw std::runtime_error("yo");
}


expected<int> te_generate_expected_value(expected<int>) { return 3; }

expected<int> te_generate_expected_value_fail(expected<int>) {
  return aom::unexpected{std::make_exception_ptr(std::runtime_error("yo"))};
}

expected<int> te_generate_expected_value_throw(expected<int>) {
  throw std::runtime_error("yo");
}

}  // namespace


TEST_CASE("Future of int") {


SUBCASE("blank") { Future<int> fut; }

SUBCASE("unfilled_promise_failiure") {
  Future<int> fut;
  {
    Promise<int> p;
    fut = p.get_future();
  }

  REQUIRE_THROWS_AS(fut.std_future().get(), Unfullfilled_promise);
}

SUBCASE("preloaded_std_get") {
  pf_set pf;

  pf.complete();

  REQUIRE_EQ(1, pf[0].get());
  REQUIRE_THROWS_AS(pf[1].get(), std::logic_error);
  REQUIRE_EQ(1, pf[2].get());
  REQUIRE_THROWS_AS(pf[3].get(), std::logic_error);
}

SUBCASE("delayed_std_get") {
  pf_set pf;

  std::mutex mtx;
  std::unique_lock l(mtx);
  std::thread thread([&] {
    std::lock_guard ll(mtx);
    pf.complete();
  });

  auto std_f1 = pf[0].f.std_future();
  auto std_f2 = pf[1].f.std_future();
  auto std_f3 = pf[2].f.std_future();
  auto std_f4 = pf[3].f.std_future();

  l.unlock();

  REQUIRE_EQ(1, std_f1.get());
  REQUIRE_THROWS_AS(std_f2.get(), std::logic_error);
  REQUIRE_EQ(1, std_f3.get());
  REQUIRE_THROWS_AS(std_f4.get(), std::logic_error);

  thread.join();
}

SUBCASE("then_noop_pre") {
  pf_set pf;

  auto f1 = pf[0].f.then(no_op);
  auto f2 = pf[1].f.then(no_op);
  auto f3 = pf[2].f.then(no_op);
  auto f4 = pf[3].f.then(no_op);

  pf.complete();

  REQUIRE_NOTHROW(f1.std_future().get());
  REQUIRE_THROWS_AS(f2.std_future().get(), std::logic_error);
  REQUIRE_NOTHROW(f3.std_future().get());
  REQUIRE_THROWS_AS(f4.std_future().get(), std::logic_error);
}

SUBCASE("then_noop_post") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(no_op);
  auto f2 = pf[1].f.then(no_op);
  auto f3 = pf[2].f.then(no_op);
  auto f4 = pf[3].f.then(no_op);

  REQUIRE_NOTHROW(f1.std_future().get());
  REQUIRE_THROWS_AS(f2.std_future().get(), std::logic_error);
  REQUIRE_NOTHROW(f3.std_future().get());
  REQUIRE_THROWS_AS(f4.std_future().get(), std::logic_error);
}

SUBCASE("then_failure_pre") {
  pf_set pf;

  auto f1 = pf[0].f.then(failure);
  auto f2 = pf[1].f.then(failure);
  auto f3 = pf[2].f.then(failure);
  auto f4 = pf[3].f.then(failure);

  pf.complete();

  REQUIRE_THROWS_AS(f1.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f2.std_future().get(), std::logic_error);
  REQUIRE_THROWS_AS(f3.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f4.std_future().get(), std::logic_error);
}

SUBCASE("then_failure_post") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(failure);
  auto f2 = pf[1].f.then(failure);
  auto f3 = pf[2].f.then(failure);
  auto f4 = pf[3].f.then(failure);

  REQUIRE_THROWS_AS(f1.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f2.std_future().get(), std::logic_error);
  REQUIRE_THROWS_AS(f3.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f4.std_future().get(), std::logic_error);
}

SUBCASE("then_expect_success_pre") {
  pf_set pf;

  auto f1 = pf[0].f.then_expect(expected_noop);
  auto f2 = pf[1].f.then_expect(expected_noop);
  auto f3 = pf[2].f.then_expect(expected_noop);
  auto f4 = pf[3].f.then_expect(expected_noop);

  pf.complete();

  REQUIRE_EQ(1, f1.std_future().get());
  REQUIRE_EQ(1, f2.std_future().get());
  REQUIRE_EQ(1, f3.std_future().get());
  REQUIRE_EQ(1, f4.std_future().get());
}

SUBCASE("then_expect_success_post") {
  pf_set pf;

  auto f1 = pf[0].f.then_expect(expected_noop);
  auto f2 = pf[1].f.then_expect(expected_noop);
  auto f3 = pf[2].f.then_expect(expected_noop);
  auto f4 = pf[3].f.then_expect(expected_noop);

  pf.complete();

  REQUIRE_EQ(1, f1.std_future().get());
  REQUIRE_EQ(1, f2.std_future().get());
  REQUIRE_EQ(1, f3.std_future().get());
  REQUIRE_EQ(1, f4.std_future().get());
}

SUBCASE("then_expect_failure_pre") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then_expect(expected_noop_fail);
  auto f2 = pf[1].f.then_expect(expected_noop_fail);
  auto f3 = pf[2].f.then_expect(expected_noop_fail);
  auto f4 = pf[3].f.then_expect(expected_noop_fail);

  REQUIRE_THROWS_AS(f1.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f2.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f3.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f4.std_future().get(), std::runtime_error);
}

SUBCASE("then_expect_failure_post") {
  pf_set pf;

  auto f1 = pf[0].f.then_expect(expected_noop_fail);
  auto f2 = pf[1].f.then_expect(expected_noop_fail);
  auto f3 = pf[2].f.then_expect(expected_noop_fail);
  auto f4 = pf[3].f.then_expect(expected_noop_fail);

  pf.complete();

  REQUIRE_THROWS_AS(f1.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f2.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f3.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f4.std_future().get(), std::runtime_error);
}

SUBCASE("then_expect_finally_success_pre") {
  pf_set pf;

  auto ref = expect_noop_count;

  pf[0].f.finally(expected_noop);
  pf[1].f.finally(expected_noop);
  pf[2].f.finally(expected_noop);
  pf[3].f.finally(expected_noop);

  pf.complete();

  REQUIRE_EQ(4 + ref, expect_noop_count);
}

SUBCASE("then_expect_finally_success_post") {
  pf_set pf;

  auto ref = expect_noop_count;

  pf.complete();

  pf[0].f.finally(expected_noop);
  pf[1].f.finally(expected_noop);
  pf[2].f.finally(expected_noop);
  pf[3].f.finally(expected_noop);

  REQUIRE_EQ(4 + ref, expect_noop_count);
}


SUBCASE("expected_returning_callback") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(generate_expected_value);
  auto f2 = pf[1].f.then(generate_expected_value);
  auto f3 = pf[2].f.then(generate_expected_value);
  auto f4 = pf[3].f.then(generate_expected_value);

  REQUIRE_EQ(3, f1.get());
  REQUIRE_THROWS_AS(f2.std_future().get(), std::logic_error);
  REQUIRE_EQ(3, f3.get());
  REQUIRE_THROWS_AS(f4.std_future().get(), std::logic_error);
}

SUBCASE("expected_returning_callback_fail") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(generate_expected_value_fail);
  auto f2 = pf[1].f.then(generate_expected_value_fail);
  auto f3 = pf[2].f.then(generate_expected_value_fail);
  auto f4 = pf[3].f.then(generate_expected_value_fail);

  REQUIRE_THROWS_AS(f1.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f2.std_future().get(), std::logic_error);
  REQUIRE_THROWS_AS(f3.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f4.std_future().get(), std::logic_error);
}

SUBCASE("expected_returning_callback_throw") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(generate_expected_value_throw);
  auto f2 = pf[1].f.then(generate_expected_value_throw);
  auto f3 = pf[2].f.then(generate_expected_value_throw);
  auto f4 = pf[3].f.then(generate_expected_value_throw);

  REQUIRE_THROWS_AS(f1.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f2.std_future().get(), std::logic_error);
  REQUIRE_THROWS_AS(f3.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f4.std_future().get(), std::logic_error);
}


SUBCASE("te_expected_returning_callback") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then_expect(te_generate_expected_value);
  auto f2 = pf[1].f.then_expect(te_generate_expected_value);
  auto f3 = pf[2].f.then_expect(te_generate_expected_value);
  auto f4 = pf[3].f.then_expect(te_generate_expected_value);

  REQUIRE_EQ(3, f1.get());
  REQUIRE_EQ(3, f2.get());
  REQUIRE_EQ(3, f3.get());
  REQUIRE_EQ(3, f4.get());
}

SUBCASE("te_expected_returning_callback_fail") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then_expect(te_generate_expected_value_fail);
  auto f2 = pf[1].f.then_expect(te_generate_expected_value_fail);
  auto f3 = pf[2].f.then_expect(te_generate_expected_value_fail);
  auto f4 = pf[3].f.then_expect(te_generate_expected_value_fail);

  REQUIRE_THROWS_AS(f1.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f2.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f3.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f4.std_future().get(), std::runtime_error);
}

SUBCASE("te_expected_returning_callback_throw") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then_expect(te_generate_expected_value_throw);
  auto f2 = pf[1].f.then_expect(te_generate_expected_value_throw);
  auto f3 = pf[2].f.then_expect(te_generate_expected_value_throw);
  auto f4 = pf[3].f.then_expect(te_generate_expected_value_throw);

  REQUIRE_THROWS_AS(f1.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f2.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f3.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f4.std_future().get(), std::runtime_error);
}

SUBCASE("promote_tuple_to_variadic") {
  Promise<std::tuple<int, int>> p_t;
  auto f_t = p_t.get_future();

  Future<int, int> real_f = flatten(f_t);

  int a = 0;
  int b = 0;

  real_f.finally([&](expected<int> ia, expected<int> ib) {
    a = *ia;
    b = *ib;
  });

  REQUIRE_EQ(0, a);
  REQUIRE_EQ(0, b);

  p_t.set_value(std::make_tuple(2, 3));
  REQUIRE_EQ(2, a);
  REQUIRE_EQ(3, b);
}

SUBCASE("random_timing") {
  std::random_device rd;
  std::mt19937 e2(rd());

  std::uniform_real_distribution<> dist(0, 0.002);

  for (int i = 0; i < 10000; ++i) {
    Promise<int> prom;
    Future<int> fut = prom.get_future();
    std::thread writer_thread([&]() {
      std::this_thread::sleep_for(
          std::chrono::duration<double, std::milli>(dist(e2)));

      prom.set_value(12);
    });

    std::this_thread::sleep_for(
        std::chrono::duration<double, std::milli>(dist(e2)));

    REQUIRE_EQ(12, fut.get());
    writer_thread.join();
  }
}
}