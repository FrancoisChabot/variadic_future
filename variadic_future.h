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

#ifndef AOM_VARIADIC_FUTURE_INCLUDED_H
#define AOM_VARIADIC_FUTURE_INCLUDED_H

// CHANGE THIS IF YOU WANT TO USE SOME OTHER EXPECTED TYPE
#include "expected_lite.h"

namespace aom {
  template <typename T>
  using expected = nonstd::expected<T, std::exception_ptr>;
  using unexpected = nonstd::unexpected_type<std::exception_ptr>;
}


#include <cassert>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>

namespace aom {

template <typename... Ts>
class Future;

namespace detail {
template <typename... Ts>
std::tuple<Ts...> extract(expected<Ts>... src) {
  return std::make_tuple(std::move(*src)...);
}

template <std::size_t id, typename... Ts>
std::optional<std::exception_ptr> get_first_failure(
    const std::tuple<expected<Ts>...>& ref) {
  if (!std::get<id>(ref).has_value()) {
    return std::get<id>(ref).error();
  }

  if constexpr (id == sizeof...(Ts) - 1) {
    return std::nullopt;
  } else {
    return get_first_failure<id + 1>(ref);
  }
}

template <typename... Ts>
class Future_storage;

template <typename T>
struct is_future : public std::false_type {};

template <typename... Ts>
struct is_future<Future<Ts...>> : public std::true_type {};

template <typename T>
constexpr bool is_future_v = is_future<T>::value;

// The interface to the fully realized future callback handling.

class Immediate_queue {};

template <typename Q>
struct Perform_enqueue {
  template <typename F>
  static void exec(Q* q, F&& f) {
    q->push(std::move(f));
  }
};

template <>
struct Perform_enqueue<Immediate_queue> {
  template <typename F>
  static void exec(Immediate_queue*, F&& f) {
    f();
  }
};

template <typename Q, typename F>
void enqueue(Q* q, F&& f) {
  Perform_enqueue<Q>::exec(q, std::move(f));
}

template <typename... ArgsT>
class Future_handler_iface {
 public:
 Future_handler_iface() = default;
  Future_handler_iface(const Future_handler_iface&) = delete;
  Future_handler_iface(Future_handler_iface&&) = delete;
  Future_handler_iface& operator=(const Future_handler_iface&) = delete;
  Future_handler_iface& operator=(Future_handler_iface&&) = delete;
  virtual ~Future_handler_iface() {}

  virtual void finish(std::tuple<expected<ArgsT>...>) = 0;
  virtual void fullfill(std::tuple<ArgsT...>) = 0;
  virtual void fail(std::exception_ptr) = 0;
};

template <typename QueueT, typename... Ts>
class Future_handler_base : public Future_handler_iface<Ts...> {
  QueueT* queue_;

 public:
  using queue_type = QueueT;

  Future_handler_base(QueueT* q) : queue_(q) {}
  ~Future_handler_base() {}

 protected:
  QueueT* get_queue() { return queue_; }
};

template <typename... Ts>
class Future_handler_base<Immediate_queue, Ts...>
    : public Future_handler_iface<Ts...> {
 public:
  using queue_type = Immediate_queue;

  Future_handler_base(Immediate_queue*) {}

 protected:
  constexpr static Immediate_queue* get_queue() { return nullptr; }
};

template <typename T>
struct Storage_for_cb_result {
  using type = Future_storage<T>;
};

template <>
struct Storage_for_cb_result<void> {
  using type = Future_storage<>;
};

template <typename T>
using Storage_for_cb_result_t = typename Storage_for_cb_result<T>::type;

template <typename CbT, typename QueueT, typename... Ts>
class Future_then_handler : public Future_handler_base<QueueT, Ts...> {
 public:
  using cb_result_type = std::invoke_result_t<CbT, Ts...>;
  using dst_storage_type = Storage_for_cb_result_t<cb_result_type>;
  using dst_type = std::shared_ptr<dst_storage_type>;

