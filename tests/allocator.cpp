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

#include <array>
#include <queue>


using namespace aom;

namespace {
template <typename T>
struct Test_alloc {
  using value_type = T;

  template <typename U>
  struct rebind {
    using other = Test_alloc<U>;
  };

  Test_alloc(int* counter, int* total) : counter_(counter), total_(total) {}

  T* allocate(std::size_t count) {
    ++*counter_;
    ++*total_;
    return reinterpret_cast<T*>(std::malloc(count * sizeof(T)));
  }

  void deallocate(T* ptr, std::size_t) {
    std::free(ptr);
    --*counter_;
  }

  template <typename U>
  explicit Test_alloc(const Test_alloc<U>& rhs) {
    counter_ = rhs.counter_;
    total_ = rhs.total_;
  }

  template <typename U>
  Test_alloc& operator=(Test_alloc<U>& rhs) {
    counter_ = rhs.counter_;
    total_ = rhs.total_;
    return *this;
  }

  int* counter_;
  int* total_;
};

template <>
struct Test_alloc<void> {
  using value_type = void;
  
  template <typename U>
  struct rebind {
    using other = Test_alloc<U>;
  };

  Test_alloc(int* counter, int* total) : counter_(counter), total_(total) {}

  template <typename U>
  explicit Test_alloc(const Test_alloc<U>& rhs) {
    counter_ = rhs.counter_;
    total_ = rhs.total_;
  }

  template <typename U>
  Test_alloc& operator=(Test_alloc<U>& rhs) {
    counter_ = rhs.counter_;
    total_ = rhs.total_;
    return *this;
  }

  int* counter_;
  int* total_;
};

using Future_type = Basic_future<Test_alloc<void>, int>;
using Promise_type = Basic_promise<Test_alloc<void>, int>;

struct prom_fut {
  prom_fut(int* counter, int* total)
  : p(Test_alloc<void>(counter, total)) {
    f = p.get_future();
  }

  int get() { return f.std_future().get(); }

  Future_type f;
  Promise_type p;
};

struct pf_set {
  pf_set()
      : pf{prom_fut(&counter, &total), prom_fut(&counter, &total),
           prom_fut(&counter, &total), prom_fut(&counter, &total)} {}

  ~pf_set() {
    std::cerr << "Alloc count was: " << total << "\n";
    EXPECT_EQ(counter, 0);
  }

  int counter = 0;
  int total = 0;
  std::array<prom_fut, 4> pf;

  prom_fut& operator[](std::size_t i) { return pf[i]; }

  void complete() {
    pf[0].p.set_value(1);
    pf[1].p.set_exception(std::make_exception_ptr(std::logic_error("nope")));
    pf[2].p.finish(expected<int>(1));
    pf[3].p.finish(
        aom::unexpected(std::make_exception_ptr(std::logic_error(""))));
  }
};

void no_op(int i) { EXPECT_EQ(i, 1); }

void failure(int i) {
  EXPECT_EQ(i, 1);
  throw std::runtime_error("dead");
}

int expect_noop_count = 0;
int expected_noop(expected<int>) {
  ++expect_noop_count;
  return 1;
}

void expected_noop_fail(expected<int>) { throw std::runtime_error("dead"); }

}  // namespace

TEST(Future_alloc, blank) { Future_type fut; }

TEST(Future_alloc, unfilled_promise_failiure) {
  int counter = 0;
  int total = 0;

  Future_type fut;
  {
    Promise_type p(Test_alloc<void>(&counter, &total));
    fut = p.get_future();
  }

  EXPECT_THROW(fut.std_future().get(), Unfullfilled_promise);
}

TEST(Future_alloc, preloaded_std_get) {
  pf_set pf;

  pf.complete();

  EXPECT_EQ(1, pf[0].get());
  EXPECT_THROW(pf[1].get(), std::logic_error);
  EXPECT_EQ(1, pf[2].get());
  EXPECT_THROW(pf[3].get(), std::logic_error);
}

