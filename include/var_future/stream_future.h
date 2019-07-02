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

#ifndef AOM_VARIADIC_STREAM_FUTURE_INCLUDED_H
#define AOM_VARIADIC_STREAM_FUTURE_INCLUDED_H

#include "var_future/config.h"

#include "var_future/future.h"

#include "var_future/impl/stream/stream_storage_decl.h"

#include <memory>

namespace aom {

template <typename Alloc, typename... Ts>
class Basic_stream_future {
 public:
  Basic_stream_future() = default;
  Basic_stream_future(Basic_stream_future&&) = default;
  Basic_stream_future& operator=(Basic_stream_future&&) = default;

  /// The underlying storage type.
  using storage_type = detail::Stream_storage<Alloc, Ts...>;

  template <typename CbT>
  [[nodiscard]] Basic_future<Alloc, void> for_each(CbT&& cb);

  template <typename QueueT, typename CbT>
  [[nodiscard]] Basic_future<Alloc, void> for_each(QueueT& queue, CbT&& cb);

  explicit Basic_stream_future(detail::Storage_ptr<storage_type> s);

 private:
  detail::Storage_ptr<storage_type> storage_;
};

template <typename... Ts>
using Stream_future = Basic_stream_future<std::allocator<void>, Ts...>;

template <typename Alloc, typename... Ts>
class Basic_stream_promise {
 public:
  using future_type = Basic_stream_future<Alloc, Ts...>;
  using storage_type = typename future_type::storage_type;
  using fail_type = std::exception_ptr;

  Basic_stream_promise();
  Basic_stream_promise(Basic_stream_promise&&) = default;
  Basic_stream_promise& operator=(Basic_stream_promise&&) = default;
  ~Basic_stream_promise();

  future_type get_future(const Alloc& alloc = Alloc());

  template <typename... Us>
  void push(Us&&...);
  void complete();

  void set_exception(fail_type);

 private:
  detail::Storage_ptr<storage_type> storage_;

  Basic_stream_promise(const Basic_stream_promise&) = delete;
  Basic_stream_promise& operator=(const Basic_stream_promise&) = delete;
};

template <typename... Ts>
using Stream_promise = Basic_stream_promise<std::allocator<void>, Ts...>;
}  // namespace aom

#include "var_future/impl/stream/stream_future.h"
#include "var_future/impl/stream/stream_promise.h"
#include "var_future/impl/stream/stream_storage_impl.h"

#endif