  Future_then_handler(QueueT* q, dst_type dst, CbT cb)
      : Future_handler_base<QueueT, Ts...>(q),
        dst_(std::move(dst)),
        cb_(std::move(cb)) {}

  void finish(std::tuple<expected<Ts>...> v) override {
    do_finish(this->get_queue(), std::move(v), std::move(dst_), std::move(cb_));
  }

  void fullfill(std::tuple<Ts...> v) override {
    do_fullfill(this->get_queue(), std::move(v), std::move(dst_),
                std::move(cb_));
  };

  void fail(std::exception_ptr e) override {
    do_fail(this->get_queue(), e, std::move(dst_), std::move(cb_));
  }

  static void do_finish(QueueT* q, std::tuple<expected<Ts>...> v, dst_type dst,
                        CbT cb) {
    auto failure = get_first_failure<0>(v);
    if (!failure) {
      auto extracted = std::apply(extract<Ts...>, std::move(v));
      do_fullfill(q, std::move(extracted), std::move(dst), std::move(cb));
    } else {
      std::exception_ptr e = *failure;
      enqueue(q, [cb = std::move(cb), dst = std::move(dst), e,
                  v = std::move(v)] { dst->fail(e); });
    }
  }

  static void do_fullfill(QueueT* q, std::tuple<Ts...> v, dst_type dst,
                          CbT cb) {
    enqueue(q, [cb = std::move(cb), dst = std::move(dst), v = std::move(v)] {
      try {
        if constexpr (std::is_same_v<void, cb_result_type>) {
          std::apply(cb, std::move(v));
          dst->fullfill({});
        } else {
          dst->fullfill(std::apply(cb, std::move(v)));
        }

      } catch (...) {
        dst->fail(std::current_exception());
      }
    });
  }

  static void do_fail(QueueT* q, std::exception_ptr e, dst_type dst, CbT) {
    enqueue(q, [dst = std::move(dst), e = std::move(e)] { dst->fail(e); });
  }

 private:
  dst_type dst_;
  CbT cb_;
};

template <typename CbT, typename... Ts>
struct Expect_then_cb_resolver {
  using type = std::invoke_result_t<CbT, expected<Ts>...>;
};

template <typename CbT>
struct Expect_then_cb_resolver<CbT> {
  using type = std::invoke_result_t<CbT, expected<void>>;
};

// handling for then_expect()
template <typename CbT, typename QueueT, typename... Ts>
class Future_then_expect_handler : public Future_handler_base<QueueT, Ts...> {
 public:
  using cb_result_type = typename Expect_then_cb_resolver<CbT, Ts...>::type;
  using dst_storage_type = Storage_for_cb_result_t<cb_result_type>;
  using dst_type = std::shared_ptr<dst_storage_type>;

  Future_then_expect_handler(QueueT* q, dst_type dst, CbT cb)
      : Future_handler_base<QueueT, Ts...>(q),
        dst_(std::move(dst)),
        cb_(std::move(cb)) {}

  void finish(std::tuple<expected<Ts>...> v) override {
    do_finish(this->get_queue(), std::move(v), std::move(dst_), std::move(cb_));
  }

  void fullfill(std::tuple<Ts...> v) override {
    do_fullfill(this->get_queue(), std::move(v), std::move(dst_),
                std::move(cb_));
  };

  void fail(std::exception_ptr e) override {
    do_fail(this->get_queue(), e, std::move(dst_), std::move(cb_));
  }

