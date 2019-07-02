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

#ifndef AOM_VARIADIC_IMPL_STREAM_STORAGE_DECL_INCLUDED_H
#define AOM_VARIADIC_IMPL_STREAM_STORAGE_DECL_INCLUDED_H

#include "var_future/config.h"

namespace aom {

namespace detail {

template <typename... Ts>
class Stream_handler_iface {
 public:
  using fullfill_type = fullfill_type_t<Ts...>;
  using fail_type = fail_type_t<Ts...>;
  using finish_type = finish_type_t<Ts...>;

  Stream_handler_iface() = default;
  Stream_handler_iface(const Stream_handler_iface&) = delete;
  Stream_handler_iface(Stream_handler_iface&&) = delete;
  Stream_handler_iface& operator=(const Stream_handler_iface&) = delete;
  Stream_handler_iface& operator=(Stream_handler_iface&&) = delete;
  virtual ~Stream_handler_iface() {}

  // The future has been completed
  virtual void push(Ts...) = 0;
};

template <typename QueueT, typename Enable = void, typename... Ts>
class Stream_handler_base : public Stream_handler_iface<Ts...> {
 public:
  Stream_handler_base(QueueT* q) : queue_(q) {}

 protected:
  QueueT* get_queue() { return queue_; }

 private:
  QueueT* queue_;
};

// If the queue's push() method is static, do not bother storing the pointer.
template <typename QueueT, typename... Ts>
class Stream_handler_base<QueueT, std::enable_if_t<has_static_push_v<QueueT>>,
                          Ts...> : public Stream_handler_iface<Ts...> {
 public:
  Stream_handler_base(QueueT*) {}

 protected:
  constexpr static QueueT* get_queue() { return nullptr; }
};

enum class Stream_storage_state {
  PENDING,     // The stream has neither value, nor callback.
  READY,       // The stream has a callback attached to it.
  READY_SBO,   // The stream has a callback stored in SBO attached to it.
  FULLFILLED,  // The stream has a value, but no callback.
  ERROR,       // The stream has an error, but no callback.
  COMPLETED,   // The stream has closed, either successfully, or with an error.
};

template <typename Alloc, typename... Ts>
class Stream_storage : public Alloc {
  Stream_storage(const Alloc& alloc);

 public:
  ~Stream_storage();
  using fail_type = std::exception_ptr;
  using fullfill_type = std::tuple<Ts...>;
  using fullfill_buffer_type = std::vector<fullfill_type>;
  using allocator_type = Alloc;

  void fail(fail_type&& e);

  template <typename... Us>
  void push(Us&&...);
  void complete();

  template <typename Handler_t, typename QueueT, typename... Args_t>
  void set_handler(QueueT* queue, Args_t&&... args);

  Alloc& allocator() { return *static_cast<Alloc*>(this); }
  const Alloc& allocator() const { return *static_cast<const Alloc*>(this); }

  Basic_future<Alloc, void> get_final_future() {
    return final_promise_.get_future(allocator());
  }

 private:
  template <typename Handler_t, typename QueueT, typename... Args_t>
  void install_handler_(QueueT* queue, Args_t&&... args);

  Stream_storage_state state_ = Stream_storage_state::PENDING;

  static constexpr std::size_t sbo_space = 16;

  struct Callback_data {
    // This will either point to sbo_buffer_, or heap-allocated data, depending
    // on state_.
    Stream_handler_iface<Ts...>* callback_;
    typename std::aligned_storage<sbo_space>::type sbo_buffer_;
  };

  union {
    Callback_data cb_data_;
    fullfill_buffer_type fullfilled_;
    fail_type failure_;
  };

  template <typename T>
  friend struct Storage_ptr;

  std::mutex mtx_;
  Basic_promise<Alloc, void> final_promise_;
  std::atomic<std::uint8_t> ref_count_ = 0;
};
}  // namespace detail
}  // namespace aom
#endif