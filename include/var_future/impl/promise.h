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

#ifndef AOM_VARIADIC_IMPL_PROMISE_INCLUDED_H
#define AOM_VARIADIC_IMPL_PROMISE_INCLUDED_H

#include "var_future/config.h"

namespace aom {

template <typename Alloc, typename... Ts>
Basic_promise<Alloc, Ts...>::Basic_promise(const Alloc& alloc) {
  storage_.allocate(alloc);
}

template <typename Alloc, typename... Ts>
Basic_promise<Alloc, Ts...>::~Basic_promise() {
  if (storage_ && !value_assigned_) {
    storage_->fail(std::make_exception_ptr(Unfullfilled_promise{}));
  }
}

template <typename Alloc, typename... Ts>
typename Basic_promise<Alloc, Ts...>::future_type
Basic_promise<Alloc, Ts...>::get_future() {
  assert(!future_created_);
  future_created_ = true;

  return future_type{storage_};
}

template <typename Alloc, typename... Ts>
template <typename... Us>
void Basic_promise<Alloc, Ts...>::finish(Us&&... f) {
  assert(storage_ && !value_assigned_);

  storage_->finish(std::make_tuple(std::forward<Us>(f)...));
  value_assigned_ = true;

  if (future_created_) storage_.reset();
}

template <typename Alloc, typename... Ts>
template <typename... Us>
void Basic_promise<Alloc, Ts...>::set_value(Us&&... vals) {
  assert(storage_ && !value_assigned_);

  storage_->fullfill(fullfill_type(std::forward<Us>(vals)...));
  value_assigned_ = true;
  if (future_created_) storage_.reset();
}

template <typename Alloc, typename... Ts>
void Basic_promise<Alloc, Ts...>::set_exception(fail_type e) {
  assert(storage_ && !value_assigned_);

  storage_->fail(std::move(e));
  value_assigned_ = true;

  if (future_created_) storage_.reset();
}

template <typename Alloc, typename... Ts>
Basic_promise<Alloc, Ts...>::operator bool() const {
  return storage_;
}
}  // namespace aom
#endif