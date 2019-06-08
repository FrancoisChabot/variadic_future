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

namespace {

  struct prom_fut {
    prom_fut() {
      f = p.get_future();
    }

    void get() {
      f.get_std_future().get();
    }

    Promise<void> p;
    Future<void> f;
  };

  struct pf_set {
    prom_fut pf[4];

    prom_fut& operator[](std::size_t i) {return pf[i];}

    void complete() {
      pf[0].p.set_value();
      pf[1].p.set_exception(std::make_exception_ptr(std::logic_error("nope")));
      pf[2].p.finish(expected<void>());
      pf[3].p.finish(unexpected(std::make_exception_ptr(std::logic_error(""))));
    }
  };

  int expect_noop_count = 0;
  void expected_noop(expected<void>) {++expect_noop_count;}
  void expected_noop_fail(expected<void>) {
    throw std::runtime_error("dead");
  }

  void no_op() {}

  void failure() {
    throw std::runtime_error("dead");
  }
}

TEST(Future_void, blank) {
  Future<void> fut;
}

TEST(Future_void, unfilled_promise_failiure) {
  Future<void> fut;
  {
    Promise<void> p;
    fut = p.get_future();
  }

  EXPECT_THROW(fut.get_std_future().get(), Unfullfilled_promise);
}

TEST(Future_void, preloaded_std_get) {
  pf_set pf;

  pf.complete();

  EXPECT_NO_THROW(  pf[0].get());
  EXPECT_THROW(     pf[1].get(), std::logic_error);
  EXPECT_NO_THROW(  pf[2].get());
  EXPECT_THROW(     pf[3].get(), std::logic_error);
}

TEST(Future_void, delayed_std_get) {
  pf_set pf;

  std::mutex mtx;
  std::unique_lock l(mtx);
  std::thread thread([&] {
    std::lock_guard ll(mtx);
    pf.complete();
  });

  auto std_f1 = pf[0].f.get_std_future();
  auto std_f2 = pf[1].f.get_std_future();
  auto std_f3 = pf[2].f.get_std_future();
  auto std_f4 = pf[3].f.get_std_future();
  
  l.unlock();
  
  EXPECT_NO_THROW(std_f1.get());
  EXPECT_THROW(std_f2.get(), std::logic_error);
  EXPECT_NO_THROW(std_f3.get());
  EXPECT_THROW(std_f4.get(), std::logic_error);

  thread.join();
}

TEST(Future_void, then_noop_pre) {
  pf_set pf;

  auto f1 = pf[0].f.then(no_op);
  auto f2 = pf[1].f.then(no_op);
  auto f3 = pf[2].f.then(no_op);
  auto f4 = pf[3].f.then(no_op);

  pf.complete();
  
  EXPECT_NO_THROW(f1.get_std_future().get());
  EXPECT_THROW(f2.get_std_future().get(), std::logic_error);
  EXPECT_NO_THROW(f3.get_std_future().get());
  EXPECT_THROW(f4.get_std_future().get(), std::logic_error);
}

TEST(Future_void, then_noop_post) {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(no_op);
  auto f2 = pf[1].f.then(no_op);
  auto f3 = pf[2].f.then(no_op);
  auto f4 = pf[3].f.then(no_op);
  
  EXPECT_NO_THROW(f1.get_std_future().get());
  EXPECT_THROW(f2.get_std_future().get(), std::logic_error);
  EXPECT_NO_THROW(f3.get_std_future().get());
  EXPECT_THROW(f4.get_std_future().get(), std::logic_error);
}

TEST(Future_void, then_failure_pre) {
  pf_set pf;

  auto f1 = pf[0].f.then(failure);
  auto f2 = pf[1].f.then(failure);
  auto f3 = pf[2].f.then(failure);
  auto f4 = pf[3].f.then(failure);
  
  pf.complete();

  EXPECT_THROW(f1.get_std_future().get(), std::runtime_error);
  EXPECT_THROW(f2.get_std_future().get(), std::logic_error);
  EXPECT_THROW(f3.get_std_future().get(), std::runtime_error);
  EXPECT_THROW(f4.get_std_future().get(), std::logic_error);
}

