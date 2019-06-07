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
void Future_storage<Ts...>::fullfill(fullfill_type&& v) {
  std::lock_guard l(mtx_);
  assert(state_ == State::READY || state_ == State::PENDING);
  if (state_ == State::PENDING) {
    state_ = State::FULLFILLED;
    new (&fullfilled_) fullfill_type(std::move(v));
  } else {
    callbacks_->fullfill(std::move(v));
  }
}

template <typename... Ts>
void Future_storage<Ts...>::finish(finish_type&& f) {
  std::lock_guard l(mtx_);
  assert(state_ == State::READY || state_ == State::PENDING);
  if (state_ == State::PENDING) {
    state_ = State::FINISHED;
    new (&finished_) finish_type(std::move(f));
  } else {
    callbacks_->finish(std::move(f));
  }
}

template <typename... Ts>
void Future_storage<Ts...>::fail(fail_type&& e) {
  std::lock_guard l(mtx_);
  assert(state_ == State::READY || state_ == State::PENDING);

  if (state_ == State::PENDING) {
    state_ = State::ERROR;
    new (&failure_) fail_type(std::move(e));
  } else {
    callbacks_->fail(e);
  }
}

template <typename... Ts>
template <typename Handler_t, typename QueueT, typename... Args_t>
void Future_storage<Ts...>::set_handler(QueueT* queue, Args_t&&... args) {
  std::lock_guard l(mtx_);
  assert(state_ != State::READY);

  if (state_ == State::PENDING) {
    state_ = State::READY;

    new (&callbacks_) Future_handler_iface<Ts...>*();
    callbacks_ = new Handler_t(queue, std::forward<Args_t>(args)...);
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
      delete callbacks_;
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