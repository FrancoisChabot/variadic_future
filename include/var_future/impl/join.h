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

#ifndef AOM_VARIADIC_IMPL_TIE_INCLUDED_H
#define AOM_VARIADIC_IMPL_TIE_INCLUDED_H

#include "var_future/config.h"

#include "var_future/impl/utils.h"

namespace aom {

namespace detail {

template <typename Alloc, typename... FutTs>
struct Landing {
  std::tuple<expected<decay_future_t<FutTs>>...> landing_;
  std::atomic<int> fullfilled_ = 0;

  using storage_type = Future_storage<Alloc, decay_future_t<FutTs>...>;
  Storage_ptr<storage_type> dst_;

  void ping() {
    if (++fullfilled_ == sizeof...(FutTs)) {
      dst_->finish(std::move(landing_));
    }
  }
};

// Used by join to listen to the individual futures.
template <std::size_t id, typename LandingT, typename... FutTs>
void bind_landing(const std::shared_ptr<LandingT>&, FutTs&&...) {}

template <std::size_t id, typename LandingT, typename Front, typename... FutTs>
void bind_landing(const std::shared_ptr<LandingT>& l, Future<Front>&& front,
                  FutTs&&... futs) {
// This is technically UB by omission in the standard,
// but is 100% fine in all known compilers right now.
// See: https://stackoverflow.com/questions/56497862/
#ifndef VAR_FUTURE_NO_UB
  front.finally([l](expected<Front>&& e) {
    std::get<id>(l->landing_) = std::move(e);
    l->ping();
  });
#else
  auto value_landing = &std::get<id>(l->landing_);
  front.finally([=](expected<Front>&& e) {
    *value_landing = std::move(e);
    l->ping();
  });
#endif

  bind_landing<id + 1>(l, std::forward<FutTs>(futs)...);
}

}  // namespace detail

template <typename FirstT, typename... FutTs>
auto join(FirstT&& first, FutTs&&... futs) {
  static_assert(sizeof...(FutTs) >= 1, "Trying to join less than two futures?");
  static_assert(is_future_v<FirstT>);
  static_assert(std::conjunction_v<is_future<FutTs>...>,
                "trying to join a non-future");

  using landing_type =
      detail::Landing<typename FirstT::allocator_type, std::decay_t<FirstT>,
                      std::decay_t<FutTs>...>;
  using fut_type = typename landing_type::storage_type::future_type;

  auto landing = std::make_shared<landing_type>();
  landing->dst_.allocate(first.allocator());
  landing->dst_->bind();

  detail::bind_landing<0>(landing, std::forward<FirstT>(first),
                          std::forward<FutTs>(futs)...);

  return fut_type{landing->dst_};
}
}  // namespace aom
#endif