// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/cxxaux.hh>
#include <coroutine>
#include <exception>
#include <functional>
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

} // Ase
