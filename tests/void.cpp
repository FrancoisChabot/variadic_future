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

namespace {

struct prom_fut {
  prom_fut() { f = p.get_future(); }

  void get() { f.std_future().get(); }

  Promise<void> p;
  Future<void> f;
};

struct pf_set {
  prom_fut pf[4];

  prom_fut& operator[](std::size_t i) { return pf[i]; }

  void complete() {
    pf[0].p.set_value();
    pf[1].p.set_exception(std::make_exception_ptr(std::logic_error("nope")));
    pf[2].p.finish(expected<void>());
    pf[3].p.finish(
        aom::unexpected(std::make_exception_ptr(std::logic_error(""))));
  }
};

int expect_noop_count = 0;
void expected_noop(expected<void>) { ++expect_noop_count; }
void expected_noop_fail(expected<void>) { throw std::runtime_error("dead"); }

void no_op() {}

void failure() { throw std::runtime_error("dead"); }

int return_int() { return 1; }

int return_int_fail() { throw std::runtime_error(""); }


expected<void> expected_cb() { return {}; }
expected<void> expected_cb_fail() {
  return aom::unexpected{std::make_exception_ptr(std::runtime_error("yikes"))};
}
}  // namespace

TEST_CASE("future pf void") {
SUBCASE("fundamental_expectations") {
  // These tests failing do not mean that the library doesn't work.
  // It's just that some architecture-related assumptions made are not being
  // met, so performance might be sub-optimal.

  // The special immediate queue type qualifies as having static push.
  REQUIRE(detail::has_static_push_v<detail::Immediate_queue>);

  // Base handler should be nothing but a vtable pointer.
  REQUIRE_EQ(sizeof(void*),
            sizeof(detail::Future_handler_base<detail::Immediate_queue, void>));
}

SUBCASE("blank") { Future<void> fut; }

SUBCASE("unfilled_promise_failiure") {
  Future<void> fut;
  {
    Promise<void> p;
    fut = p.get_future();
  }

  REQUIRE_THROWS_AS(fut.std_future().get(), Unfullfilled_promise);
}

SUBCASE("preloaded_std_get") {
  pf_set pf;

  pf.complete();

  REQUIRE_NOTHROW(pf[0].get());
  REQUIRE_THROWS_AS(pf[1].get(), std::logic_error);
  REQUIRE_NOTHROW(pf[2].get());
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

  REQUIRE_NOTHROW(std_f1.get());
  REQUIRE_THROWS_AS(std_f2.get(), std::logic_error);
  REQUIRE_NOTHROW(std_f3.get());
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

  REQUIRE_NOTHROW(f1.std_future().get());
  REQUIRE_NOTHROW(f2.std_future().get());
  REQUIRE_NOTHROW(f3.std_future().get());
  REQUIRE_NOTHROW(f4.std_future().get());
}

SUBCASE("then_expect_success_post") {
  pf_set pf;

  auto f1 = pf[0].f.then_expect(expected_noop);
  auto f2 = pf[1].f.then_expect(expected_noop);
  auto f3 = pf[2].f.then_expect(expected_noop);
  auto f4 = pf[3].f.then_expect(expected_noop);

  pf.complete();

  REQUIRE_NOTHROW(f1.std_future().get());
  REQUIRE_NOTHROW(f2.std_future().get());
  REQUIRE_NOTHROW(f3.std_future().get());
  REQUIRE_NOTHROW(f4.std_future().get());
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

SUBCASE("chain_to_int") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(return_int);
  auto f2 = pf[1].f.then(return_int);
  auto f3 = pf[2].f.then(return_int);
  auto f4 = pf[3].f.then(return_int);

  REQUIRE_EQ(1, f1.std_future().get());
  REQUIRE_THROWS_AS(f2.std_future().get(), std::logic_error);
  REQUIRE_EQ(1, f3.std_future().get());
  REQUIRE_THROWS_AS(f4.std_future().get(), std::logic_error);
}

SUBCASE("chain_to_int_fail") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(return_int_fail);
  auto f2 = pf[1].f.then(return_int_fail);
  auto f3 = pf[2].f.then(return_int_fail);
  auto f4 = pf[3].f.then(return_int_fail);

  REQUIRE_THROWS_AS(f1.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f2.std_future().get(), std::logic_error);
  REQUIRE_THROWS_AS(f3.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f4.std_future().get(), std::logic_error);
}

SUBCASE("expected_returning_callback") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(expected_cb);
  auto f2 = pf[1].f.then(expected_cb);
  auto f3 = pf[2].f.then(expected_cb);
  auto f4 = pf[3].f.then(expected_cb);

  REQUIRE_NOTHROW(f1.std_future().get());
  REQUIRE_THROWS_AS(f2.std_future().get(), std::logic_error);
  REQUIRE_NOTHROW(f3.std_future().get());
  REQUIRE_THROWS_AS(f4.std_future().get(), std::logic_error);
}

SUBCASE("expected_returning_callback_fail") {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(expected_cb_fail);
  auto f2 = pf[1].f.then(expected_cb_fail);
  auto f3 = pf[2].f.then(expected_cb_fail);
  auto f4 = pf[3].f.then(expected_cb_fail);

  REQUIRE_THROWS_AS(f1.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f2.std_future().get(), std::logic_error);
  REQUIRE_THROWS_AS(f3.std_future().get(), std::runtime_error);
  REQUIRE_THROWS_AS(f4.std_future().get(), std::logic_error);
}
}