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
#include "var_future/stream_future.h"

#include "gtest/gtest.h"

#include <queue>

using namespace aom;

TEST(Stream, ignored_promise) {
  Stream_promise<int> prom;

  (void)prom;
}

TEST(Stream, simple_stream) {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  int total = 0 ;

  fut.for_each([&](int v) {
    total += v;
  }).finally([&](expected<void>){
    total = -1;
  });
  
  EXPECT_EQ(total, 0);
  
  prom.push(1);
  EXPECT_EQ(total, 1);

  prom.push(2);
  EXPECT_EQ(total, 3);

  prom.push(3);
  EXPECT_EQ(total, 6);

  prom.complete();
  EXPECT_EQ(total, -1);
}

TEST(Stream, no_data_completed_stream) {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  int total = 0 ;

  auto done = fut.for_each([&](int v) {
    total += v;
  });
  
  prom.complete();
  done.get();

  EXPECT_EQ(total, 0);
}

TEST(Stream, partially_pre_filled) {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  prom.push(1);

  int total = 0 ;
  auto done = fut.for_each([&](int v) {
    total += v;
  });
  EXPECT_EQ(total, 1);

  prom.push(2);
  EXPECT_EQ(total, 3);

  prom.complete();
  done.get();
}


TEST(Stream, multiple_pre_filled) {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  prom.push(1);
  prom.push(2);
  int total = 0 ;
  auto done = fut.for_each([&](int v) {
    std::cout << v << "\n";
    total += v;
  });

  EXPECT_EQ(total, 3);

  prom.complete();
  done.get();
}

TEST(Stream, uncompleted_stream) {
  Stream_promise<int> prom;
  auto fut = prom.get_future();

  int total = 0 ;

  auto done = fut.for_each([&](int v) {
    total += v;
  });
  
  {
    Stream_promise<int> destroyer = std::move(prom);
    EXPECT_EQ(total, 0);
    destroyer.push(1);
    EXPECT_EQ(total, 1);
    destroyer.push(2);
    EXPECT_EQ(total, 3);
  }

  EXPECT_THROW(done.get(), Unfullfilled_promise);
}