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

template <typename T>
struct Test_alloc {
  using value_type = T;

  template <typename U>
  explicit Test_alloc(const Test_alloc<U>&) {}

  template <typename U>
  struct rebind {
    using other = Test_alloc<U>;
  };

  T* allocate(std::size_t count) {
    return reinterpret_cast<T*>(std::malloc(count * sizeof(T)));
  }

  void deallocate(T* ptr, std::size_t) { std::free(ptr); }
};

template <>
struct Test_alloc<void> {
  using value_type = void;

  template <typename U>
  struct rebind {
    using other = Test_alloc<U>;
  };
};

TEST_CASE("joining futures") {
SUBCASE("simple_join") {
  Promise<int> p1;
  Promise<int> p2;

  auto f = join(p1.get_future(), p2.get_future()).then([](int x, int y) {
    return x + y;
  });

  std::cout << "aa\n";
  p1.set_value(1);
  p2.set_value(2);

  std::cout << "bb\n";
  REQUIRE_EQ(3, f.get());
}

SUBCASE("simple_join_with_allocator") {
  Basic_promise<Test_alloc<void>, int> p1;
  Basic_promise<Test_alloc<void>, int> p2;

  auto f = join(p1.get_future(), p2.get_future()).then([](int x, int y) {
    return x + y;
  });

  p1.set_value(1);
  p2.set_value(2);

  REQUIRE_EQ(3, f.get());
}
}