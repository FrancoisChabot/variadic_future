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

#ifndef AOM_VARIADIC_IMPL_STREAM_FUTURE_INCLUDED_H
#define AOM_VARIADIC_IMPL_STREAM_FUTURE_INCLUDED_H

#include "var_future/config.h"

#include "var_future/impl/stream/foreach_handler.h"

#include <future>

namespace aom {

template <typename Alloc, typename... Ts>
Basic_stream_future<Alloc, Ts...>::Basic_stream_future(
    detail::Storage_ptr<storage_type> s)
    : storage_(std::move(s)) {}

template <typename Alloc, typename... Ts>
template <typename CbT>
Basic_future<Alloc, void> Basic_stream_future<Alloc, Ts...>::for_each(
    CbT&& cb) {
  detail::Immediate_queue queue;
  return this->for_each(queue, std::forward<CbT>(cb));
}

template <typename Alloc, typename... Ts>
template <typename QueueT, typename CbT>
[[nodiscard]] Basic_future<Alloc, void>
Basic_stream_future<Alloc, Ts...>::for_each(QueueT& queue, CbT&& cb) {
  assert(storage_);
  static_assert(std::is_invocable_v<CbT, Ts...>,
                "for_each should be accepting the correct arguments");

  using handler_t =
      detail::Future_stream_foreach_handler<Alloc, std::decay_t<CbT>, QueueT, Ts...>;

  // This must be done BEFORE set_handler
  auto result_fut = storage_->get_final_future();

  storage_->template set_handler<handler_t>(&queue, std::move(cb));
  storage_.reset();

  return result_fut;
}

}  // namespace aom
#endif