TEST(Future_alloc, delayed_std_get) {
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

  EXPECT_EQ(1, std_f1.get());
  EXPECT_THROW(std_f2.get(), std::logic_error);
  EXPECT_EQ(1, std_f3.get());
  EXPECT_THROW(std_f4.get(), std::logic_error);

  thread.join();
}

TEST(Future_alloc, then_noop_pre) {
  pf_set pf;

  auto f1 = pf[0].f.then(no_op);
  auto f2 = pf[1].f.then(no_op);
  auto f3 = pf[2].f.then(no_op);
  auto f4 = pf[3].f.then(no_op);

  pf.complete();

  EXPECT_NO_THROW(f1.std_future().get());
  EXPECT_THROW(f2.std_future().get(), std::logic_error);
  EXPECT_NO_THROW(f3.std_future().get());
  EXPECT_THROW(f4.std_future().get(), std::logic_error);
}

TEST(Future_alloc, then_noop_post) {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(no_op);
  auto f2 = pf[1].f.then(no_op);
  auto f3 = pf[2].f.then(no_op);
  auto f4 = pf[3].f.then(no_op);

  EXPECT_NO_THROW(f1.std_future().get());
  EXPECT_THROW(f2.std_future().get(), std::logic_error);
  EXPECT_NO_THROW(f3.std_future().get());
  EXPECT_THROW(f4.std_future().get(), std::logic_error);
}

TEST(Future_alloc, then_noop_pre_large_callback) {
  pf_set pf;

  struct payload_t {
    int x = 0;
    int y = 0;
    int z = 0;
  };

  payload_t payload;

  payload.x = 1;
  payload.y = 1;
  payload.z = 1;

  auto callback = [payload](int) {};

  auto f1 = pf[0].f.then(callback);
  auto f2 = pf[1].f.then(callback);
  auto f3 = pf[2].f.then(callback);
  auto f4 = pf[3].f.then(callback);

  pf.complete();

  EXPECT_NO_THROW(f1.std_future().get());
  EXPECT_THROW(f2.std_future().get(), std::logic_error);
  EXPECT_NO_THROW(f3.std_future().get());
  EXPECT_THROW(f4.std_future().get(), std::logic_error);
}

TEST(Future_alloc, then_failure_pre) {
  pf_set pf;

  auto f1 = pf[0].f.then(failure);
  auto f2 = pf[1].f.then(failure);
  auto f3 = pf[2].f.then(failure);
  auto f4 = pf[3].f.then(failure);

  pf.complete();

  EXPECT_THROW(f1.std_future().get(), std::runtime_error);
  EXPECT_THROW(f2.std_future().get(), std::logic_error);
  EXPECT_THROW(f3.std_future().get(), std::runtime_error);
  EXPECT_THROW(f4.std_future().get(), std::logic_error);
}

TEST(Future_alloc, then_failure_post) {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(failure);
  auto f2 = pf[1].f.then(failure);
  auto f3 = pf[2].f.then(failure);
  auto f4 = pf[3].f.then(failure);

  EXPECT_THROW(f1.std_future().get(), std::runtime_error);
  EXPECT_THROW(f2.std_future().get(), std::logic_error);
  EXPECT_THROW(f3.std_future().get(), std::runtime_error);
  EXPECT_THROW(f4.std_future().get(), std::logic_error);
}

TEST(Future_alloc, then_expect_success_pre) {
  pf_set pf;

  auto f1 = pf[0].f.then_expect(expected_noop);
  auto f2 = pf[1].f.then_expect(expected_noop);
  auto f3 = pf[2].f.then_expect(expected_noop);
  auto f4 = pf[3].f.then_expect(expected_noop);

  pf.complete();

  EXPECT_EQ(1, f1.std_future().get());
  EXPECT_EQ(1, f2.std_future().get());
  EXPECT_EQ(1, f3.std_future().get());
  EXPECT_EQ(1, f4.std_future().get());
}