  static void do_finish(QueueT* q, std::tuple<expected<Ts>...> v, dst_type dst,
                        CbT cb) {
    enqueue(q, [v = std::move(v), cb = std::move(cb), dst = std::move(dst)] {
      try {
        if constexpr (std::is_same_v<void, cb_result_type>) {
          if constexpr (sizeof...(Ts) == 0) {
            cb({});
          } else {
            std::apply(cb, std::move(v));
          }
          dst->fullfill({});
        } else {
          if constexpr (sizeof...(Ts) == 0) {
            dst->fullfill(cb({}));
          } else {
            dst->fullfill(std::apply(cb, std::move(v)));
          }
        }

      } catch (...) {
        dst->fail(std::current_exception());
      }
    });
}

  static void do_fullfill(QueueT* q, std::tuple<Ts...> v, dst_type dst,
                          CbT cb) {
    enqueue(q, [v = std::move(v), cb = std::move(cb), dst = std::move(dst)] {
      try {
        if constexpr (std::is_same_v<void, cb_result_type>) {
          if constexpr (sizeof...(Ts) == 0) {
            cb({});
          } else {
            std::apply(cb, std::move(v));
          }
          dst->fullfill({});
        } else {
          if constexpr (sizeof...(Ts) == 0) {
            dst->fullfill(cb({}));
          } else {
            dst->fullfill(std::apply(cb, std::move(v)));
          }
        }

      } catch (...) {
        dst->fail(std::current_exception());
      }
    });
  }

  static void do_fail(QueueT* q, std::exception_ptr e, dst_type dst, CbT cb) {
    enqueue(q, [e = std::move(e), dst = std::move(dst), cb = std::move(cb)] {
      try {
        if constexpr (std::is_same_v<void, cb_result_type>) {
          cb(unexpected{e});
          dst->fullfill({});
        } else {
          dst->fullfill(cb(unexpected{e}));
        }

      } catch (...) {
        dst->fail(std::current_exception());
      }
    });
  }

 private:
  dst_type dst_;
  CbT cb_;
};

template <typename CbT, typename QueueT, typename... Ts>
class Future_then_finally_handler : public Future_handler_base<QueueT, Ts...> {
  CbT cb_;

 public:
  Future_then_finally_handler(QueueT* q, CbT cb)
      : Future_handler_base<QueueT, Ts...>(q), cb_(std::move(cb)) {}

  void finish(std::tuple<expected<Ts>...> v) override {
    do_finish(this->get_queue(), std::move(v), std::move(cb_));
  }

  void fullfill(std::tuple<Ts...> v) override {
    do_fullfill(this->get_queue(), std::move(v), std::move(cb_));
  };

  void fail(std::exception_ptr e) override {
    do_fail(this->get_queue(), e, cb_);
  }

  static void do_finish(QueueT* q, std::tuple<expected<Ts>...> value, CbT cb) {}

  static void do_fullfill(QueueT* q, std::tuple<Ts...> value, CbT cb) {
    enqueue(q, [cb = std::move(cb), v = std::move(value)] {
      std::apply(cb, std::move(v));
    });
  }

  static void do_fail(QueueT* q, std::exception_ptr, CbT) {
    // Swallowed
  }
};

// handling for then_expect()
template <typename CbT, typename QueueT, typename... Ts>
class Future_then_finally_expect_handler
    : public Future_handler_base<QueueT, Ts...> {
  CbT cb_;

 public:
  Future_then_finally_expect_handler(QueueT* q, CbT cb)
      : Future_handler_base<QueueT, Ts...>(q), cb_(std::move(cb)) {}

  void finish(std::tuple<expected<Ts>...> v) override {
    do_finish(this->get_queue(), std::move(v), std::move(cb_));
  }

  void fullfill(std::tuple<Ts...> v) override {
    do_fullfill(this->get_queue(), std::move(v), std::move(cb_));
  };

  void fail(std::exception_ptr e) override {
    do_fail(this->get_queue(), e, std::move(cb_));
  }

  static void do_finish(QueueT* q, std::tuple<expected<Ts>...> value, CbT cb) {
    if constexpr (sizeof...(Ts) == 0) {
      // special case, we are expecting an expected<void> here...
      cb(std::get<0>(value));

    } else {
      std::apply(cb, std::move(value));
    }
  }

  static void do_fullfill(QueueT* q, std::tuple<Ts...> value, CbT cb) {
    if constexpr (sizeof...(Ts) == 0) {
      // special case, we are expecting an expected<void> here...
      cb({});
    } else {
      std::apply(cb, std::move(value));
    }
  }

  static void do_fail(QueueT* q, std::exception_ptr e, CbT cb) {
    static_assert(sizeof...(Ts) < 2);

    cb(unexpected{e});
  }
};

// Holds the shared state associated with a Future<>.
template <typename... Ts>
class Future_storage {
 public:
  using value_type = std::tuple<Ts...>;
  using finish_type = std::tuple<expected<Ts>...>;
  using future_type = Future<Ts...>;

