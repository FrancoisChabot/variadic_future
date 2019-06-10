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

template <typename... Ts>
Promise<Ts...>::~Promise() {
  if (storage_) {
    storage_->fail(std::make_exception_ptr(Unfullfilled_promise{}));
  }
}

template <typename... Ts>
typename Promise<Ts...>::future_type Promise<Ts...>::get_future() {
  assert(!storage_);

  storage_ = detail::Storage_ptr<storage_type>(new storage_type());

  return future_type{storage_};
}

template <typename... Ts>
template <typename... Us>
void Promise<Ts...>::finish(Us&&... f) {
  assert(storage_);

  storage_->finish(std::make_tuple(std::forward<Us>(f)...));
  storage_.reset();
}

template <typename... Ts>
template <typename... Us>
void Promise<Ts...>::set_value(Us&&... vals) {
  assert(storage_);

  storage_->fullfill(std::make_tuple(std::forward<Us>(vals)...));
  storage_.reset();
}

template <typename... Ts>
void Promise<Ts...>::set_exception(fail_type e) {
  assert(storage_);

  storage_->fail(std::move(e));
  storage_.reset();
}
}  // namespace aom
#endif