TEST(Future_alloc, then_expect_success_post) {
  pf_set pf;

  auto f1 = pf[0].f.then_expect(expected_noop);
  auto f2 = pf[1].f.then_expect(expected_noop);
  auto f3 = pf[2].f.then_expect(expected_noop);
  auto f4 = pf[3].f.then_expect(expected_noop);

  pf.complete();

  EXPECT_EQ(1, f1.std_future().get());
  EXPECT_EQ(1, f2.std_future().get());
  EXPECT_EQ(1, f3.std_future().get());
  EXPECT_EQ(1, f4.std_future().get());
}

TEST(Future_alloc, then_expect_failure_pre) {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then_expect(expected_noop_fail);
  auto f2 = pf[1].f.then_expect(expected_noop_fail);
  auto f3 = pf[2].f.then_expect(expected_noop_fail);
  auto f4 = pf[3].f.then_expect(expected_noop_fail);

  EXPECT_THROW(f1.std_future().get(), std::runtime_error);
  EXPECT_THROW(f2.std_future().get(), std::runtime_error);
  EXPECT_THROW(f3.std_future().get(), std::runtime_error);
  EXPECT_THROW(f4.std_future().get(), std::runtime_error);
}

TEST(Future_alloc, then_expect_failure_post) {
  pf_set pf;

  auto f1 = pf[0].f.then_expect(expected_noop_fail);
  auto f2 = pf[1].f.then_expect(expected_noop_fail);
  auto f3 = pf[2].f.then_expect(expected_noop_fail);
  auto f4 = pf[3].f.then_expect(expected_noop_fail);

  pf.complete();

  EXPECT_THROW(f1.std_future().get(), std::runtime_error);
  EXPECT_THROW(f2.std_future().get(), std::runtime_error);
  EXPECT_THROW(f3.std_future().get(), std::runtime_error);
  EXPECT_THROW(f4.std_future().get(), std::runtime_error);
}

TEST(Future_alloc, then_expect_finally_success_pre) {
  pf_set pf;

  auto ref = expect_noop_count;

  pf[0].f.finally(expected_noop);
  pf[1].f.finally(expected_noop);
  pf[2].f.finally(expected_noop);
  pf[3].f.finally(expected_noop);

  pf.complete();

  EXPECT_EQ(4 + ref, expect_noop_count);
}

TEST(Future_alloc, then_expect_finally_success_post) {
  pf_set pf;

  auto ref = expect_noop_count;

  pf.complete();

  pf[0].f.finally(expected_noop);
  pf[1].f.finally(expected_noop);
  pf[2].f.finally(expected_noop);
  pf[3].f.finally(expected_noop);

  EXPECT_EQ(4 + ref, expect_noop_count);
}

namespace {
expected<int> generate_expected_value(int) { return 3; }

expected<int> generate_expected_value_fail(int) {
  return aom::unexpected{std::make_exception_ptr(std::runtime_error("yo"))};
}

expected<int> generate_expected_value_throw(int) {
  throw std::runtime_error("yo");
}
}  // namespace

TEST(Future_alloc, expected_returning_callback) {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(generate_expected_value);
  auto f2 = pf[1].f.then(generate_expected_value);
  auto f3 = pf[2].f.then(generate_expected_value);
  auto f4 = pf[3].f.then(generate_expected_value);

  EXPECT_EQ(3, f1.get());
  EXPECT_THROW(f2.std_future().get(), std::logic_error);
  EXPECT_EQ(3, f3.get());
  EXPECT_THROW(f4.std_future().get(), std::logic_error);
}

