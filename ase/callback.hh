// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CALLBACK_HH__
#define __ASE_CALLBACK_HH__

#include <ase/defs.hh>

namespace Ase {

// == CallbackList<> ==
/// Reentrant callback list with configurable arguments.
template<class... A>
class CallbackList : public std::enable_shared_from_this<CallbackList<A...>> {
  CallbackList() {}
public:
  ASE_DEFINE_MAKE_SHARED (CallbackList);
  using Callback = std::function<void(const A&...)>;
  /// Check if the callback list is empty, i.e. invocation will not call any callbacks.
  bool    empty   () const              { return funcs_.empty(); }
  /// Delete a previously added callback via its id, returns if any was found.
  bool    del     (size_t id)           { return del_id (id); }
  /// Add a callback, returns an id that can be used for deletion.
  size_t  add     (const Callback &f)   { return add_id (f); }
  /// Add a callback and return a deleter that removes the callback when invoked.
  std::function<void()>
  add_delcb (const Callback &f)
  {
    std::shared_ptr<CallbackList> callbacklistp = this->shared_from_this();
    ASE_ASSERT_RETURN (callbacklistp != nullptr, {});
    const size_t id = add_id (f);
    auto destroy_callback = [id, callbacklistp] () { callbacklistp->del (id); };
    return destroy_callback;
  }
  /// Call all callbacks in the order they were added via wrapper function.
  void
  call (const std::function<void(const Callback&, const A&...)> &wrapper, const A &...args)
  {
    size_t next = 0;
    iters_.push_back (&next);
    while (next < funcs_.size())
      // increment next *before* calling callback to be accurate and adjusted by del_id()
      wrapper (funcs_[next++].func, std::forward<const A> (args)...);
    const size_t old_size = iters_.size();
    iters_.erase (std::remove_if (iters_.begin(), iters_.end(),
                                  [&next] (const size_t *p) { return p == &next; }),
                  iters_.end());
    ASE_ASSERT_RETURN (1 == old_size - iters_.size());
  }
  /// Call all callbacks in the order they were added.
  void
  operator() (const A &...args)
  {
    auto caller = [] (const Callback &cb, const A &...args) { cb (std::forward<const A> (args)...); };
    return call (caller, std::forward<const A> (args)...);
  }
private:
  struct Entry { Callback func; size_t id = 0; };
  std::vector<Entry>   funcs_;
  std::vector<size_t*> iters_;
  static size_t
  newid()
  {
    static size_t counter = 0;
    return ++counter;
  }
  size_t
  add_id (const Callback &f)
  {
    const size_t id = newid();
    funcs_.push_back ({ f, id });
    return id;
  }
  bool
  del_id (const size_t id)
  {
    for (size_t j = 0; j < funcs_.size(); j++)
      if (funcs_[j].id == id) {
        funcs_.erase (funcs_.begin() + j);
        for (size_t i = 0; i < iters_.size(); i++)
          if (j < *iters_[i]) // if a callback *before* next
            *iters_[i] -= 1;  // was deleted, adjust next
        return true;
      }
    return false;
  }
  ASE_CLASS_NON_COPYABLE (CallbackList);
};

// == RtCall ==
/// Wrap simple callback pointers, without using malloc (obstruction free).
struct RtCall {
  /// Wrap a simple `void func()` object member function call.
  template<typename T> RtCall (T &o, void (T::*f) ());
  /// Wrap a single argument `void func (T*)` function call with its pointer argument.
  template<typename T> RtCall (void (*f) (T*), T *d);
  /// Wrap a simple `void func ()` function call.
  /*ctor*/             RtCall (void (*f) ());
  /// Copy function call pointers from another RtCall.
  explicit             RtCall (const RtCall &call);
  /// Invoke the wrapped function call.
  void                 invoke ();
  /// Clear function pointers.
  /*dtor*/            ~RtCall ();
private:
  static constexpr int pdsize = 4;
  ptrdiff_t mem_[pdsize] = { 0, 0, 0, 0 };
  struct Callable {
    virtual void call () const = 0;
  };
  const Callable* callable () const;
};

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
template<typename T>
RtCall::RtCall (T &o, void (T::*f) ())
{
  struct Wrapper : Callable {
    Wrapper (T &o, void (T::*f) ()) : f_ (f), o_ (&o) {}
    void call() const override { return (o_->*f_)(); }
    void (T::*f_) (); T *o_;
  };
  static_assert (sizeof (mem_) >= sizeof (Wrapper));
  Wrapper *w = new (mem_) Wrapper { o, f };
  ASE_ASSERT_RETURN (w == (void*) mem_);
}

template<typename T>
RtCall::RtCall (void (*f) (T*), T *d)
{
  struct Wrapper : Callable {
    Wrapper (void (*f) (T*), T *d) : f_ (f), d_ (d) {}
    void call() const override { return f_ (d_); }
    void (*f_) (T*); T *d_;
  };
  static_assert (sizeof (mem_) >= sizeof (Wrapper));
  Wrapper *w = new (mem_) Wrapper { f, d };
  ASE_ASSERT_RETURN (w == (void*) mem_);
}

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
