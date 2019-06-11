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

#include <memory>

namespace aom {

template <typename... Ts>
class Future {
  static_assert(sizeof...(Ts) >= 1, "you probably meant Future<void>");

 public:
  using storage_type = detail::Future_storage<Ts...>;
  
  using value_type = detail::future_value_type_t<Ts...>;

  using fullfill_type = detail::fullfill_type_t<Ts...>;
  using finish_type = detail::finish_type_t<Ts...>;
  using fail_type = detail::fail_type_t<Ts...>;

  //
  Future() = default;

  // returns a future that's already fullfilled.
  template <typename... Us>
  static Future fullfilled(Us&&...);
  
  // returns a future that's already finished
  template <typename... Us>
  static Future finished(Us&&...);

  // returns a future that's already failed.
  static Future failed(std::exception_ptr);

  // Promote a future of tuple into a higher-order future.
  explicit Future(Future<std::tuple<Ts...>>&& rhs);

  Future(Future&&) = default;
  Future& operator=(Future&&) = default;

  // Calls cb once the future has been fulfilled.
  //
  // expects: cb to be a Callable(Ts...)
  //
  // Returns: a future of whichever type is returned by cb.
  // If this future is failed, then the resulting future
  // will be failed with that same failure, and cb will be destroyed without
  // being invoked.
  //
  // if cb throws an exception, that exception will become the resulting
  // future's failure
  template <typename CbT>
  [[nodiscard]] auto then(CbT cb);

  // Pushes the execution of cb in queue once the future has been fulfilled.
  //
  // expects: cb to be a Callable(Ts...)
  //
  // Returns: a future of whichever type is returned by cb.
  // If this future is failed, then the resulting future
  // will be failed with that same failure, and cb will be destroyed without
  // being invoked.
  //
  // if cb throws an exception, that exception will become the resulting
  // future's failure
  template <typename CbT, typename QueueT>
  [[nodiscard]] auto then(QueueT& queue, CbT cb);

  // Calls cb once the future has been fulfilled.
  //
  // expects: cb to be a Callable(aom::expected<Ts>...)
  //
  // Returns: a future of whichever type is returned by cb.
  // If this future is failed, then the resulting future
  // will be failed with that same failure, and cb will be destroyed without
  // being invoked.
  //
  // if cb throws an exception, that exception will become the resulting
  // future's failure
  template <typename CbT>
  [[nodiscard]] auto then_expect(CbT cb);

  // Pushes the execution of cb in queue once the future has been fulfilled.
  //
  // expects: cb to be a Callable(aom::expected<Ts>...)
  //
  // Returns: a future of whichever type is returned by cb.
  // If this future is failed, then the resulting future
  // will be failed with that same failure, and cb will be destroyed without
  // being invoked.
  //
  // if cb throws an exception, that exception will become the resulting
  // future's failure
  template <typename CbT, typename QueueT>
  [[nodiscard]] auto then_expect(QueueT& queue, CbT cb);

  // Calls cb once the future has been fulfilled.
  //
  // expects: cb to be a Callable(aom::expected<Ts>...)
  template <typename CbT>
  void finally(CbT cb);

  // Pushes the execution of cb in queue once the future has been fulfilled.
  //
  // expects: cb to be a Callable(aom::expected<Ts>...)
  template <typename CbT, typename QueueT>
  void finally(QueueT& queue, CbT cb);

  // Convenience function to obtain a std::future<> bound to this future.
  auto std_future();

  // Shorthand for std_future().get().
  auto get();

 private:
  detail::Storage_ptr<storage_type> storage_;

  Future(const Future&) = delete;
  Future& operator=(const Future&) = delete;


  template<typename CbT, typename QueueT>
  friend auto async(QueueT& q, CbT&& cb);

  template <typename... Us>
  friend auto tie(Us&&...);

  template <typename... Us>
  friend class Future;

  template <typename... Us>
  friend class Promise;
  
  // Primarily internal constructor.
  explicit Future(detail::Storage_ptr<storage_type> s);

};

// Error assigned to a future when its promise is destroyed before being finished.
struct Unfullfilled_promise : public std::logic_error {
  Unfullfilled_promise() : std::logic_error("Unfullfilled_promise") {}
};

template <typename... Ts>
class Promise {
  static_assert(sizeof...(Ts) >= 1, "you probably meant Promise<void>");

 public:
  using future_type = Future<Ts...>;
  using storage_type = typename future_type::storage_type;

  using value_type = detail::future_value_type_t<Ts...>;

  using fullfill_type = detail::fullfill_type_t<Ts...>;
  using finish_type = detail::finish_type_t<Ts...>;
  using fail_type = detail::fail_type_t<Ts...>;

  Promise() = default;
  Promise(Promise&&) = default;
  Promise& operator=(Promise&&) = default;
  ~Promise();

  // Returns a future that is bound to this promise
  future_type get_future();

  // Fullfills the promise.
  //
  // expects: std::forward<Us...>(vals) can be used to initialize fullfill_type.
  //          which is std::tuple<Ts..> with the void types removed from Ts.
  //          For example:
  //          Promise<void, int, void, int> p; p->set_value(1, 2);
  template <typename... Us>
  void set_value(Us&&... vals);

  // Finishes the promise.
  //
  // expects: std::forward<Us...>(vals) can be used to initialize finish_type.
  //          which is std::tuple<expected<Ts>..>
  template <typename... Us>
  void finish(Us&&... f);

  // Fails the promise.
  //
  // If the promise as more than one member, it fails all of them with the same
  // error.
  void set_exception(fail_type e);

 private:
  detail::Storage_ptr<storage_type> storage_;

  Promise(const Promise&) = delete;
  Promise& operator=(const Promise&) = delete;
};

// Ties a set of Future<> into a single Future<> that is finished once all child
// futures are finished.
template <typename... FutTs>
auto tie(FutTs&&... futs);

template<typename CbT, typename QueueT>
auto async(QueueT& q, CbT&& cb);

}  // namespace aom

#include "var_future/impl/future.h"
#include "var_future/impl/promise.h"
#include "var_future/impl/storage_impl.h"
#include "var_future/impl/tie.h"
#include "var_future/impl/async.h"

#endif