  Future_storage() {}

  void fullfill(value_type&& v) {
    std::lock_guard l(mtx);
    assert(state_ == State::READY || state_ == State::PENDING);
    if (state_ == State::PENDING) {
      state_ = State::VALUE;
      new (&value_) value_type(std::move(v));
    } else {
      callbacks_->fullfill(std::move(v));
    }
  };

  void finish(finish_type&& v) {
    std::lock_guard l(mtx);

    assert(state_ == State::READY || state_ == State::PENDING);
    if (state_ == State::PENDING) {
      state_ = State::FINISHED;
      new (&finish_) finish_type(std::move(v));
    } else {
      callbacks_->finish(std::move(v));
    }
  }

  void fail(std::exception_ptr e) {
    std::lock_guard l(mtx);
    assert(state_ == State::READY || state_ == State::PENDING);

    if (state_ == State::PENDING) {
      state_ = State::ERROR;
      new (&error_) std::exception_ptr(e);
    } else {
      callbacks_->fail(e);
    }
  }

  template <typename Handler_t, typename... Args_t>
  void set_handler(typename Handler_t::queue_type* queue, Args_t&&... args) {
    std::lock_guard l(mtx);
    assert(state_ != State::READY);

    if (state_ == State::PENDING) {
      state_ = State::READY;

      new (&callbacks_) Future_handler_iface<Ts...>*();
      callbacks_ = new Handler_t(queue, std::forward<Args_t>(args)...);
    } else if (state_ == State::VALUE) {
      Handler_t::do_fullfill(queue, std::move(value_),
                             std::forward<Args_t>(args)...);
    } else if (state_ == State::FINISHED) {
      Handler_t::do_finish(queue, std::move(finish_),
                           std::forward<Args_t>(args)...);
    } else if (state_ == State::ERROR) {
      Handler_t::do_fail(queue, std::move(error_),
                         std::forward<Args_t>(args)...);
    }
  }

  ~Future_storage() {
    switch (state_) {
      // LCOV_EXCL_START
      case State::PENDING:
        assert(false);
        break;
      // LCOV_EXCL_END
      case State::READY:
        delete callbacks_;
        break;
      case State::VALUE:
        value_.~value_type();
        break;
      case State::FINISHED:
        finish_.~finish_type();
        break;
      case State::ERROR:
        error_.~exception_ptr();
        break;
    }
  }

 private:
  enum class State {
    PENDING,
    READY,
    VALUE,
    FINISHED,
    ERROR,
  };
  State state_ = State::PENDING;

  union {
    Future_handler_iface<Ts...>* callbacks_;
    value_type value_;
    finish_type finish_;
    std::exception_ptr error_;
  };

  std::mutex mtx;
};


}  // namespace detail
template <typename... Ts>
class Future {
 public:
  using storage_type = detail::Future_storage<Ts...>;

  Future() = default;
  Future(std::tuple<Ts...> values) : storage_(std::make_shared<storage_type>()) {
    storage_->fullfill(std::move(values));
  }

  Future(std::exception_ptr exception) : storage_(std::make_shared<storage_type>()) {
    storage_->fail(exception);
  }

