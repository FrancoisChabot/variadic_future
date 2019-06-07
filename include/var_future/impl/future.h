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

#ifndef AOM_VARIADIC_IMPL_FUTURE_INCLUDED_H
#define AOM_VARIADIC_IMPL_FUTURE_INCLUDED_H

#include "var_future/config.h"

#include "var_future/impl/then.h"
#include "var_future/impl/then_expect.h"
#include "var_future/impl/then_finally_expect.h"

#include <future>

namespace aom {

template <typename... Ts>
Future<Ts...>::Future(fullfill_type values)
    : storage_(std::make_shared<storage_type>()) {
  storage_->fullfill(std::move(values));
}

template <typename... Ts>
Future<Ts...>::Future(finish_type f)
    : storage_(std::make_shared<storage_type>()) {
  storage_->finish(f);
}

template <typename... Ts>
Future<Ts...>::Future(fail_type err)
    : storage_(std::make_shared<storage_type>()) {
  storage_->fail(err);
}

template <typename... Ts>
Future<Ts...>::Future(std::shared_ptr<storage_type> s)
    : storage_(std::move(s)) {}

// Synchronously calls cb once the future has been fulfilled.
// cb will be invoked directly in whichever thread fullfills
// the future.
//
// returns a future of whichever type is returned by cb.
// If this future is failed, then the resulting future
// will be failed with that same failure, and cb will be destroyed without
// being invoked.
//
// If you intend to discard the result, then you may want to use
// then_finally_expect() instead.
template <typename... Ts>
template <typename CbT>
[[nodiscard]] auto Future<Ts...>::then(CbT cb) {
  // Kinda flying by the seats of our pants here...
  // We rely on the fact that `Immediate_queue` handlers ignore the passed
  // queue.
  detail::Immediate_queue queue;
  return this->then(std::move(cb), queue);
}

template <typename... Ts>
template <typename CbT>
[[nodiscard]] auto Future<Ts...>::then_expect(CbT cb) {
  // Kinda flying by the seats of our pants here...
  // We rely on the fact that `Immediate_queue` handlers ignore the passed
  // queue.
  detail::Immediate_queue queue;
  return this->then_expect(std::move(cb), queue);
}


template <typename... Ts>
template <typename CbT>
void Future<Ts...>::then_finally_expect(CbT cb) {
  detail::Immediate_queue queue;
  return this->then_finally_expect(std::move(cb), queue);
}

// Queues cb in the target queue once the future has been fulfilled.
//
// It's expected that the queue itself will outlive the future.
//
// Returns a future of whichever type is returned by cb.
// If this future is failed, then the resulting future
// will be failed with that same failure, and cb will be destroyed without
// being invoked.
//
// The assignment of the failure will be done synchronously in the fullfilling
// thread.
// TODO: Maybe we can add an option to change that behavior
template <typename... Ts>
template <typename CbT, typename QueueT>
[[nodiscard]] auto Future<Ts...>::then(CbT cb, QueueT& queue) {
  using handler_t = detail::Future_then_handler<CbT, QueueT, Ts...>;
  using result_storage_t = typename handler_t::dst_storage_type;
  using result_fut_t = typename result_storage_t::future_type;

  auto result = std::make_shared<result_storage_t>();

  storage_->template set_handler<handler_t>(&queue, result, std::move(cb));

  return result_fut_t(std::move(result));
}

template <typename... Ts>
template <typename CbT, typename QueueT>
[[nodiscard]] auto Future<Ts...>::then_expect(CbT cb, QueueT& queue) {
  using handler_t = detail::Future_then_expect_handler<CbT, QueueT, Ts...>;
  using result_storage_t = typename handler_t::dst_storage_type;
  using result_fut_t = typename result_storage_t::future_type;

  auto result = std::make_shared<result_storage_t>();

  storage_->template set_handler<handler_t>(&queue, result, std::move(cb));

  return result_fut_t(std::move(result));
}

template <typename... Ts>
template <typename CbT, typename QueueT>
void Future<Ts...>::then_finally_expect(CbT cb, QueueT& queue) {
  assert(storage_);

  using handler_t =
      detail::Future_then_finally_expect_handler<CbT, QueueT, Ts...>;
  storage_->template set_handler<handler_t>(&queue, std::move(cb));
}

template <typename... Ts>
auto Future<Ts...>::get_std_future() {
  constexpr bool all_voids = std::conjunction_v<std::is_same<Ts, void>...>;
  if constexpr (all_voids) {
    std::promise<void> prom;
    auto fut = prom.get_future();
    this->then_finally_expect(
        [p = std::move(prom)](expected<Ts>... vals) mutable {
          auto err = detail::get_first_error(vals...);
          if (err) {
            p.set_exception(*err);
          } else {
            p.set_value();
          }
        });
    return fut;
  } else if constexpr (sizeof...(Ts) == 1) {
    using T = std::tuple_element_t<0, std::tuple<Ts...>>;

    std::promise<T> prom;
    auto fut = prom.get_future();
    this->then_finally_expect([p = std::move(prom)](expected<T> v) mutable {
      if (v.has_value()) {
        p.set_value(std::move(v.value()));
      } else {
        p.set_exception(v.error());
      }
    });
    return fut;
  } else {
    using tuple_t = detail::fullfill_type_t<Ts...>;

    std::promise<tuple_t> p;
    auto fut = p.get_future();
    this->then_finally_expect([p = std::move(p)](expected<Ts>... v) mutable {
      auto err = detail::get_first_error(v...);
      if (err) {
        p.set_exception(*err);
      } else {
        p.set_value({v.value()...});
      }
    });
    return fut;
  }
}

}  // namespace aom
#endif