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

#ifndef AOM_VARIADIC_IMPL_STORAGE_IMPL_INCLUDED_H
#define AOM_VARIADIC_IMPL_STORAGE_IMPL_INCLUDED_H

#include "var_future/config.h"

#include "var_future/impl/storage_decl.h"
#include "var_future/impl/utils.h"

#include <type_traits>

namespace aom {

namespace detail {

template <typename Alloc, typename... Ts>
Future_storage<Alloc, Ts...>::Future_storage(const Alloc& alloc)
    : Alloc(alloc) {}

template <typename Alloc, typename... Ts>
void Future_storage<Alloc, Ts...>::fullfill(fullfill_type&& v) {
  auto prev_state = state_.load();

  if (prev_state & Future_storage_state_ready_bit) {
    cb_data_.callback_->fullfill(std::move(v));
  } else {
    // This is expected to be fairly rare...
    new (&finished_)
        finish_type(fullfill_to_finish<0, 0, finish_type>(std::move(v)));
    prev_state = state_.fetch_or(Future_storage_state_finished_bit);

    // Handle the case where a handler was added just in time.
    // This should be extremely rare.
    if (prev_state & Future_storage_state_ready_bit) {
      cb_data_.callback_->finish(std::move(finished_));
    }
  }
}

template <typename Alloc, typename... Ts>
template <typename... Us>
void Future_storage<Alloc, Ts...>::fullfill(
    Segmented_callback_result<Us...>&& f) {
  fullfill(std::move(f.values_));
}

template <typename Alloc, typename... Ts>
template <typename Arg_alloc>
void Future_storage<Alloc, Ts...>::fullfill(
    Basic_future<Arg_alloc, Ts...>&& f) {
  Storage_ptr<Future_storage<Alloc, Ts...>> self(this);
  f.finally([self](expected<Ts>... f) {
    self->finish(std::make_tuple(std::move(f)...));
  });
}

template <typename Alloc, typename... Ts>
void Future_storage<Alloc, Ts...>::finish(finish_type&& f) {
  auto prev_state = state_.load();

  if (prev_state & Future_storage_state_ready_bit) {
    // THis should be the likelyest scenario.
    cb_data_.callback_->finish(std::move(f));
    // No need to set the finished bit.
  } else {
    // This is expected to be fairly rare...
    new (&finished_) finish_type(std::move(f));
    prev_state = state_.fetch_or(Future_storage_state_finished_bit);

    // Handle the case where a handler was added just in time.
    // This should be extremely rare.
    if (prev_state & Future_storage_state_ready_bit) {
      cb_data_.callback_->finish(std::move(finished_));
    }
  }
}

template <typename Alloc, typename... Ts>
template <typename Arg_alloc>
void Future_storage<Alloc, Ts...>::finish(Basic_future<Arg_alloc, Ts...>&& f) {
  f.finally([this](expected<Ts>... f) {
    this->finish(std::make_tuple(std::move(f)...));
  });
}

template <typename Alloc, typename... Ts>
void Future_storage<Alloc, Ts...>::fail(fail_type&& e) {
  auto prev_state = state_.load();

  if (prev_state & Future_storage_state_ready_bit) {
    cb_data_.callback_->finish(
        fail_to_expect<0, std::tuple<expected<Ts>...>>(e));
  } else {
    // This is expected to be fairly rare...
    new (&finished_) finish_type(fail_to_expect<0, finish_type>(e));
    prev_state = state_.fetch_or(Future_storage_state_finished_bit);

    // Handle the case where a handler was added just in time.
    // This should be extremely rare.
    if (prev_state & Future_storage_state_ready_bit) {
      cb_data_.callback_->finish(
          fail_to_expect<0, std::tuple<expected<Ts>...>>(e));
    }
  }
}

template <typename Alloc, typename... Ts>
template <typename Handler_t, typename QueueT, typename... Args_t>
void Future_storage<Alloc, Ts...>::set_handler(QueueT* queue,
                                               Args_t&&... args) {
  assert(cb_data_.callback_ == nullptr);

  using alloc_traits = std::allocator_traits<Alloc>;
  using Real_alloc = typename alloc_traits::template rebind_alloc<Handler_t>;

  Real_alloc real_alloc(allocator());
  auto ptr = real_alloc.allocate(1);
  try {
    cb_data_.callback_ =
        new (ptr) Handler_t(queue, std::forward<Args_t>(args)...);
  } catch (...) {
    real_alloc.deallocate(ptr, 1);
    throw;
  }

  auto prev_state = state_.fetch_or(Future_storage_state_ready_bit);
  if ((prev_state & Future_storage_state_finished_bit) != 0) {
    // This is unlikely...
    cb_data_.callback_->finish(std::move(finished_));
  }
}

template <typename Alloc, typename... Ts>
Future_storage<Alloc, Ts...>::~Future_storage() {
  auto state = state_.load();

  if (state & Future_storage_state_ready_bit) {
    assert(cb_data_.callback_ != nullptr);

    cb_data_.callback_->~Future_handler_iface<Ts...>();
    using alloc_traits = std::allocator_traits<Alloc>;
    using Real_alloc = typename alloc_traits::template rebind_alloc<
        Future_handler_iface<Ts...>>;

    Real_alloc real_alloc(allocator());
    real_alloc.deallocate(cb_data_.callback_, 1);
  }

  if (state & Future_storage_state_finished_bit) {
    finished_.~finish_type();
  }
}

}  // namespace detail
}  // namespace aom
#endif