TEST(Future_alloc, expected_returning_callback_fail) {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(generate_expected_value_fail);
  auto f2 = pf[1].f.then(generate_expected_value_fail);
  auto f3 = pf[2].f.then(generate_expected_value_fail);
  auto f4 = pf[3].f.then(generate_expected_value_fail);

  EXPECT_THROW(f1.std_future().get(), std::runtime_error);
  EXPECT_THROW(f2.std_future().get(), std::logic_error);
  EXPECT_THROW(f3.std_future().get(), std::runtime_error);
  EXPECT_THROW(f4.std_future().get(), std::logic_error);
}

TEST(Future_alloc, expected_returning_callback_throw) {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then(generate_expected_value_throw);
  auto f2 = pf[1].f.then(generate_expected_value_throw);
  auto f3 = pf[2].f.then(generate_expected_value_throw);
  auto f4 = pf[3].f.then(generate_expected_value_throw);

  EXPECT_THROW(f1.std_future().get(), std::runtime_error);
  EXPECT_THROW(f2.std_future().get(), std::logic_error);
  EXPECT_THROW(f3.std_future().get(), std::runtime_error);
  EXPECT_THROW(f4.std_future().get(), std::logic_error);
}

namespace {
expected<int> te_generate_expected_value(expected<int>) { return 3; }

expected<int> te_generate_expected_value_fail(expected<int>) {
  return aom::unexpected{std::make_exception_ptr(std::runtime_error("yo"))};
}

expected<int> te_generate_expected_value_throw(expected<int>) {
  throw std::runtime_error("yo");
}
}  // namespace

TEST(Future_alloc, te_expected_returning_callback) {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then_expect(te_generate_expected_value);
  auto f2 = pf[1].f.then_expect(te_generate_expected_value);
  auto f3 = pf[2].f.then_expect(te_generate_expected_value);
  auto f4 = pf[3].f.then_expect(te_generate_expected_value);

  EXPECT_EQ(3, f1.get());
  EXPECT_EQ(3, f2.get());
  EXPECT_EQ(3, f3.get());
  EXPECT_EQ(3, f4.get());
}

TEST(Future_alloc, te_expected_returning_callback_fail) {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then_expect(te_generate_expected_value_fail);
  auto f2 = pf[1].f.then_expect(te_generate_expected_value_fail);
  auto f3 = pf[2].f.then_expect(te_generate_expected_value_fail);
  auto f4 = pf[3].f.then_expect(te_generate_expected_value_fail);

  EXPECT_THROW(f1.std_future().get(), std::runtime_error);
  EXPECT_THROW(f2.std_future().get(), std::runtime_error);
  EXPECT_THROW(f3.std_future().get(), std::runtime_error);
  EXPECT_THROW(f4.std_future().get(), std::runtime_error);
}

TEST(Future_alloc, te_expected_returning_callback_throw) {
  pf_set pf;

  pf.complete();

  auto f1 = pf[0].f.then_expect(te_generate_expected_value_throw);
  auto f2 = pf[1].f.then_expect(te_generate_expected_value_throw);
  auto f3 = pf[2].f.then_expect(te_generate_expected_value_throw);
  auto f4 = pf[3].f.then_expect(te_generate_expected_value_throw);

  EXPECT_THROW(f1.std_future().get(), std::runtime_error);
  EXPECT_THROW(f2.std_future().get(), std::runtime_error);
  EXPECT_THROW(f3.std_future().get(), std::runtime_error);
  EXPECT_THROW(f4.std_future().get(), std::runtime_error);
}
/*
TEST(Future_alloc, promote_tuple_to_variadic) {
  Promise<std::tuple<int, int>> p_t;
  auto f_t = p_t.get_future();

  Future<int, int> real_f = flatten(f_t);

  int a = 0;
  int b = 0;

  real_f.finally([&](expected<int> ia, expected<int> ib) {
    a = *ia;
    b = *ib;
  });

  EXPECT_EQ(0, a);
  EXPECT_EQ(0, b);

  p_t.set_value(std::make_tuple(2,3));
  EXPECT_EQ(2, a);
  EXPECT_EQ(3, b);
}
*/
