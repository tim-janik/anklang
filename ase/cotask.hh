// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/cxxaux.hh>
#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <type_traits>

namespace Ase {

/// Concept: Is T an awaitable?
template<typename T>
concept IsAwaitable = requires (T &t, std::coroutine_handle<> h)
{
  { t.await_ready() } -> std::convertible_to<bool>;
  t.await_resume();
  t.await_suspend (h);
};

/// Start a coroutine in fire-and-forget mode.
struct DetachedTask {
  // not-awaitable: bool await_ready   () { return true; }
  // not-awaitable: void await_suspend (std::coroutine_handle<> h) {}
  // not-awaitable: void await_resume  () {}
  struct promise_type {
    constexpr DetachedTask get_return_object()      { return DetachedTask {}; }
    constexpr auto         initial_suspend()        { return std::suspend_never{}; } // immediate start
    constexpr auto         final_suspend() noexcept { return std::suspend_never{}; } // auto .destroy()
    constexpr void         return_void()            {}
    constexpr void         unhandled_exception()    { ase_rethrow (std::current_exception()); } //  std::terminate(); }
  };
};

/// Helper for CoTask<>
struct CoTaskAux {
  template<typename promise_type>
  struct FinalAwaiter {
    // Always suspend, so CoTask<>::frame_ stays valid
    constexpr bool await_ready() const noexcept { return false; }
    /// Resume the parent frame that is co_await-ing this CoTask<>
    std::coroutine_handle<>
    await_suspend (std::coroutine_handle<promise_type> h) noexcept
    {
      auto &promise = h.promise();      // `h` is the CoTask<> handle, currently in co_return
      if (promise.continuation_)        // we have an awaiting caller
        return promise.continuation_;   // jump to awaiting caller
      if (promise.exception_)           // excption without continuation
        ase_rethrow (promise.exception_);    // must not be forgotten
      return std::noop_coroutine();     // Detached root, suspend is a no-op
    }
    constexpr void await_resume() noexcept { ASE_ASSERT_UNREACHED(); }
  };
  struct promise_base  {
    std::exception_ptr exception_ = nullptr;            // Exception thrown from this coroutine
    void unhandled_exception() noexcept { exception_ = std::current_exception(); }
    // Always create + return CoTask, before starting/resuming execution
    constexpr std::suspend_always initial_suspend() noexcept { return {}; }
#if 0   // universal co_await
    template<typename T>
    struct ConstantAwaiter {
      T value_;
      constexpr bool await_ready() const noexcept { return true; }
      void await_suspend (std::coroutine_handle<>) const noexcept {}
      T await_resume() noexcept { return std::move (value_); }
    };
    template<typename T> decltype(auto) // Accepts anything: Tasks, Awaiters, Integers, Strings…
    await_transform (T &&value)
    {
      if constexpr (IsAwaitable<T>)
        // raw Awaiter that has await_ready/suspend/resume
        return std::forward<T> (value);
      else if constexpr (requires { value.operator co_await(); })
        // T has a member `operator co_await`
        return std::forward<T> (value).operator co_await();
      else if constexpr (requires { operator co_await (std::forward<T> (value)); })
        // T has a global `operator co_await`
        return operator co_await (std::forward<T> (value));
      else
        // T is a plain value like int, string
        return ConstantAwaiter<std::remove_cvref_t<T>> { std::forward<T>(value) };
    }
#endif
  };
};

/** General purpose coroutine task.
 * This class represents a unit of asynchronous work that lazily executes
 * when awaited. It is designed for composition (Task A awaits Task B).
 */
template<typename Result>
struct CoTask {
  static_assert (!std::is_reference_v<Result>, "The Result in CoTask<Result> must be movable or copyable");
  struct promise_type : CoTaskAux::promise_base {
    std::coroutine_handle<> continuation_ = nullptr;    // Waiter frame co_await-ing this CoTask
    CoTask get_return_object() noexcept { return CoTask { std::coroutine_handle<promise_type>::from_promise (*this) }; }
    CoTaskAux::FinalAwaiter<promise_type> final_suspend() noexcept { return {}; }
    std::optional<Result> result_;
    void
    return_value (Result &&value) noexcept (std::is_nothrow_move_constructible_v<Result>)
    {
      result_.emplace (std::move (value));      // move-only
    }
    void
    return_value (const Result &value) noexcept (std::is_nothrow_copy_constructible_v<Result>)
    {
      result_.emplace (value);                  // copyable
    }
  };
  // -- Awaitable Protocol --
  constexpr bool await_ready () const noexcept { return !frame_ || frame_.done(); }
  Result
  await_resume()
  {
    if (frame_.promise().exception_) ase_rethrow (frame_.promise().exception_);
    return std::move (*frame_.promise().result_);
  }
  std::coroutine_handle<>
  await_suspend (std::coroutine_handle<> waiter) noexcept
  {
    // assert SINGLE caller for this coroutine
    ASE_ASSERT_RETURN (frame_.promise().continuation_ == nullptr, std::noop_coroutine());
    frame_.promise().continuation_ = waiter; // next task to wake up when we co_return
    return frame_.done() ? waiter : frame_;  // handle to resume immediately (symmetric transfer)
  }
  // -- Frame Handling ---
  explicit CoTask (std::coroutine_handle<promise_type> h) : frame_ (h) {}
  ~CoTask ()
  {
    ASE_RETURN_UNLESS (frame_);
    ASE_ASSERT_RETURN (frame_.done()); // assert unless we start implementing cancelation
    frame_.destroy();
  }
  /*ctor*/ CoTask    (CoTask &&o) noexcept : frame_ (o.frame_) { o.frame_ = nullptr; }
  CoTask&  operator= (CoTask &&o) noexcept { std::swap (frame_, o.frame_); return *this; }
  /*ctor*/ CoTask    (const CoTask&) = delete;  // no-copy
  CoTask&  operator= (const CoTask&) = delete;  // no-copy
protected:
  std::coroutine_handle<promise_type> frame_; ///< Handle for this task
};

// TODO: should we rethrow instead of storing if continuation_ == nullptr? i.e. we're a root task

/// Like CoTask<Result> without return type.
template<>
struct CoTask<void> {
  struct promise_type : CoTaskAux::promise_base {
    std::coroutine_handle<> continuation_ = nullptr;    // Waiter frame co_await-ing this CoTask
    CoTask get_return_object() noexcept { return CoTask { std::coroutine_handle<promise_type>::from_promise (*this) }; }
    CoTaskAux::FinalAwaiter<promise_type> final_suspend() noexcept { return {}; }
    constexpr void return_void() noexcept {}            // Handle 'co_return;'
  };
  // -- Awaitable Protocol --
  constexpr bool await_ready () const noexcept { return !frame_ || frame_.done(); }
  void
  await_resume()
  {
    if (frame_.promise().exception_) ase_rethrow (frame_.promise().exception_);
  }
  std::coroutine_handle<>
  await_suspend (std::coroutine_handle<> waiter) noexcept
  {
    // assert SINGLE caller for this coroutine
    ASE_ASSERT_RETURN (frame_.promise().continuation_ == nullptr, std::noop_coroutine());
    frame_.promise().continuation_ = waiter; // next task to wake up when we co_return
    return frame_.done() ? waiter : frame_;  // handle to resume immediately (symmetric transfer)
  }
  // -- Frame Handling ---
  explicit CoTask    (std::coroutine_handle<promise_type> h) : frame_ (h) {}
  /*dtor*/~CoTask    () { ASE_RETURN_UNLESS (frame_); ASE_ASSERT_RETURN (frame_.done()); frame_.destroy(); }
  /*ctor*/ CoTask    (CoTask &&o) noexcept : frame_ (o.frame_) { o.frame_ = nullptr; }
  CoTask&  operator= (CoTask &&o) noexcept { std::swap (frame_, o.frame_); return *this; }
  /*ctor*/ CoTask    (const CoTask&) = delete;  // no-copy
  CoTask&  operator= (const CoTask&) = delete;  // no-copy
protected:
  std::coroutine_handle<promise_type> frame_; ///< Handle for this task
};
using CoTaskVoid = CoTask<void>;

} // Ase
