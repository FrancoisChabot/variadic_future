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

#ifndef AOM_VARIADIC_IMPL_STREAM_PROMISE_INCLUDED_H
#define AOM_VARIADIC_IMPL_STREAM_PROMISE_INCLUDED_H

#include "var_future/config.h"

namespace aom {

template <typename Alloc, typename... Ts>
Basic_stream_promise<Alloc, Ts...>::Basic_stream_promise() {}

template <typename Alloc, typename... Ts>
Basic_stream_promise<Alloc, Ts...>::~Basic_stream_promise() {
  if (storage_) {
    storage_->fail(std::make_exception_ptr(Unfullfilled_promise{}));
  }
}

template <typename Alloc, typename... Ts>
typename Basic_stream_promise<Alloc, Ts...>::future_type
Basic_stream_promise<Alloc, Ts...>::get_future(const Alloc& alloc) {
  storage_.allocate(alloc);

  return future_type{storage_};
}

template <typename Alloc, typename... Ts>
template <typename... Us>
void Basic_stream_promise<Alloc, Ts...>::push(Us&&... vals) {
  assert(storage_);
  storage_->push(std::forward<Us>(vals)...);
}

template <typename Alloc, typename... Ts>
void Basic_stream_promise<Alloc, Ts...>::complete() {
  assert(storage_);
  storage_->complete();

  storage_.reset();
}

template <typename Alloc, typename... Ts>
void Basic_stream_promise<Alloc, Ts...>::set_exception(fail_type e) {
  assert(storage_);
  
  storage_->fail(std::move(e));
  storage_.reset();
}

template <typename Alloc, typename... Ts>
Basic_stream_promise<Alloc, Ts...>::operator bool() const {
  return storage_;
}
}  // namespace aom
#endif