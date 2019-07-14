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

TEST_CASE("future of reference") {
  Promise<std::reference_wrapper<int>> p;
  Future<std::reference_wrapper<int>> f = p.get_future();

  int var = 0;

  p.set_value(var);

  f.finally(
      [](expected<std::reference_wrapper<int>> dst) { (*dst).get() = 4; });

  REQUIRE_EQ(var, 4);
}