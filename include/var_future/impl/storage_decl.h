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
#include <type_traits>

namespace aom {

namespace detail {

template <typename... Ts>
struct Segmented_callback_result {
  std::tuple<Ts...> values_;
};

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
  // virtual void fail(fail_type) = 0;
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

constexpr std::uint8_t Future_storage_state_ready_bit = 1;
constexpr std::uint8_t Future_storage_state_finished_bit = 2;

// Holds the shared state associated with a Future<>.
template <typename Alloc, typename... Ts>
class Future_storage : public Alloc {
  template <typename T>
  friend struct Storage_ptr;

  Future_storage(const Alloc& alloc);

 public:
  using allocator_type = Alloc;

  using fullfill_type = fullfill_type_t<Ts...>;
  using fail_type = fail_type_t<Ts...>;
  using finish_type = finish_type_t<Ts...>;

  using future_type = Basic_future<Alloc, Ts...>;

  ~Future_storage();

  void fullfill(fullfill_type&& v);

  template <typename Arg_alloc>
  void fullfill(Basic_future<Arg_alloc, Ts...>&& f);

  template <typename... Us>
  void fullfill(Segmented_callback_result<Us...>&& f);

  void finish(finish_type&& f);

  template <typename Arg_alloc>
  void finish(Basic_future<Arg_alloc, Ts...>&& f);

  void fail(fail_type&& e);

  template <typename Handler_t, typename QueueT, typename... Args_t>
  void set_handler(QueueT* queue, Args_t&&... args);

  Alloc& allocator() { return *static_cast<Alloc*>(this); }

  const Alloc& allocator() const { return *static_cast<Alloc*>(this); }

private:
  struct Callback_data {
    Future_handler_iface<Ts...>* callback_ = nullptr;
  };

  Callback_data cb_data_;
    
  // finished is in an union because it only gets constructed on demand.
  union {
    finish_type finished_;
  };

  // Storage_ptr support
  template <typename T>
  friend struct Storage_ptr;

  std::atomic<std::uint8_t> state_ = 0;
  std::atomic<std::uint8_t> ref_count_ = 0;
};

// Yes, we are using a custom std::shared_ptr<> alternative. This is because
// common handlers have a owning pointer to a Future_storage, and each byte
// saved increases the likelyhood that it will fit in SBO, which has very large
// performance impact.
template <typename T>
struct Storage_ptr {
  Storage_ptr() = default;
  Storage_ptr(T* val) : ptr_(val) {
    assert(val);
    inc();
  }

  Storage_ptr(const Storage_ptr& rhs) : ptr_(rhs.ptr_) {
    if (ptr_) {
      inc();
    }
  }

  Storage_ptr(Storage_ptr&& rhs) : ptr_(rhs.ptr_) { rhs.ptr_ = nullptr; }

  Storage_ptr& operator=(const Storage_ptr& rhs) {
    ptr_ = rhs.ptr_;
    if (ptr_) {
      inc();
    }
  }

  Storage_ptr& operator=(Storage_ptr&& rhs) {
    clear();

    ptr_ = rhs.ptr_;
    rhs.ptr_ = nullptr;

    return *this;
  }

  void clear() {
    if (ptr_) {
      if (ptr_->ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        using alloc_traits = std::allocator_traits<typename T::allocator_type>;
        using Alloc = typename alloc_traits::template rebind_alloc<T>;

        Alloc real_alloc(ptr_->allocator());
        ptr_->~T();
        real_alloc.deallocate(ptr_, 1);
      }
    }
  }

  void reset() {
    clear();
    ptr_ = nullptr;
  }

  operator bool() const { return ptr_ != nullptr; }

  ~Storage_ptr() { clear(); }

  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }

  void allocate(typename T::allocator_type const& alloc) {
    clear();
    using alloc_traits = std::allocator_traits<typename T::allocator_type>;
    using Alloc = typename alloc_traits::template rebind_alloc<T>;

    Alloc real_alloc(alloc);
    T* new_ptr = real_alloc.allocate(1);
    try {
      ptr_ = new (new_ptr) T(alloc);
      inc();
    } catch (...) {
      real_alloc.deallocate(new_ptr, 1);
    }
  }

private:
  void inc() {
    ptr_->ref_count_.fetch_add(1, std::memory_order_relaxed);
  }

  T* ptr_ = nullptr;
};

template <typename Alloc, typename T>
struct Storage_for_cb_result {
  using type = Future_storage<Alloc, decay_future_t<T>>;
};

template <typename Alloc, typename T>
struct Storage_for_cb_result<Alloc, expected<T>> {
  using type = typename Storage_for_cb_result<Alloc, T>::type;
};

template <typename Alloc, typename... Us>
struct Storage_for_cb_result<Alloc, Segmented_callback_result<Us...>> {
  using type = Future_storage<Alloc, decay_future_t<Us>...>;
};

template <typename Alloc, typename T>
using Storage_for_cb_result_t = typename Storage_for_cb_result<Alloc, T>::type;

}  // namespace detail
}  // namespace aom
#endif