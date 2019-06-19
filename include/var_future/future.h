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

template <typename Alloc, typename... Ts>
class Basic_promise;

/**
 * @brief Values that will be eventually available
 *
 * @tparam Ts
 * fields set to `void` have special rules applied to them.
 * The following types may not be used:
 * - `expected<T>`
 * - `Future<Ts...>`
 * - `Promise<T>`
 *
 * @invariant A Future is in one of two states:
 * - \b Uninitialized: The only legal operation is to assign another future to
 * it.
 * - \b Ready: All operations are legal.
 */
template <typename Alloc, typename... Ts>
class Basic_future {
  static_assert(sizeof...(Ts) >= 1, "you probably meant Future<void>");

 public:
  using promise_type = Basic_promise<Alloc, Ts...>;

   /// The underlying storage type.
  using storage_type = detail::Future_storage<Alloc, Ts...>;

  /// Allocator
  using allocator_type = Alloc;

  /// Ts...
  using value_type = detail::future_value_type_t<Ts...>;
  using fullfill_type = detail::fullfill_type_t<Ts...>;
  using finish_type = detail::finish_type_t<Ts...>;

  /**
   * @brief Construct an \b uninitialized Future
   *
   * @post `this` will be \b uninitialized
   */
  Basic_future() = default;

  /**
   * @brief Move constructor
   *
   * @param rhs
   *
   * @post `rhs` will be \b uninitialized
   */
  Basic_future(Basic_future&& rhs) = default;

  /**
   * @brief Move assignment
   *
   * @param rhs
   * @return Future&
   *
   * @post `rhs` will be \b uninitialized
   */
  Basic_future& operator=(Basic_future&& rhs) = default;

  /**
   * @brief Creates a future that is finished by the invocation of cb when this
   *        is fullfilled.
   *
   * @tparam CbT
   * @param callback
   * @return Future<decltype(callback(Ts...))> a \ready Future
   *
   * @pre the future must be \b ready
   * @post the future will be \b uninitialized
   */
  template <typename CbT>
  [[nodiscard]] auto then(CbT&& callback);

  /**
   * @brief Creates a future that is finished by the invocation of cb when this
   *        is finished.
   *
   * @tparam QueueT
   * @tparam CbT
   * @param queue
   * @param callback
   * @return auto
   *
   * @pre the future must be \b ready
   * @post the future will be \b uninitialized
   */
  template <typename QueueT, typename CbT>
  [[nodiscard]] auto then(QueueT& queue, CbT&& callback);

  /**
   * @brief Creates a future that is finished by the invocation of cb when this
   *        is finished.
   *
   * @tparam CbT
   * @param callback
   * @return auto
   *
   * @pre the future must be \b ready
   * @post the future will be \b uninitialized
   */
  template <typename CbT>
  [[nodiscard]] auto then_expect(CbT&& callback);

  /**
   * @brief Creates a future that is finished by the invocation of cb from
   *        queue when this is finished.
   *
   * @tparam QueueT
   * @tparam CbT
   * @param queue
   * @param callback
   * @return auto
   *
   * @pre the future must be \b ready
   * @post the future will be \b uninitialized
   */
  template <typename QueueT, typename CbT>
  [[nodiscard]] auto then_expect(QueueT& queue, CbT&& callback);

  /**
   * @brief Invokes cb when this is finished.
   *
   * @tparam CbT
   * @param callback
   *
   * @pre the future must be \b ready
   * @post the future will be \b uninitialized
   */
  template <typename CbT>
  void finally(CbT&& callback);

  /**
   * @brief Invokes cb from queue when this is finished.
   *
   * @tparam QueueT
   * @tparam CbT
   * @param queue
   * @param callback
   *
   * @pre the future must be \b ready
   * @post the future will be \b uninitialized
   */
  template <typename QueueT, typename CbT>
  void finally(QueueT& queue, CbT&& callback);

