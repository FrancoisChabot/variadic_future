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

#include <atomic>
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
  void fullfill(future_type&& f);

  void finish(finish_type&& f);
  void finish(future_type&& f);
  
  void fail(fail_type&& e);

  template <typename Handler_t, typename QueueT, typename... Args_t>
  void set_handler(QueueT* queue, Args_t&&... args);

 private:
  enum class State {
    PENDING,
    READY,
    READY_SOO,
    FINISHED,
    FULLFILLED,
    ERROR,
  } state_ = State::PENDING;

  static bool is_ready_state(State v);

  // Calculate how much memory we want to reserve for the eventual callback
  static constexpr std::size_t soo_space =
      std::min(std::size_t(var_fut_min_soo_size),
               std::max({sizeof(fullfill_type), sizeof(finish_type),
                         sizeof(fail_type)}) -
                   sizeof(Future_handler_iface<Ts...>*));


  struct Callback_data {
    // This will either point to soo_buffer_, or heap-allocated data, depending on state_.
    Future_handler_iface<Ts...>* callback_; 
    typename std::aligned_storage<soo_space>::type soo_buffer_;
  };

  union {
    Callback_data cb_data_;
    fullfill_type fullfilled_;
    finish_type finished_;
    fail_type failure_;
  };

  std::mutex mtx_;

  template <typename T>
  friend struct Storage_ptr;

  std::atomic<std::uint8_t> ref_count_ = 0;
};

// Yes, we are using a custom std::shared_ptr<> alternative. This is because
// common handlers have a owning pointer to a Future_storage, and each byte
// saved increases the likelyhood that it will fit in SOO, which has very large
// performance impact.
template <typename T>
struct Storage_ptr {
  Storage_ptr() = default;
  Storage_ptr(T* val) : ptr_(val) { 
    assert(val);
    ++(ptr_->ref_count_); 
  }

  Storage_ptr(const Storage_ptr& rhs) : ptr_(rhs.ptr_) {
    if (ptr_) {
      ++(ptr_->ref_count_);
    }
  }

  Storage_ptr(Storage_ptr&& rhs) : ptr_(rhs.ptr_) { rhs.ptr_ = nullptr; }

  Storage_ptr& operator=(const Storage_ptr& rhs) = delete;
  Storage_ptr& operator=(Storage_ptr&& rhs) {
    reset();

    ptr_ = rhs.ptr_;
    if (ptr_) {
      ++ptr_->ref_count_;
    }
    return *this;
  }

  void reset() {
    if (ptr_) {
      if (--(ptr_->ref_count_) == 0) {
        delete ptr_;
      }
    }
    ptr_ = nullptr;
  }

  operator bool() const { return ptr_ != nullptr; }

  ~Storage_ptr() { reset(); }

  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }

  T* ptr_ = nullptr;
};

template <typename T>
struct Storage_for_cb_result {
  using type = Future_storage<decay_future_t<T>>;
};

template <typename T>
struct Storage_for_cb_result<expected<T>> {
  using type = typename Storage_for_cb_result<T>::type;
};

template <typename T>
using Storage_for_cb_result_t = typename Storage_for_cb_result<T>::type;

}  // namespace detail
}  // namespace aom
#endif