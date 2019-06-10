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

#ifndef AOM_VARIADIC_IMPL_THEN_EXPECT_INCLUDED_H
#define AOM_VARIADIC_IMPL_THEN_EXPECT_INCLUDED_H

#include "var_future/config.h"

#include "var_future/impl/storage_decl.h"

namespace aom {

namespace detail {

// handling for Future::finally()
template <typename CbT, typename QueueT, typename... Ts>
class Future_then_expect_handler
    : public Future_handler_base<QueueT, void, Ts...> {
 public:
  using parent_type = Future_handler_base<QueueT, void, Ts...>;

  using fullfill_type = typename parent_type::fullfill_type;
  using finish_type = typename parent_type::finish_type;
  using fail_type = typename parent_type::fail_type;

  using cb_result_type =
      decltype(std::apply(std::declval<CbT>(), std::declval<finish_type>()));

  using dst_storage_type = Storage_for_cb_result_t<cb_result_type>;
  using dst_type = Storage_ptr<dst_storage_type>;

  Future_then_expect_handler(QueueT* q, dst_type dst, CbT cb)
      : parent_type(q), dst_(std::move(dst)), cb_(std::move(cb)) {}

  void fullfill(fullfill_type v) override {
    do_fullfill(this->get_queue(), std::move(v), std::move(dst_),
                std::move(cb_));
  };

  void finish(finish_type f) override {
    do_finish(this->get_queue(), std::move(f), std::move(dst_), std::move(cb_));
  };

  void fail(fail_type e) override {
    do_fail(this->get_queue(), e, std::move(dst_), std::move(cb_));
  }

  static void do_fullfill(QueueT* q, fullfill_type v, dst_type dst, CbT cb) {
    std::tuple<expected<Ts>...> cb_args;
    fullfill_to_finish<0, 0>(std::move(v), cb_args);

    do_finish(q, std::move(cb_args), std::move(dst), std::move(cb));
  }

  static void do_finish(QueueT* q, finish_type f, dst_type dst, CbT cb) {
    enqueue(q, [cb = std::move(cb), dst = std::move(dst), f = std::move(f)] {
      try {
        if constexpr (std::is_same_v<void, cb_result_type>) {
          std::apply(cb, std::move(f));
          dst->fullfill(std::tuple<>{});
        } else {
          if constexpr (is_expected_v<cb_result_type>) {
            dst->finish(std::apply(cb, std::move(f)));
          }
          else{
            dst->fullfill(std::apply(cb, std::move(f)));
          }
        }

      } catch (...) {
        dst->fail(std::current_exception());
      }
    });
  }

  static void do_fail(QueueT* q, fail_type e, dst_type dst, CbT cb) {
    std::tuple<expected<Ts>...> cb_args;
    fail_to_expect<0>(e, cb_args);
    do_finish(q, std::move(cb_args), std::move(dst), std::move(cb));
  }

 private:
  dst_type dst_;
  CbT cb_;
};
}  // namespace detail
}  // namespace aom
#endif