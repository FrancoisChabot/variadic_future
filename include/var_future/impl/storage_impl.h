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
bool Future_storage<Alloc, Ts...>::is_ready_state(Future_storage_state v) {
  return v == Future_storage_state::READY;
}

template <typename Alloc, typename... Ts>
void Future_storage<Alloc, Ts...>::fullfill(fullfill_type&& v) {
  std::lock_guard l(mtx_);
  assert(is_ready_state(state_) || state_ == Future_storage_state::PENDING);
  if (state_ == Future_storage_state::PENDING) {
    state_ = Future_storage_state::FULLFILLED;
    new (&fullfilled_) fullfill_type(std::move(v));
  } else {
    cb_data_.callback_->fullfill(std::move(v));
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
  f.finally([this](expected<Ts>... f) {
    this->finish(std::make_tuple(std::move(f)...));
  });
}

template <typename Alloc, typename... Ts>
void Future_storage<Alloc, Ts...>::finish(finish_type&& f) {
  std::lock_guard l(mtx_);
  assert(is_ready_state(state_) || state_ == Future_storage_state::PENDING);
  if (state_ == Future_storage_state::PENDING) {
    state_ = Future_storage_state::FINISHED;
    new (&finished_) finish_type(std::move(f));
  } else {
    cb_data_.callback_->finish(std::move(f));
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
  std::lock_guard l(mtx_);
  assert(is_ready_state(state_) || state_ == Future_storage_state::PENDING);

  if (state_ == Future_storage_state::PENDING) {
    state_ = Future_storage_state::ERROR;
    new (&failure_) fail_type(std::move(e));
  } else {
    auto cb_args = fail_to_expect<0, std::tuple<expected<Ts>...>>(e);
    cb_data_.callback_->finish(cb_args);
  }
}

template <typename Alloc, typename... Ts>
template <typename Handler_t, typename QueueT, typename... Args_t>
void Future_storage<Alloc, Ts...>::set_handler(QueueT* queue,
                                               Args_t&&... args) {
  std::lock_guard l(mtx_);
  assert(!is_ready_state(state_));

  if (state_ == Future_storage_state::PENDING) {
    using alloc_traits = std::allocator_traits<Alloc>;
    using Real_alloc =
        typename alloc_traits::template rebind_alloc<Handler_t>;

    Real_alloc real_alloc(allocator());
    auto ptr = real_alloc.allocate(1);
    try {
      cb_data_.callback_ =
          new (ptr) Handler_t(queue, std::forward<Args_t>(args)...);
      state_ = Future_storage_state::READY;
    } catch (...) {
      real_alloc.deallocate(ptr, 1);
      throw;
    }
  } else if (state_ == Future_storage_state::FULLFILLED) {
    Handler_t::do_fullfill(queue, std::move(fullfilled_),
                           std::forward<Args_t>(args)...);
  } else if (state_ == Future_storage_state::FINISHED) {
    Handler_t::do_finish(queue, std::move(finished_),
                         std::forward<Args_t>(args)...);
  } else if (state_ == Future_storage_state::ERROR) {
    Handler_t::do_fail(queue, std::move(failure_),
                       std::forward<Args_t>(args)...);
  }
}

template <typename Alloc, typename... Ts>
Future_storage<Alloc, Ts...>::~Future_storage() {
  assert(state_ != Future_storage_state::PENDING);
  switch (state_) {
    case Future_storage_state::READY:
      cb_data_.callback_->~Future_handler_iface<Ts...>();
      {
        using alloc_traits = std::allocator_traits<Alloc>;
        using Real_alloc = typename alloc_traits::template rebind_alloc<
            Future_handler_iface<Ts...>>;

        Real_alloc real_alloc(allocator());
        real_alloc.deallocate(cb_data_.callback_, 1);
      }
      break;
    case Future_storage_state::FINISHED:
      finished_.~finish_type();
      break;
    case Future_storage_state::FULLFILLED:
      fullfilled_.~fullfill_type();
      break;
    case Future_storage_state::ERROR:
      failure_.~fail_type();
      break;
    case Future_storage_state::PENDING:
      break;
  }
}

}  // namespace detail
}  // namespace aom
#endif