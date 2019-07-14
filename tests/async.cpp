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

TEST_CASE("async function") {
  std::queue<std::function<void()>> queue;

  auto fut = async(queue, []() { return 12; });

  REQUIRE_EQ(1, queue.size());
  int dst = 0;
  fut.finally([&](expected<int> x) { dst = x.value(); });
  REQUIRE_EQ(0, dst);

  queue.front()();
  queue.pop();

  REQUIRE_EQ(12, dst);
}
