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
  if (cb_data_.callback_) {
    cb_data_.callback_->~Stream_handler_iface<Ts...>();
    using alloc_traits = std::allocator_traits<Alloc>;
    using Real_alloc = typename alloc_traits::template rebind_alloc<
        Stream_handler_iface<Ts...>>;

    Real_alloc real_alloc(allocator());
    real_alloc.deallocate(cb_data_.callback_, 1);
  }
}

template <typename Alloc, typename... Ts>
Stream_storage<Alloc, Ts...>::Stream_storage(const Alloc& alloc)
    : Alloc(alloc) {
  final_promise_.allocate(alloc);
}

template <typename Alloc, typename... Ts>
template <typename... Us>
void Stream_storage<Alloc, Ts...>::push(Us&&... args) {
  auto flags = state_.load();

  assert((flags & (Stream_storage_state_fail_bit |
                   Stream_storage_state_complete_bit)) == 0);

  if (flags & Stream_storage_state_ready_bit) {
    // This is supposed to be by far and wide the most common case.
    cb_data_.callback_->push(std::forward<Us>(args)...);
  } else {
    std::unique_lock l(mtx_);
    flags = state_.load();

    if (flags & Stream_storage_state_ready_bit) {
      // This is extremely unlikely.
      l.unlock();
      cb_data_.callback_->push(std::forward<Us>(args)...);
    } else {
      fullfilled_.push_back(std::move(args)...);
    }
  }
}

template <typename Alloc, typename... Ts>
void Stream_storage<Alloc, Ts...>::complete() {
  std::unique_lock l(mtx_);
  auto flags = state_.load();

  if (flags & Stream_storage_state_ready_bit) {
    l.unlock();
    cb_data_.callback_->complete();
  } else {
    state_.fetch_or(Stream_storage_state_complete_bit);
  }
}

template <typename Alloc, typename... Ts>
void Stream_storage<Alloc, Ts...>::fail(fail_type&& e) {
  std::unique_lock l(mtx_);
  auto flags = state_.load();

  if (flags & Stream_storage_state_ready_bit) {
    l.unlock();
    cb_data_.callback_->fail(std::move(e));
  } else {
    error_ = std::move(e);
    state_.fetch_or(Stream_storage_state_fail_bit);
  }
}

template <typename Alloc, typename... Ts>
template <typename Handler_t, typename QueueT, typename... Args_t>
void Stream_storage<Alloc, Ts...>::set_handler(QueueT* queue,
                                               Args_t&&... args) {
  using alloc_traits = std::allocator_traits<Alloc>;
  using Real_alloc = typename alloc_traits::template rebind_alloc<Handler_t>;

  Handler_t* new_handler = nullptr;

  Real_alloc real_alloc(allocator());
  auto ptr = real_alloc.allocate(1);
  try {
    new_handler = new (ptr)
        Handler_t(final_promise_, queue, std::forward<Args_t>(args)...);
  } catch (...) {
    real_alloc.deallocate(ptr, 1);
    throw;
  }

  cb_data_.callback_ = new_handler;

  std::unique_lock l(mtx_);
  for (auto& v : fullfilled_) {
    Handler_t::do_push(queue, new_handler->cb_, std::move(v));
  }

  auto flags = state_.fetch_or(Stream_storage_state_ready_bit);
  l.unlock();

  if ((flags & Stream_storage_state_complete_bit) != 0) {
    cb_data_.callback_->complete();
  } else if ((flags & Stream_storage_state_fail_bit) != 0) {
    cb_data_.callback_->fail(std::move(error_));
  }
}

}  // namespace detail
}  // namespace aom
#endif