  Future(std::shared_ptr<storage_type> s) : storage_(std::move(s)) {}
  Future(Future&&) = default;
  Future(const Future&) = delete;
  Future& operator=(Future&&) = default;
  Future& operator=(const Future&) = delete;

  // Synchronously calls cb once the future has been fulfilled.
  // cb will be invoked directly in whichever thread fullfills
  // the future.
  //
  // returns a future of whichever type is returned by cb.
  // If this future is failed, then the resulting future
  // will be failed with that same failure, and cb will be destroyed without
  // being invoked.
  //
  // If you intend to discard the result, use then_finally() instead.
  template <typename CbT>
  [[nodiscard]] auto then(CbT cb) {
    // Kinda flying by the seats of our pants here...
    // We rely on the fact that `Immediate_queue` handlers ignore the passed
    // queue.
    detail::Immediate_queue queue;
    return this->then(std::move(cb), queue);
  }

  template <typename CbT>
  [[nodiscard]] auto then_expect(CbT cb) {
    detail::Immediate_queue queue;
    return this->then_expect(std::move(cb), queue);
  }

  // Identical to then, but does not generate a future, which is lighter
  // If cb throws an exception, it will bubble up to the calling thread,
  // wherever that may be.
  template <typename CbT>
  void then_finally(CbT cb) {
    detail::Immediate_queue queue;
    return this->then_finally(std::move(cb), queue);
  }

  template <typename CbT>
  void then_finally_expect(CbT cb) {
    detail::Immediate_queue queue;
    return this->then_finally_expect(std::move(cb), queue);
  }

  // Queues cb in the target queue once the future has been fulfilled.
  //
  // It's expected that the queue itself will outlive the future.
  //
  // Returns a future of whichever type is returned by cb.
  // If this future is failed, then the resulting future
  // will be failed with that same failure, and cb will be destroyed without
  // being invoked.
  //
  // The assignment of the failure will be done synchronously in the fullfilling
  // thread.
  // TODO: Maybe we can add an option to change that behavior
  template <typename CbT, typename QueueT>
  [[nodiscard]] auto then(CbT cb, QueueT& queue) {
    using handler_t = detail::Future_then_handler<CbT, QueueT, Ts...>;
    using result_storage_t = typename handler_t::dst_storage_type;
    using result_fut_t = typename result_storage_t::future_type;

    auto result = std::make_shared<result_storage_t>();

    storage_->template set_handler<handler_t>(&queue, result, std::move(cb));

    return result_fut_t(std::move(result));
  }

  // Special case:
  //   on a Future<>, then_expect() expects a callback with a single
  //   expeted<void> argument
  template <typename CbT, typename QueueT>
  [[nodiscard]] auto then_expect(CbT cb, QueueT& queue) {
    using handler_t = detail::Future_then_expect_handler<CbT, QueueT, Ts...>;
    using result_storage_t = typename handler_t::dst_storage_type;
    using result_fut_t = typename result_storage_t::future_type;

    auto result = std::make_shared<result_storage_t>();

    storage_->template set_handler<handler_t>(&queue, result, std::move(cb));

    return result_fut_t(std::move(result));
  }

  template <typename CbT, typename QueueT>
  void then_finally(CbT cb, QueueT& queue) {
    assert(storage_);

    using handler_t = detail::Future_then_finally_handler<CbT, QueueT, Ts...>;
    storage_->template set_handler<handler_t>(&queue, std::move(cb));
  }

  template <typename CbT, typename QueueT>
  void then_finally_expect(CbT cb, QueueT& queue) {
    assert(storage_);

    using handler_t =
        detail::Future_then_finally_expect_handler<CbT, QueueT, Ts...>;
    storage_->template set_handler<handler_t>(&queue, std::move(cb));
  }

