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

TEST(Future, fill_from_promise_st_then) {
  
  // pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    prom.set_value(1);

    auto result_fut = fut.then([&](int v) {
      EXPECT_EQ(v, 1);
      return v + 4;
    });
    
    EXPECT_EQ(result_fut.get(), 5);
  }

  // post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    auto result_fut = fut.then([&](int v) {
      EXPECT_EQ(v, 1);
      return v + 4;
    });

    prom.set_value(1);

    EXPECT_EQ(result_fut.get(), 5);
  }
}

TEST(Future, fail_from_promise_st_then) {
  
  // pre-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    prom.set_exception(get_error());

    auto result_fut = fut.then([&](int v) {
      return v + 4;
    });
    
    EXPECT_THROW(result_fut.get(), std::runtime_error);
  }

  // post-filled
  {
    Promise<int> prom;
    auto fut = prom.get_future();

    auto result_fut = fut.then([&](int v) {
      return v + 4;
    });

    prom.set_exception(get_error());
    
    EXPECT_THROW(result_fut.get(), std::runtime_error);
  }
}
