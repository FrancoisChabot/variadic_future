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

template <typename... Ts>
Future_storage<Ts...>::Future_storage() {}

template <typename... Ts>
bool Future_storage<Ts...>::is_ready_state(State v) {
  return v == State::READY || v == State::READY_SOO;
}

template <typename... Ts>
void Future_storage<Ts...>::fullfill(fullfill_type&& v) {
  std::lock_guard l(mtx_);
  assert(is_ready_state(state_) || state_ == State::PENDING);
  if (state_ == State::PENDING) {
    state_ = State::FULLFILLED;
    new (&fullfilled_) fullfill_type(std::move(v));
  } else {
    cb_data_.callback_->fullfill(std::move(v));
  }
}

template <typename... Ts>
void Future_storage<Ts...>::fullfill(future_type&& f) {
  f.finally([this](expected<Ts>... f) {
    this->finish(std::make_tuple(std::move(f)...));
  });
}

template <typename... Ts>
void Future_storage<Ts...>::finish(finish_type&& f) {
  std::lock_guard l(mtx_);
  assert(is_ready_state(state_) || state_ == State::PENDING);
  if (state_ == State::PENDING) {
    state_ = State::FINISHED;
    new (&finished_) finish_type(std::move(f));
  } else {
    cb_data_.callback_->finish(std::move(f));
  }
}

template <typename... Ts>
void Future_storage<Ts...>::finish(future_type&& f) {
  f.finally([this](expected<Ts>... f) {
    this->finish(std::make_tuple(std::move(f)...));
  });
}

template <typename... Ts>
void Future_storage<Ts...>::fail(fail_type&& e) {
  std::lock_guard l(mtx_);
  assert(is_ready_state(state_) || state_ == State::PENDING);

  if (state_ == State::PENDING) {
    state_ = State::ERROR;
    new (&failure_) fail_type(std::move(e));
  } else {
    cb_data_.callback_->fail(e);
  }
}

template <typename... Ts>
template <typename Handler_t, typename QueueT, typename... Args_t>
void Future_storage<Ts...>::set_handler(QueueT* queue, Args_t&&... args) {
  std::lock_guard l(mtx_);
  assert(!is_ready_state(state_));

  if (state_ == State::PENDING) {
    new (&cb_data_) Callback_data();
    if constexpr (sizeof(Handler_t) <= soo_space) {
      state_ = State::READY_SOO;
      cb_data_.callback_ = new (&cb_data_.soo_buffer_)
          Handler_t(queue, std::forward<Args_t>(args)...);
    } else {
      state_ = State::READY;
      cb_data_.callback_ = new Handler_t(queue, std::forward<Args_t>(args)...);
    }
  } else if (state_ == State::FULLFILLED) {
    Handler_t::do_fullfill(queue, std::move(fullfilled_),
                           std::forward<Args_t>(args)...);
  } else if (state_ == State::FINISHED) {
    Handler_t::do_finish(queue, std::move(finished_),
                         std::forward<Args_t>(args)...);
  } else if (state_ == State::ERROR) {
    Handler_t::do_fail(queue, std::move(failure_),
                       std::forward<Args_t>(args)...);
  }
}

template <typename... Ts>
Future_storage<Ts...>::~Future_storage() {
  assert(state_ != State::PENDING);
  switch (state_) {
    case State::READY:
      delete cb_data_.callback_;
      cb_data_.~Callback_data();
      break;
    case State::READY_SOO:
      cb_data_.callback_->~Future_handler_iface<Ts...>();
      cb_data_.~Callback_data();
      break;
    case State::FINISHED:
      finished_.~finish_type();
      break;
    case State::FULLFILLED:
      fullfilled_.~fullfill_type();
      break;
    case State::ERROR:
      failure_.~fail_type();
      break;
    case State::PENDING:
      break;
  }
}

}  // namespace detail
}  // namespace aom
#endif