  /**
   * @brief Blocks until the future is finished, and then either return the
   *        value, or throw the error
   *
   * @return auto
   *
   * @pre the future must be \b ready
   * @post the future will be \b uninitialized
   */
  value_type get();

  /**
   * @brief Obtain a std::future bound to this future.
   *
   * @return auto
   */
  auto std_future();

  /**
   * @brief Get the allocator associated with this future.
   *
   * @pre The future must have shared storage associated with it.
   *
   * @return Alloc&
   */
  allocator_type& allocator();

  /**
   * @brief Create a future directly from its underlying storage.
   */
  explicit Basic_future(detail::Storage_ptr<storage_type> s);

private:
  detail::Storage_ptr<storage_type> storage_;
};

template <typename... Ts>
using Future = Basic_future<std::allocator<void>, Ts...>;

/**
 * @brief Error assigned to a future who's promise is destroyed before being
 *        finished.
 *
 */
struct Unfullfilled_promise : public std::logic_error {
  Unfullfilled_promise() : std::logic_error("Unfullfilled_promise") {}
};

/**
 * @brief Landing for a value that finishes a Future.
 *
 * @tparam Ts
 */
template <typename Alloc, typename... Ts>
class Basic_promise {
  static_assert(sizeof...(Ts) >= 1, "you probably meant Promise<void>");

 public:
  using future_type = Basic_future<Alloc, Ts...>;
  using storage_type = typename future_type::storage_type;

  using value_type = detail::future_value_type_t<Ts...>;

  using fullfill_type = detail::fullfill_type_t<Ts...>;
  using finish_type = detail::finish_type_t<Ts...>;
  using fail_type = detail::fail_type_t<Ts...>;

  Basic_promise(const Alloc& alloc = Alloc());
  Basic_promise(Basic_promise&&) = default;
  Basic_promise& operator=(Basic_promise&&) = default;
  ~Basic_promise();

  /**
   * @brief Returns a future that is bound to this promise
   *
   * @return future_type
   */
  future_type get_future();

  /**
   * @brief Fullfills the promise
   *
   * @tparam Us
   * @param values
   */
  template <typename... Us>
  void set_value(Us&&... values);

  /**
   * @brief Finishes the promise
   *
   * @tparam Us
   * @param expecteds
   */
  template <typename... Us>
  void finish(Us&&... expecteds);

  /**
   * @brief Fails the promise
   *
   * @param error
   */
  void set_exception(fail_type error);

  // private:
  detail::Storage_ptr<storage_type> storage_;

  Basic_promise(const Basic_promise&) = delete;
  Basic_promise& operator=(const Basic_promise&) = delete;
};

template <typename... Ts>
using Promise = Basic_promise<std::allocator<void>, Ts...>;
/**
 * @brief Ties a set of Future<> into a single Future<> that is finished once
 *        all child futures are finished.
 *
 * @tparam FutTs
 * @param futures
 * @return auto
 */
template <typename... FutTs>
auto join(FutTs&&... futures);

// Convenience function that creates a promise for the result of the cb, pushes
// cb in q, and returns a future to that promise.

/**
 * @brief Posts a callback into to queue, and return a future that will be
 *        finished upon executaiton of the callback.
 *
 * @tparam QueueT
 * @tparam CbT
 * @param q
 * @param callback
 * @return auto
 */
template <typename QueueT, typename CbT>
auto async(QueueT& q, CbT&& callback);

/**
 * @brief Create a higher-order Future from a `future<tuple>`
 *
 * @param rhs
 *
 * @post `rhs` will be \b uninitialized.
 * @post this will inherit the state `rhs` was in.
 */
template <typename Alloc, typename... Ts>
Basic_future<Alloc, Ts...> flatten(Basic_future<Alloc, std::tuple<Ts...>>& rhs);

template<typename... Ts>
auto segmented(Ts&&... args);


}  // namespace aom

#include "var_future/impl/async.h"
#include "var_future/impl/future.h"
#include "var_future/impl/join.h"
#include "var_future/impl/promise.h"
#include "var_future/impl/storage_impl.h"

#endif