  auto get_std_future(std::unique_lock<std::mutex>* mtx=nullptr) {
    static_assert(
        sizeof...(Ts) <= 1,
        "converting multi-futures to std::future is not supported yet.");

    if constexpr (sizeof...(Ts) == 0) {
      std::promise<void> prom;
      auto fut = prom.get_future();
      this->then_finally_expect(
          [p = std::move(prom)](expected<void> v) mutable {
            if (v.has_value()) {
              p.set_value();
            } else {
              p.set_exception(v.error());
            }
          });
        if(mtx) {
          mtx->unlock();
        }
      return fut;
    } else if constexpr (sizeof...(Ts) == 1) {
      using T = std::tuple_element_t<0, std::tuple<Ts...>>;

      std::promise<T> prom;
      auto fut = prom.get_future();
      this->then_finally_expect([p = std::move(prom)](expected<T> v) mutable {
        if (v.has_value()) {
          p.set_value(std::move(v.value()));
        } else {
          p.set_exception(v.error());
        }
      });
      if(mtx) {
          mtx->unlock();
        }
      return fut;
    } else {
      // Not sure... return a std::future<tuple<Ts>>?
    }
  }

  // akin to a std::future::get()
  decltype(auto) get() { return this->get_std_future().get(); }

 private:
  std::shared_ptr<storage_type> storage_;
};

template <typename... Ts>
class Promise {
 public:
  using future_type = Future<Ts...>;
  using storage_type = typename future_type::storage_type;

  future_type get_future() {
    assert(!storage_);

    storage_ = std::make_shared<storage_type>();
    return future_type{storage_};
  }

  void set_value(Ts... vals) {
    assert(storage_);

    storage_->fullfill(std::make_tuple(std::move(vals)...));
    storage_.reset();
  }

  void finish(expected<Ts>... vals) {
    assert(storage_);

    storage_->finish(std::make_tuple(std::move(vals)...));
    storage_.reset();
  }

  void set_exception(std::exception_ptr e) {
    assert(storage_);

    storage_->fail(e);
    storage_.reset();
  }

 private:
  std::shared_ptr<storage_type> storage_;
};

namespace detail {

template <typename... Ts>
struct Landing {
  std::tuple<expected<Ts>...> storage_;
  std::atomic<int> fullfilled_ = 0;
  std::atomic<bool> failure_ = false;
  std::shared_ptr<Future_storage<Ts...>> dst_;

  void ping(bool failure) {
    if (failure) {
      failure_.store(true);
    }

    if (++fullfilled_ == sizeof...(Ts)) {
      if (!failure_.load()) {
        std::tuple<Ts...> successes =
            std::apply(extract<Ts...>, std::move(storage_));
        dst_->fullfill(std::move(successes));
      } else {
        // TODO: package the errors?
        dst_->fail(std::get<0>(storage_).error());
      }
    }
  }
};

template <std::size_t id, typename LandingT, typename... Ts>
void bind_landing(const std::shared_ptr<LandingT>& l, Future<Ts>... futs) {}

template <std::size_t id, typename LandingT, typename Front, typename... Ts>
void bind_landing(const std::shared_ptr<LandingT>& l, Future<Front> front,
                  Future<Ts>... futs) {
  front.then_finally_expect([=](expected<Front> e) {
    std::get<id>(l->storage_) = e;
    l->ping(!e.has_value());
  });
  bind_landing<id + 1>(l, std::move(futs)...);
}
 
}  // namespace detail
template <typename... Ts>
Future<Ts...> tie(Future<Ts>... futs) {
  static_assert(sizeof...(Ts) >= 2, "Trying to tie less than two futures?");

  using result_type = Future<Ts...>;
  using landing_type = detail::Landing<Ts...>;

  auto landing = std::make_shared<landing_type>();
  landing->dst_ = std::make_shared<detail::Future_storage<Ts...>>();

  detail::bind_landing<0>(landing, std::move(futs)...);

  return landing->dst_;
}

}  // namespace easy_grpc
#endif