TEST(Future_void, then_failure_post) {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(failure);
  auto f2 = pf[1].f.then(failure);
  auto f3 = pf[2].f.then(failure);
  auto f4 = pf[3].f.then(failure);

  EXPECT_THROW(f1.get_std_future().get(), std::runtime_error);
  EXPECT_THROW(f2.get_std_future().get(), std::logic_error);
  EXPECT_THROW(f3.get_std_future().get(), std::runtime_error);
  EXPECT_THROW(f4.get_std_future().get(), std::logic_error);
}

TEST(Future_void, then_expect_success_pre) {
  pf_set pf;

  auto f1 = pf[0].f.then_expect(expected_noop);
  auto f2 = pf[1].f.then_expect(expected_noop);
  auto f3 = pf[2].f.then_expect(expected_noop);
  auto f4 = pf[3].f.then_expect(expected_noop);
  
  pf.complete();

  EXPECT_NO_THROW(f1.get_std_future().get());
  EXPECT_NO_THROW(f2.get_std_future().get());
  EXPECT_NO_THROW(f3.get_std_future().get());
  EXPECT_NO_THROW(f4.get_std_future().get());

}

TEST(Future_void, then_expect_success_post) {
  pf_set pf;

  auto f1 = pf[0].f.then_expect(expected_noop);
  auto f2 = pf[1].f.then_expect(expected_noop);
  auto f3 = pf[2].f.then_expect(expected_noop);
  auto f4 = pf[3].f.then_expect(expected_noop);
  
  pf.complete();

  EXPECT_NO_THROW(f1.get_std_future().get());
  EXPECT_NO_THROW(f2.get_std_future().get());
  EXPECT_NO_THROW(f3.get_std_future().get());
  EXPECT_NO_THROW(f4.get_std_future().get());
}

TEST(Future_void, then_expect_failure_pre) {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then_expect(expected_noop_fail);
  auto f2 = pf[1].f.then_expect(expected_noop_fail);
  auto f3 = pf[2].f.then_expect(expected_noop_fail);
  auto f4 = pf[3].f.then_expect(expected_noop_fail);

  EXPECT_THROW(f1.get_std_future().get(), std::runtime_error);
  EXPECT_THROW(f2.get_std_future().get(), std::runtime_error);
  EXPECT_THROW(f3.get_std_future().get(), std::runtime_error);
  EXPECT_THROW(f4.get_std_future().get(), std::runtime_error);
}

TEST(Future_void, then_expect_failure_post) {
  pf_set pf;

  auto f1 = pf[0].f.then_expect(expected_noop_fail);
  auto f2 = pf[1].f.then_expect(expected_noop_fail);
  auto f3 = pf[2].f.then_expect(expected_noop_fail);
  auto f4 = pf[3].f.then_expect(expected_noop_fail);

  pf.complete();

  EXPECT_THROW(f1.get_std_future().get(), std::runtime_error);
  EXPECT_THROW(f2.get_std_future().get(), std::runtime_error);
  EXPECT_THROW(f3.get_std_future().get(), std::runtime_error);
  EXPECT_THROW(f4.get_std_future().get(), std::runtime_error);
}


TEST(Future_void, then_expect_finally_success_pre) {
  pf_set pf;

  auto ref = expect_noop_count;
  
  pf[0].f.then_finally_expect(expected_noop);
  pf[1].f.then_finally_expect(expected_noop);
  pf[2].f.then_finally_expect(expected_noop);
  pf[3].f.then_finally_expect(expected_noop);

  pf.complete();

  EXPECT_EQ(4+ref, expect_noop_count);
}

TEST(Future_void, then_expect_finally_success_post) {
  pf_set pf;

  auto ref = expect_noop_count;

  pf.complete();

  pf[0].f.then_finally_expect(expected_noop);
  pf[1].f.then_finally_expect(expected_noop);
  pf[2].f.then_finally_expect(expected_noop);
  pf[3].f.then_finally_expect(expected_noop);

  EXPECT_EQ(4+ref, expect_noop_count);
}
