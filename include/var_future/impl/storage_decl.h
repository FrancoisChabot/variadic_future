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

#ifndef AOM_VARIADIC_IMPL_STORAGE_DECL_INCLUDED_H
#define AOM_VARIADIC_IMPL_STORAGE_DECL_INCLUDED_H

#include "var_future/config.h"

#include "var_future/impl/utils.h"

#include <mutex>
#include <type_traits>

namespace aom {

namespace detail {

template <typename... Ts>
class Future_handler_iface {
 public:
  using fullfill_type = fullfill_type_t<Ts...>;
  using fail_type = fail_type_t<Ts...>;
  using finish_type = finish_type_t<Ts...>;

  Future_handler_iface() = default;
  Future_handler_iface(const Future_handler_iface&) = delete;
  Future_handler_iface(Future_handler_iface&&) = delete;
  Future_handler_iface& operator=(const Future_handler_iface&) = delete;
  Future_handler_iface& operator=(Future_handler_iface&&) = delete;
  virtual ~Future_handler_iface() {}

  // The future has been completed
  virtual void fullfill(fullfill_type) = 0;

  // The future has been potentially completed.
  // There may be 0 or all errors.
  virtual void finish(finish_type) = 0;

  // The future has been failed.
  virtual void fail(fail_type) = 0;
};

template <typename QueueT, typename Enable = void, typename... Ts>
class Future_handler_base : public Future_handler_iface<Ts...> {
 public:
  Future_handler_base(QueueT* q) : queue_(q) {}

 protected:
  QueueT* get_queue() { return queue_; }

 private:
  QueueT* queue_;
};

// If the queue's push() method is static, do not bother storing the pointer.
template <typename QueueT, typename... Ts>
class Future_handler_base<QueueT, std::enable_if_t<has_static_push_v<QueueT>>,
                          Ts...> : public Future_handler_iface<Ts...> {
 public:
  Future_handler_base(QueueT*) {}

 protected:
  constexpr static QueueT* get_queue() { return nullptr; }
};

// Holds the shared state associated with a Future<>.
template <typename... Ts>
class Future_storage {
 public:
  using fullfill_type = fullfill_type_t<Ts...>;
  using fail_type = fail_type_t<Ts...>;
  using finish_type = finish_type_t<Ts...>;

  using future_type = Future<Ts...>;

  Future_storage();
  ~Future_storage();

  void fullfill(fullfill_type&& v);
  void finish(finish_type&& f);
  void fail(fail_type&& e);

  template <typename Handler_t, typename QueueT, typename... Args_t>
  void set_handler(QueueT* queue, Args_t&&... args);

 private:
  enum class State {
    PENDING,
    READY,
    FINISHED,
    FULLFILLED,
    ERROR,
  } state_ = State::PENDING;

  union {
    Future_handler_iface<Ts...>* callbacks_;
    fullfill_type fullfilled_;
    finish_type finished_;
    fail_type failure_;
  };

  std::mutex mtx_;
};

template <typename T>
struct Storage_for_cb_result {
  using type = Future_storage<decay_future_t<T>>;
};

template <typename T>
using Storage_for_cb_result_t = typename Storage_for_cb_result<T>::type;

}  // namespace detail
}  // namespace aom
#endif