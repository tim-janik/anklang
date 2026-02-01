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

} // Ase
