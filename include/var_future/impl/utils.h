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

#ifndef AOM_VARIADIC_IMPL_UTILS_INCLUDED_H
#define AOM_VARIADIC_IMPL_UTILS_INCLUDED_H

#include "var_future/config.h"

#include <optional>

namespace aom {

template <typename... Ts>
class Future;

// Determines wether a type is a Future<...>
template <typename T>
struct is_future : public std::false_type {};

template <typename... Ts>
struct is_future<Future<Ts...>> : public std::true_type {};

template <typename T>
constexpr bool is_future_v = is_future<T>::value;

namespace detail {

// Determines wether a type is a expected<...>
template <typename T>
struct is_expected : public std::false_type {};

template <typename T>
struct is_expected<expected<T>> : public std::true_type {};

template <typename T>
constexpr bool is_expected_v = is_expected<T>::value;

// Function: get_first_error()
// Returns the first error in a set of expected<>, if any
template <typename... Ts>
struct get_first_error_impl {
  static std::optional<std::exception_ptr> exec() { return std::nullopt; }
};

template <typename T, typename... Ts>
struct get_first_error_impl<T, Ts...> {
  static std::optional<std::exception_ptr> exec(const expected<T>& first,
                                                const expected<Ts>&... rest) {
    if (!first.has_value()) {
      return first.error();
    }
    return get_first_error_impl<Ts...>::exec(rest...);
  }
};

template <typename... Ts>
std::optional<std::exception_ptr> get_first_error(const expected<Ts>&... vals) {
  return get_first_error_impl<Ts...>::exec(vals...);
}

// Converts a Future<T> into T
template <typename T>
struct decay_future {
  using type = T;
};

template <typename T>
struct decay_future<Future<T>> {
  using type = T;
};

template <typename T>
using decay_future_t = typename decay_future<T>::type;

// Concatenates Two tuple types
template <typename LhsT, typename RhsT>
using tuple_cat_t =
    decltype(std::tuple_cat(std::declval<LhsT>(), std::declval<RhsT>()));


template<typename... Ts>
struct future_value_type;

template<typename T, typename... Ts>
struct future_value_type<T, Ts...> {
  using type = T;
};

template<typename T, typename U, typename... Ts>
struct future_value_type<T, U, Ts...> {
  using type = std::tuple<T, U, Ts...>;
};

template <typename... Ts>
using future_value_type_t = typename future_value_type<Ts...>::type;


// Determines the fullfillment type of a Future<Ts...>
template <typename... Ts>
struct fullfill_type {
  using type = std::tuple<>;
};

template <typename... Ts>
struct fullfill_type<void, Ts...> {
  using type = typename fullfill_type<Ts...>::type;
};

template <typename T, typename... Ts>
struct fullfill_type<T, Ts...> {
  using lhs_t = std::tuple<T>;
  using rhs_t = typename fullfill_type<Ts...>::type;

  using type = tuple_cat_t<lhs_t, rhs_t>;
};

// Determines the fullfillment type of a Future<Ts...>
template <typename... Ts>
using fullfill_type_t = typename fullfill_type<Ts...>::type;

// Determines the finishing type of a Future<Ts...>
template <typename... Ts>
using finish_type_t = std::tuple<expected<Ts>...>;

// Determines the hard failure type of a Future<Ts...>
template <typename... Ts>
using fail_type_t = std::exception_ptr;

template <std::size_t i, std::size_t j, typename... Ts, typename... Us>
void finish_to_fullfill(std::tuple<Ts...>&& src, std::tuple<Us...>& dst) {
  if constexpr (j >= sizeof...(Us)) {
    return;
  } else {
    using src_type = std::tuple_element_t<i, std::tuple<Ts...>>;
    if constexpr (std::is_same_v<expected<void>, src_type>) {
      finish_to_fullfill<i + 1, j>(std::move(src), dst);
    } else {
      assert(std::get<i>(src).has_value());
      std::get<j>(dst) = std::move(*std::get<i>(src));
      finish_to_fullfill<i + 1, j + 1>(std::move(src), dst);
    }
  }
}

template <std::size_t i, std::size_t j, typename... Ts, typename... Us>
void fullfill_to_finish(std::tuple<Ts...>&& src, std::tuple<Us...>& dst) {
  static_assert(sizeof...(Ts) <= sizeof...(Us));

  if constexpr (j >= sizeof...(Us)) {
    return;
  } else {
    using dst_type = std::tuple_element_t<j, std::tuple<Us...>>;
    if constexpr (std::is_same_v<expected<void>, dst_type>) {
      std::get<j>(dst) = {};
      fullfill_to_finish<i, j + 1>(std::move(src), dst);
    } else {
      std::get<j>(dst) = std::move(std::get<i>(src));
      fullfill_to_finish<i + 1, j + 1>(std::move(src), dst);
    }
  }
}

template <std::size_t i, typename... Ts>
void fail_to_expect(const std::exception_ptr& src, std::tuple<Ts...>& dst) {
  if constexpr (i >= sizeof...(Ts)) {
    return;
  } else {
    std::get<i>(dst) = unexpected{src};
  }
}

// A special Immediate queue tag type
struct Immediate_queue {
  template <typename F>
  static void push(F&& f) {
    f();
  }
};

static void no_op_test() {}
// Determines wether T's duck-typed push() method is static.
// This will be used in Future_handler_base to omit its instantiation
template <typename T, typename = void>
struct has_static_push : std::false_type {};

template <typename T>
struct has_static_push<T, decltype(void(T::push(no_op_test)))>
    : std::true_type {};

template <typename T>
constexpr bool has_static_push_v = has_static_push<T>::value;

// enqueue(), duck-type push f into q.
// If Q has a static_push method, then q is ignored.
template <typename Q, typename F>
std::enable_if_t<!has_static_push_v<Q>> enqueue(Q* q, F&& f) {
  q->push(std::forward<F>(f));
}

template <typename Q, typename F>
std::enable_if_t<has_static_push_v<Q>> enqueue(Q*, F&& f) {
  Q::push(std::forward<F>(f));
}

}  // namespace detail
}  // namespace aom
#endif
