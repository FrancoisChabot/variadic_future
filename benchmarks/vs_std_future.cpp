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

// This is an attempt to create as represnetative as possible a
// comparison with std::future.

#include <benchmark/benchmark.h>
#include "var_future/future.h"

// This is not the fairest of tests.
static void BM_std_future_reference(benchmark::State& state) {
  for (auto _ : state) {
    std::vector<std::promise<int>> proms(2000);
    std::vector<std::future<int>> futs;
    futs.reserve(proms.size());
    for (auto& p : proms) {
      futs.push_back(p.get_future());
    }

    std::thread worker([ps = std::move(proms)]() mutable {
      int i = 0;
      for (auto& p : ps) {
        p.set_value(++i);
      }
    });
    worker.detach();

    int total = 0;
    for (auto& f : futs) {
      total += f.get();
    }

    benchmark::DoNotOptimize(total);
  }
}

// This uses varfut in a weird way, but that matches better with how std futures
// work.
static void BM_using_varfut_fair(benchmark::State& state) {
  for (auto _ : state) {
    std::vector<aom::Promise<int>> proms(2000);
    std::vector<aom::Future<int>> futs;
    futs.reserve(proms.size());

    for (auto& p : proms) {
      futs.push_back(p.get_future());
    }

    std::thread worker([ps = std::move(proms)]() mutable {
      int i = 0;
      for (auto& p : ps) {
        p.set_value(++i);
      }
    });

    int total = 0;
    for (auto& f : futs) {
      f.finally([&total](aom::expected<int> v) { total += *v; });
    }
    worker.join();

    benchmark::DoNotOptimize(total);
  }
}

// This is more representative of how you would use varfut to tackle that
// specific problem.
static void BM_using_varfut_normal(benchmark::State& state) {
  for (auto _ : state) {
    std::vector<aom::Promise<int>> proms(2000);

    int total = 0;
    for (auto& p : proms) {
      p.get_future().finally([&total](aom::expected<int> v) { total += *v; });
    }

    std::thread worker([ps = std::move(proms)]() mutable {
      int i = 0;
      for (auto& p : ps) {
        p.set_value(++i);
      }
    });

    worker.join();

    benchmark::DoNotOptimize(total);
  }
}
// Register the function as a benchmark
BENCHMARK(BM_std_future_reference);
BENCHMARK(BM_using_varfut_normal);
BENCHMARK(BM_using_varfut_fair);
// Run the benchmark
BENCHMARK_MAIN();