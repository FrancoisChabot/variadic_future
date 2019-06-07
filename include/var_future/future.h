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

#ifndef AOM_VARIADIC_FUTURE_INCLUDED_H
#define AOM_VARIADIC_FUTURE_INCLUDED_H

#include "var_future/config.h"
#include "var_future/impl/storage_decl.h"

#include <cassert>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>

namespace aom {

template <typename... Ts>
class Future {
  static_assert(sizeof...(Ts) >= 1, "you probably meant Future<void>");

 public:
  using storage_type = detail::Future_storage<Ts...>;
  using fullfill_type = detail::fullfill_type_t<Ts...>;
  using finish_type = detail::finish_type_t<Ts...>;
  using fail_type = detail::fail_type_t<Ts...>;

  Future() = default;

  explicit Future(fullfill_type);
  explicit Future(finish_type);
  explicit Future(fail_type);

  explicit Future(std::shared_ptr<storage_type> s);

  Future(Future&&) = default;
  Future(const Future&) = delete;
  Future& operator=(Future&&) = default;
  Future& operator=(const Future&) = delete;

  // Synchronously calls cb once the future has been fulfilled.
  // cb will be invoked directly in whichever thread fullfills
  // the future.
  //
  // Returns: a future of whichever type is returned by cb.
  // If this future is failed, then the resulting future
  // will be failed with that same failure, and cb will be destroyed without
  // being invoked.
  //
  // If you intend to discard the result, then you may want to use
  // then_finally_expect() instead.
  template <typename CbT>
  [[nodiscard]] auto then(CbT cb);

  template <typename CbT, typename QueueT>
  [[nodiscard]] auto then(CbT cb, QueueT& queue);

  template <typename CbT>
  [[nodiscard]] auto then_expect(CbT cb);

  template <typename CbT, typename QueueT>
  [[nodiscard]] auto then_expect(CbT cb, QueueT& queue);

  template <typename CbT>
  void then_finally_expect(CbT cb);

  template <typename CbT, typename QueueT>
  void then_finally_expect(CbT cb, QueueT& queue);

  // Convenience function to obtain a std::future<> bound to this future.
  auto get_std_future();

 private:
  std::shared_ptr<storage_type> storage_;
};

template <typename... Ts>
class Promise {
  static_assert(sizeof...(Ts) >= 1, "you probably meant Promise<void>");

 public:
  using future_type = Future<Ts...>;
  using storage_type = typename future_type::storage_type;

  using fullfill_type = detail::fullfill_type_t<Ts...>;
  using finish_type = detail::finish_type_t<Ts...>;
  using fail_type = detail::fail_type_t<Ts...>;

  future_type get_future();

  template <typename... Us>
  void set_value(Us&&... vals);

  template <typename... Us>
  void finish(Us&&... f);
   
  void set_exception(fail_type e);

 private:
  std::shared_ptr<storage_type> storage_;
};

// Ties a set of Future<> into a single Future<> that is finished once all child
// futures are finished.
template <typename... FutTs>
auto tie(FutTs&&... futs);

}  // namespace aom

#include "var_future/impl/future.h"
#include "var_future/impl/promise.h"
#include "var_future/impl/storage_impl.h"
#include "var_future/impl/tie.h"

#endif