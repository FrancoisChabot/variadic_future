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

#ifndef AOM_VARIADIC_IMPL_THEN_INCLUDED_H
#define AOM_VARIADIC_IMPL_THEN_INCLUDED_H

#include "var_future/config.h"

#include "var_future/impl/storage_decl.h"

namespace aom {

namespace detail {

// Handler class for defered Future::then() execution
template <typename CbT, typename QueueT, typename... Ts>
class Future_then_handler : public Future_handler_base<QueueT, void, Ts...> {
 public:
  using parent_type = Future_handler_base<QueueT, void, Ts...>;

  using fullfill_type = typename parent_type::fullfill_type;
  using finish_type = typename parent_type::finish_type;
  using fail_type = typename parent_type::fail_type;

  using cb_result_type =
      decltype(std::apply(std::declval<CbT>(), std::declval<fullfill_type>()));
  static_assert(!is_expected_v<cb_result_type>,
                "callbacks returning expecteds is not supported yet.");

  using dst_storage_type = Storage_for_cb_result_t<cb_result_type>;
  using dst_type = Storage_ptr<dst_storage_type>;

  Future_then_handler(QueueT* q, dst_type dst, CbT cb)
      : parent_type(q), dst_(std::move(dst)), cb_(std::move(cb)) {}

  void fullfill(fullfill_type v) override {
    do_fullfill(this->get_queue(), std::move(v), std::move(dst_),
                std::move(cb_));
  };

  void finish(finish_type f) override {
    do_finish(this->get_queue(), f, std::move(dst_), std::move(cb_));
  }

  void fail(fail_type e) override {
    do_fail(this->get_queue(), e, std::move(dst_), std::move(cb_));
  }

  static void do_fullfill(QueueT* q, fullfill_type v, dst_type dst, CbT cb) {
    enqueue(q, [cb = std::move(cb), dst = std::move(dst), v = std::move(v)] {
      try {
        if constexpr (std::is_same_v<void, cb_result_type>) {
          std::apply(cb, std::move(v));
          dst->fullfill(std::tuple<>{});
        } else {
          dst->fullfill(std::apply(cb, std::move(v)));
        }

      } catch (...) {
        dst->fail(std::current_exception());
      }
    });
  }

  static void do_finish(QueueT* q, finish_type f, dst_type dst, CbT cb) {
    auto err = std::apply(get_first_error<Ts...>, f);
    if (err) {
      do_fail(q, *err, std::move(dst), std::move(cb));
    } else {
      fullfill_type cb_args;
      finish_to_fullfill<0, 0>(std::move(f), cb_args);

      do_fullfill(q, std::move(cb_args), std::move(dst), std::move(cb));
    }
  }

  static void do_fail(QueueT* q, fail_type e, dst_type dst, CbT) {
    // Straight propagation.
    enqueue(q, [dst = std::move(dst), e = std::move(e)]() mutable {
      dst->fail(std::move(e));
    });
  }

 private:
  dst_type dst_;
  CbT cb_;
};
}  // namespace detail
}  // namespace aom
#endif