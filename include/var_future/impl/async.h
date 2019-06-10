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

#ifndef AOM_VARIADIC_IMPL_ASYNC_INCLUDED_H
#define AOM_VARIADIC_IMPL_ASYNC_INCLUDED_H

#include "var_future/config.h"

namespace aom {
  template<typename CbT, typename QueueT>
  auto async(QueueT& q, CbT&& cb) {
    using cb_result_type = decltype(cb());

    using dst_storage_type = detail::Storage_for_cb_result_t<cb_result_type>;
    using result_fut_t = typename dst_storage_type::future_type;

    detail::Storage_ptr<dst_storage_type> res(new dst_storage_type());

    detail::enqueue(&q, [cb = std::move(cb), res] {
      try {
        if constexpr (std::is_same_v<void, cb_result_type>) {
          cb();
          res->fullfill();
        }
        else {
          res->fullfill(cb());
        }
      }
      catch(...) {
        res->fail(std::current_exception());
      }
    });
    
    return result_fut_t{res};
  }
}  // namespace aom
#endif



