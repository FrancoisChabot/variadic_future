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

#ifndef AOM_VARIADIC_IMPL_THEN_FINALLY_EXPECT_INCLUDED_H
#define AOM_VARIADIC_IMPL_THEN_FINALLY_EXPECT_INCLUDED_H

#include "var_future/config.h"

#include "var_future/impl/storage_decl.h"

namespace aom {

namespace detail {

// handling for Future::then_finally_expect()
template <typename CbT, typename QueueT, typename... Ts>
class Future_then_finally_expect_handler
    : public Future_handler_base<QueueT, void, Ts...> {
  using parent_type = Future_handler_base<QueueT, void, Ts...>;

  using fullfill_type = typename parent_type::fullfill_type;
  using finish_type = typename parent_type::finish_type;
  using fail_type = typename parent_type::fail_type;

  CbT cb_;

 public:
  Future_then_finally_expect_handler(QueueT* q, CbT cb)
      : parent_type(q), cb_(std::move(cb)) {}

  void fullfill(fullfill_type v) override {
    do_fullfill(this->get_queue(), std::move(v), std::move(cb_));
  };

  void finish(finish_type f) override {
    do_finish(this->get_queue(), std::move(f), std::move(cb_));
  };

  void fail(fail_type e) override {
    do_fail(this->get_queue(), e, std::move(cb_));
  }

  static void do_fullfill(QueueT* q, fullfill_type v, CbT cb) {
    std::tuple<expected<Ts>...> cb_args;
    fullfill_to_finish<0, 0>(std::move(v), cb_args);
    do_finish(q, std::move(cb_args), std::move(cb));
  }

  static void do_finish(QueueT* q, finish_type f, CbT cb) {
    enqueue(q, [cb = std::move(cb), f = std::move(f)]() mutable {
      std::apply(cb, std::move(f));
    });
  }

  static void do_fail(QueueT* q, fail_type e, CbT cb) {
    std::tuple<expected<Ts>...> cb_args;
    fail_to_expect<0>(std::move(e), cb_args);
    do_finish(q, std::move(cb_args), std::move(cb));
  }
};
}  // namespace detail
}  // namespace aom
#endif