// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CALLBACK_HH__
#define __ASE_CALLBACK_HH__

#include <ase/defs.hh>

namespace Ase {

// == JobQueue for cross-thread invocations ==
struct JobQueue {
  enum Policy { SYNC, };
  using Caller = std::function<void (Policy, const std::function<void()>&)>;
  explicit             JobQueue    (const Caller &caller);
  template<typename F> std::invoke_result_t<F>
  inline               operator+=  (const F &job);
private:
  const Caller caller_;
};



// == Implementation Details ==
template<typename F> std::invoke_result_t<F>
JobQueue::operator+= (const F &job)
{
  using R = std::invoke_result_t<F>;
  if constexpr (std::is_same<R, void>::value)
    caller_ (SYNC, job);
  else // R != void
    {
      R result;
      call_remote ([&job, &result] () {
        result = job();
      });
      return result;
    }
}

} // Ase

#endif // __ASE_CALLBACK_HH__
