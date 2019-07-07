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

#ifndef AOM_VARIADIC_IMPL_STREAM_STORAGE_IMPL_INCLUDED_H
#define AOM_VARIADIC_IMPL_STREAM_STORAGE_IMPL_INCLUDED_H

#include "var_future/config.h"

#include "var_future/impl/stream/stream_storage_decl.h"
#include "var_future/impl/utils.h"

#include <type_traits>

namespace aom {

namespace detail {

template <typename Alloc, typename... Ts>
Stream_storage<Alloc, Ts...>::~Stream_storage() {
  switch (state_) {
    case Stream_storage_state::READY:
      cb_data_.callback_->~Stream_handler_iface<Ts...>();
      {
        using alloc_traits = std::allocator_traits<Alloc>;
        using Real_alloc = typename alloc_traits::template rebind_alloc<
            Stream_handler_iface<Ts...>>;

        Real_alloc real_alloc(allocator());
        real_alloc.deallocate(cb_data_.callback_, 1);
      }
      cb_data_.~Callback_data();
      break;
    case Stream_storage_state::READY_SBO:
      cb_data_.callback_->~Stream_handler_iface<Ts...>();
      cb_data_.~Callback_data();
      break;
    case Stream_storage_state::FULLFILLED:
      fullfilled_.~fullfill_buffer_type();
      break;
    case Stream_storage_state::ERROR:
      failure_.~fail_type();
      break;
    case Stream_storage_state::COMPLETED:
      break;
    default:
      assert(false);
  }
}

template <typename Alloc, typename... Ts>
Stream_storage<Alloc, Ts...>::Stream_storage(const Alloc& alloc)
    : Alloc(alloc)
    , final_promise_(alloc) {}

template <typename Alloc, typename... Ts>
void Stream_storage<Alloc, Ts...>::fail(fail_type&& e) {
  if (state_ == Stream_storage_state::PENDING) {
    new (&failure_) fail_type(std::move(e));
  }
}

template <typename Alloc, typename... Ts>
template <typename... Us>
void Stream_storage<Alloc, Ts...>::push(Us&&... args) {
  std::lock_guard l(mtx_);
  switch (state_) {
    case Stream_storage_state::PENDING:
      new (&fullfilled_) fullfill_buffer_type();
      fullfilled_.push_back(std::move(args)...);
      state_ = Stream_storage_state::FULLFILLED;
      break;
    case Stream_storage_state::FULLFILLED:
      fullfilled_.push_back(std::move(args)...);
      break;
    case Stream_storage_state::COMPLETED:
    case Stream_storage_state::ERROR:
      assert(false);
      break;
    case Stream_storage_state::READY:
    case Stream_storage_state::READY_SBO:
      cb_data_.callback_->push(std::forward<Us>(args)...);
      break;
  }
}

template <typename Alloc, typename... Ts>
void Stream_storage<Alloc, Ts...>::complete() {
  if (state_ == Stream_storage_state::PENDING) {
    state_ = Stream_storage_state::COMPLETED;
  }

  final_promise_.set_value();
}

template <typename Alloc, typename... Ts>
template <typename Handler_t, typename QueueT, typename... Args_t>
void Stream_storage<Alloc, Ts...>::install_handler_(QueueT* queue,
                                                    Args_t&&... args) {
  new (&cb_data_) Callback_data();

  if constexpr (sizeof(Handler_t) <= sbo_space) {
    state_ = Stream_storage_state::READY_SBO;
    void* ptr = &cb_data_.sbo_buffer_;
    cb_data_.callback_ =
        new (ptr) Handler_t(queue, std::forward<Args_t>(args)...);
  } else {
    using alloc_traits = std::allocator_traits<Alloc>;
    using Real_alloc = typename alloc_traits::template rebind_alloc<Handler_t>;

    Real_alloc real_alloc(allocator());
    auto ptr = real_alloc.allocate(1);
    try {
      cb_data_.callback_ =
          new (ptr) Handler_t(queue, std::forward<Args_t>(args)...);
      state_ = Stream_storage_state::READY;
    } catch (...) {
      real_alloc.deallocate(ptr, 1);
      throw;
    }
  }
}
template <typename Alloc, typename... Ts>
template <typename Handler_t, typename QueueT, typename... Args_t>
void Stream_storage<Alloc, Ts...>::set_handler(QueueT* queue,
                                               Args_t&&... args) {
  std::lock_guard l(mtx_);
  switch (state_) {
    case Stream_storage_state::FULLFILLED:
      for (auto& f : fullfilled_) {
        Handler_t::do_push(queue, &args..., std::move(f));
      }
      fullfilled_.~fullfill_buffer_type();
      install_handler_<Handler_t>(queue, std::move(args)...);
      break;
    case Stream_storage_state::PENDING:
      install_handler_<Handler_t>(queue, std::move(args)...);
      break;

    case Stream_storage_state::ERROR:
      final_promise_.set_exception(failure_);
      failure_.~fail_type();
      break;
    case Stream_storage_state::COMPLETED:
      break;
    default:
      assert(false);
  }
}

}  // namespace detail
}  // namespace aom
#endif