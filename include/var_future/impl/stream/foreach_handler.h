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

#ifndef AOM_VARIADIC_IMPL_STREAM_FOREACH_INCLUDED_H
#define AOM_VARIADIC_IMPL_STREAM_FOREACH_INCLUDED_H

#include "var_future/config.h"

#include "var_future/impl/stream/stream_storage_decl.h"
#include "var_future/impl/utils.h"

namespace aom {

namespace detail {

// handling for Future::finally()
template <typename Alloc, typename CbT, typename QueueT, typename... Ts>
class Future_stream_foreach_handler
    : public Stream_handler_base<QueueT, void, Ts...> {
  using parent_type = Stream_handler_base<QueueT, void, Ts...>;
  using fail_type = typename parent_type::fail_type;

 public:
  CbT cb_;
  Storage_ptr<Future_storage<Alloc, void>> finalizer_;

  CbT& get_cb() { return &cb_; }

  Future_stream_foreach_handler(Storage_ptr<Future_storage<Alloc, void>> fin,
                                QueueT* q, CbT cb)
      : parent_type(q), cb_(std::move(cb)), finalizer_(std::move(fin)) {}

  void push(Ts... args) override {
    do_push(this->get_queue(), cb_, std::tuple<Ts...>(std::move(args)...));
  }

  static void do_push(QueueT* q, CbT cb, std::tuple<Ts...> args) {
    enqueue(q, [cb = std::move(cb), args = (std::move(args))]() mutable {
      std::apply(cb, std::move(args));
    });
  }

  void complete() override {
    enqueue(this->get_queue(), [fin = std::move(finalizer_)]() {
      fin->fullfill(fullfill_type_t<void>());
    });
  }

  void fail(fail_type f) override {
    enqueue(this->get_queue(),
            [f = std::move(f), fin = std::move(finalizer_)]() mutable {
              fin->fail(std::move(f));
            });
  }
};
}  // namespace detail
}  // namespace aom
#endif