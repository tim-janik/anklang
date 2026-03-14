// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "loop.hh"
#include "atomics.hh"
#include "utils.hh"
#include "platform.hh"
#include "internal.hh"
#include "strings.hh"
#include <sys/poll.h>
#include <sys/wait.h>
#include <errno.h>
#include <atomic>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <algorithm>
#include <list>

namespace Ase {

enum {
  WAITING             = 0,
  PREPARED,
  NEEDS_DISPATCH,
};

static constexpr int16_t WILLQUIT = 0x8000;     // quit() called before or during run()
static constexpr int16_t HASQUIT = 0x4000;      // last run() or iterate() was quit
static constexpr int16_t UNDEFINED_PRIORITY = -32768;
static constexpr auto    PRIORITY_CEILING = LoopPriority::SYSALLOC;

// == PollFD invariants ==
static_assert (PollFD::IN     == POLLIN);
static_assert (PollFD::PRI    == POLLPRI);
static_assert (PollFD::OUT    == POLLOUT);
static_assert (PollFD::RDNORM == POLLRDNORM);
static_assert (PollFD::RDBAND == POLLRDBAND);
static_assert (PollFD::WRNORM == POLLWRNORM);
static_assert (PollFD::WRBAND == POLLWRBAND);
static_assert (PollFD::ERR    == POLLERR);
static_assert (PollFD::HUP    == POLLHUP);
static_assert (PollFD::NVAL   == POLLNVAL);
static_assert (sizeof   (PollFD)               == sizeof   (struct pollfd));
static_assert (offsetof (PollFD, fd)           == offsetof (struct pollfd, fd));
static_assert (sizeof (((PollFD*) 0)->fd)      == sizeof (((struct pollfd*) 0)->fd));
static_assert (offsetof (PollFD, events)       == offsetof (struct pollfd, events));
static_assert (sizeof (((PollFD*) 0)->events)  == sizeof (((struct pollfd*) 0)->events));
static_assert (offsetof (PollFD, revents)      == offsetof (struct pollfd, revents));
static_assert (sizeof (((PollFD*) 0)->revents) == sizeof (((struct pollfd*) 0)->revents));

// === Stupid ID allocator ===
static std::atomic<uint64_t> global_id_counter { 101000000 };
static LoopID
alloc_id ()
{
  const uint64_t id = global_id_counter.fetch_add (+1, std::memory_order_relaxed);
  assert_return (id != 0, {});
  return LoopID (id);
}

static void
release_id (LoopID id)
{
  assert_return (id != LoopID::INVALID);
}

// === Loop ==
Loop::Loop() {}
Loop::~Loop() {}

// === LoopImpl ===
/// Loop implementation with internal state.
class LoopImpl : public Loop
{
  ASE_CLASS_NON_COPYABLE (LoopImpl);
  std::vector<PollFD>       cached_pollfd_vector_;
  std::vector<LoopSourceP*> cached_poll_candidates_;
public:
  ASE_DEFINE_MAKE_SHARED (LoopImpl);
  typedef std::vector<LoopSourceP> SourceList;
  SourceList              sources_;
  std::vector<LoopSourceP> poll_sources_;
  AtomicStack<LoopSourceP> pending_add_stack_;
  AtomicStack<LoopID>      pending_cancel_stack_;
  std::atomic<int16_t>    quit_code_ = 0;
  uint                    running_ = 0;
  uint                    rr_index_ = 0;
  int16                   dispatch_priority_ = 0;
  EventFd                 eventfd_;
  GlibGMainContext       *gcontext_;
  bool                    finishable_L        ();
  int                     run                 () override;
  bool                    running             () override;
  void                    wakeup              () override;
  void                    quit                (int quit_code) override;
  bool                    finishable          () override;
  bool                    iterate             (bool may_block) override;
  void                    iterate_pending     () override;
  bool                    pending             () override;
  bool                    set_g_main_context  (GlibGMainContext *glib_main_context) override;
  bool                    has_quit            () override;
  bool                    iterate_loops_Lm    (LoopState&, bool b, bool d);
  void                    destroy_loop        () override;
  LoopSourceP&            find_first_L        ();
  LoopSourceP&            find_source_L       (LoopID id);
  bool                    has_primary_L       (void);
  void                    remove_source_Lm    (LoopSourceP source);
  void                    kill_sources_Lm     (void);
  void                    unpoll_sources_U    ();
  void                    collect_sources_Lm  (LoopState&);
  bool                    prepare_sources_Lm  (LoopState&, std::vector<PollFD>&);
  bool                    check_sources_Lm    (LoopState&, const std::vector<PollFD>&);
  void                    dispatch_source_Lm  (LoopState&);
  void                    process_atomic_stacks ();
  LoopID                  add_source          (LoopSourceP loop_source, LoopPriority priority) override;
  void                    cancel              (LoopID id) override;
  void                    cancel              (LoopID *idp) override;
  bool                    has_primary         () override;
  LoopID                  exec_sigchld        (int64_t pid, const SigchldSlot &vfunc, LoopPriority priority) override;
  bool                    exec_once           (uint delay_ms, LoopID *once_id, const VoidSlot &vfunc, LoopPriority priority) override;
  explicit                LoopImpl            ();
  virtual                ~LoopImpl            ();
};

// === LoopImpl ===
inline LoopSourceP&
LoopImpl::find_first_L()
{
  static LoopSourceP null_source;
  return sources_.empty() ? null_source : sources_[0];
}

inline LoopSourceP&
LoopImpl::find_source_L (LoopID id)
{
  for (SourceList::iterator lit = sources_.begin(); lit != sources_.end(); lit++)
    if (id == (*lit)->id_)
      return *lit;
  static LoopSourceP null_source;
  return null_source;
}

bool
LoopImpl::has_primary_L()
{
  for (SourceList::iterator lit = sources_.begin(); lit != sources_.end(); lit++)
    if ((*lit)->primary())
      return true;
  return false;
}

void
LoopImpl::process_atomic_stacks ()
{
  LoopSourceP source;
  while (pending_add_stack_.pop (source))
    sources_.push_back (source);
  LoopID cancel_id;
  while (pending_cancel_stack_.pop (cancel_id))
    {
      LoopSourceP &src = find_source_L (cancel_id);
      if (src)
        remove_source_Lm (src);
    }
}

bool
LoopImpl::has_primary()
{
  return has_primary_L();
}

/** Add an event source to the loop.
 * This method adds a `LoopSource` to the event loop with a specified priority.
 * The source will be monitored and dispatched according to its implementation
 * and the loop's iteration logic.
 * This method is thread-safe and can be called from any thread. If the loop is
 * currently blocked in a poll() call, it will be woken up to process the new source.
 * @param source   The event source to add.
 * @param priority The priority at which the source should be dispatched.
 * @return A unique `LoopID` for the added source, or `LoopID::INVALID` on failure.
 */
LoopID
LoopImpl::add_source (LoopSourceP source, LoopPriority priority)
{
  static_assert (UNDEFINED_PRIORITY < 1, "");
  assert_return (static_cast<uint16_t> (priority) >= 1 && priority <= PRIORITY_CEILING, LoopID::INVALID);
  assert_return (source != NULL, LoopID::INVALID);
  assert_return (source->loop_ == NULL, LoopID::INVALID);
  source->loop_ = this;
  const auto source_id = source->id_ = alloc_id();
  source->loop_state_ = WAITING;
  source->priority_ = static_cast<uint16_t> (priority);
  if (pending_add_stack_.push (source))
    wakeup();
  return source_id;
}

void
LoopImpl::remove_source_Lm (LoopSourceP source)
{
  assert_return (source->loop_ == this);
  source->loop_ = NULL;
  source->loop_state_ = WAITING;
  auto pos = find (sources_.begin(), sources_.end(), source);
  assert_return (pos != sources_.end());
  sources_.erase (pos);
  release_id (source->id_);
  source->id_ = LoopID::INVALID;
  source->destroy();
}

/** Cancel an event source.
 * This method removes a source from the loop using its unique `LoopID`.
 * If the source is currently being dispatched, it will be removed after
 * the dispatch callback returns.
 * This method is thread-safe.
 * @param id The unique ID of the source to cancel.
 */
void
LoopImpl::cancel (LoopID id)
{
  return_unless (id != LoopID::INVALID);
  if (pending_cancel_stack_.push (id))
    wakeup();
}

/** Cancel an event source.
 * This method removes a source from the loop using its unique `LoopID`.
 * If the source is currently being dispatched, it will be removed after
 * the dispatch callback returns.
 * This method is thread-safe.
 * @param idp Pointer to the unique ID of the source to cancel. The ID will be reset to INVALID (0).
 */
void
LoopImpl::cancel (LoopID *idp)
{
  if (idp) {
    if (*idp != LoopID::INVALID)
      cancel (*idp);
    *idp = LoopID::INVALID;
  }
}

bool
LoopImpl::exec_once (uint delay_ms, LoopID *once_id, const VoidSlot &vfunc, LoopPriority priority)
{
  assert_return (once_id != nullptr, false);
  assert_return (static_cast<uint16_t> (priority) >= 1 && priority <= PRIORITY_CEILING, false);
  if (!vfunc) {
    cancel (once_id);
    return false;
  }
  auto once_handler = [vfunc,once_id]() { *once_id = LoopID::INVALID; vfunc(); };
  LoopSourceP source = TimedSource::create (once_handler, delay_ms, 0);
  source->loop_ = this;
  source->id_ = alloc_id();
  source->loop_state_ = WAITING;
  source->priority_ = static_cast<uint16_t> (priority);
  LoopID warn_id = LoopID::INVALID;
  {
    if (*once_id != LoopID::INVALID) {
      LoopSourceP &source = find_source_L (*once_id);
      if (source)
        remove_source_Lm (source);
      else
        warn_id = *once_id;
    }
    sources_.push_back (source);
    *once_id = source->id_;
  }
  if (warn_id != LoopID::INVALID)
    warning ("%s: failed to remove loop source: %lu", __func__, static_cast<uint64_t> (warn_id));
  wakeup();
  return true;
}

LoopID
LoopImpl::exec_sigchld (int64_t pid, const SigchldSlot &slot, LoopPriority priority)
{
  return add_source (SigchldSource::create (pid, slot), priority);
}

void
LoopImpl::kill_sources_Lm()
{
  for (;;)
    {
      LoopSourceP &source = find_first_L();
      if (source == NULL)
        break;
      remove_source_Lm (source);
    }
  unpoll_sources_U(); // unlocked
}

/** Return the thread-local singleton loop, created on first call.
 * Each thread gets its own independent loop instance that lives for the
 * duration of the thread. The loop is created lazily on first access and
 * is kept alive by the thread_local storage. Calling current() from
 * different threads returns different loop instances; calling it multiple
 * times from the same thread always returns the same instance.
 */
LoopP
Loop::current ()
{
  static thread_local LoopP thread_loop = LoopImpl::make_shared();
  return thread_loop;
}

/// Create a promise that resolves after `ms` milliseconds and returns the elapsed delay.
std::shared_ptr<Loop::Promise<uint64_t>>
Loop::delay (std::chrono::milliseconds ms)
{
  const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  auto promise = this->make_promise<uint64_t> ([&] (auto resolve) // std::function<void(uint64_t)>
  {
    this->add ([resolve, start]()
    {
      const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
      const uint64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds> (end - start).count();
      resolve (elapsed);
      return false;
    }, ms);
  });
  return promise;
}

// === LoopImpl ===
LoopImpl::LoopImpl() :
  gcontext_ (nullptr)
{
  cached_pollfd_vector_.reserve (1);
  cached_poll_candidates_.reserve (1);
  const int err = eventfd_.open();
  if (err < 0)
    fatal_error ("LoopImpl: failed to create wakeup pipe: %s", strerror (-err));
  // eventfd_ must work upfront for wakeup() to work
}

LoopImpl::~LoopImpl()
{
  destroy_loop();
  assert_return (sources_.empty() == true);
}

/** Remove all sources from a loop and prevent any further execution.
 * The destroy_loop() method removes all sources from a loop and in
 * case of a sub Loop (see create_sub_loop()) removes it from its
 * associated main loop. Calling destroy_loop() on a main loop also
 * calls destroy_loop() for all its sub loops.
 * Note that LoopImpl objects are artificially kept alive until
 * LoopImpl::destroy_loop() is called, so calling destroy_loop() is
 * mandatory for LoopImpl objects to prevent object leaks.
  */
void
LoopImpl::destroy_loop()
{
  set_g_main_context (NULL);
  // guard main_loop_ pointer across callbacks
  LoopP main_loop_guard = shared_ptr_cast<Loop*> (this);
  process_atomic_stacks();
  kill_sources_Lm();
}

/** Wake up the event loop.
 * This method wakes up the event loop if it is currently blocked waiting
 * for events. It is safe to call from any thread.
 */
void
LoopImpl::wakeup()
{
  if (eventfd_.opened())
    eventfd_.wakeup();
}

/** Run the event loop.
 *
 * This method starts the event loop and continues to process events until
 * `quit()` is called. It returns the exit code provided to `quit()`.
 * @return The exit code passed to `quit()`.
 */
int
LoopImpl::run ()
{
  LoopP main_loop_guard = shared_ptr_cast<Loop> (this);
  LoopState state;
  quit_code_ &= ~HASQUIT;               // reset old quit code, from former run()
  running_ += 1;
  while (ISLIKELY (!(WILLQUIT & quit_code_)))
    iterate_loops_Lm (state, true, true);
  running_ -= 1;
  if (quit_code_ & WILLQUIT)            // apply quit code from this run()
    quit_code_ = HASQUIT | (quit_code_ & ~WILLQUIT);
  return quit_code_ & ~HASQUIT;         // return actual exit code, not internal flag
}

bool
LoopImpl::running ()
{
  return running_ > 0;
}

bool
LoopImpl::has_quit ()
{
  return HASQUIT & quit_code_;          // last run() was quit
}

/** Stop the event loop.
 *
 * This method signals the event loop to stop processing events and exit
 * its `run()` method. The provided `quit_code` will be the return value
 * of `Loop::run()`.
 * This method is safe to call from any thread.
 * @param quit_code The exit code for the loop to return.
 */
void
LoopImpl::quit (int quit_code)
{
  if (!(WILLQUIT & quit_code_)) {
    quit_code_ = quit_code | WILLQUIT;
    wakeup ();
  }
}

bool
LoopImpl::finishable_L()
{
  // finishable if no primary sources remain
  return !has_primary_L();
}

bool
LoopImpl::finishable()
{
  return finishable_L();
}

/** Iterate the main loop once.
 *
 * LoopImpl::iterate() is the heart of the main event loop. For loop iteration,
 * all event sources are polled for incoming events. Then dispatchable sources
 * are picked one per iteration and dispatched in round-robin fashion. If no
 * sources need immediate dispatching and `may_block` is true, iterate() will
 * wait for events to become available.
 * @param may_block     If true, iterate() will wait for events occour.
 * @returns Whether more sources need immediate dispatching.
 */
bool
LoopImpl::iterate (bool may_block)
{
  LoopP     main_loop_guard = shared_ptr_cast<Loop> (this);
  LoopState state;
  running_ += 1;
  const bool sources_pending = iterate_loops_Lm (state, may_block, true);
  running_ -= 1;
  return sources_pending;
}

/** Iterate pending sources.
 *
 * This method is used to iterate pending sources, when called after quit()
 * it will continue to iterate until all sources are dispatched, unless
 * the loop is quit again.
 */
void
LoopImpl::iterate_pending ()
{
  LoopP          main_loop_guard = shared_ptr_cast<Loop> (this);
  LoopState      state;
  const uint16_t saved_quit_code_ = quit_code_; // save former quit code
  running_ += 1;
  while (ISLIKELY (!(WILLQUIT & quit_code_)))   // abort on quitting twice
    if (!iterate_loops_Lm (state, false, true))
      break;
  running_ -= 1;
  if (saved_quit_code_ & HASQUIT)               // preserve former quit code
    quit_code_ = saved_quit_code_ & ~WILLQUIT;
  else if (quit_code_ & WILLQUIT)               // or apply recent quit code
    quit_code_ = HASQUIT | (quit_code_ & ~WILLQUIT);
}

bool
LoopImpl::pending()
{
  LoopState state;
  LoopP main_loop_guard = shared_ptr_cast<Loop> (this);
  running_ += 1;
  const bool more = iterate_loops_Lm (state, false, false);
  running_ -= 1;
  return more;
}

bool
LoopImpl::set_g_main_context (GlibGMainContext *glib_main_context)
{
#ifdef  __G_LIB_H__
  if (glib_main_context)
    {
      if (gcontext_)
        return false;
      if (!g_main_context_acquire (glib_main_context))
        return false;
      gcontext_ = g_main_context_ref (glib_main_context);
    }
  else if (gcontext_)
    {
      glib_main_context = gcontext_;
      gcontext_ = NULL;
      g_main_context_release (glib_main_context);
      g_main_context_unref (glib_main_context);
    }
  return true;
#else
  return false;
#endif
}

#ifdef  __G_LIB_H__
static GPollFD*
mk_gpollfd (PollFD *pfd)
{
  GPollFD *gpfd = (GPollFD*) pfd;
  static_assert (sizeof (GPollFD) == sizeof (PollFD), "");
  static_assert (sizeof (gpfd->fd) == sizeof (pfd->fd), "");
  static_assert (sizeof (gpfd->events) == sizeof (pfd->events), "");
  static_assert (sizeof (gpfd->revents) == sizeof (pfd->revents), "");
  static_assert (offsetof (GPollFD, fd) == offsetof (PollFD, fd), "");
  static_assert (offsetof (GPollFD, events) == offsetof (PollFD, events), "");
  static_assert (offsetof (GPollFD, revents) == offsetof (PollFD, revents), "");
  static_assert (PollFD::IN == int (G_IO_IN), "");
  static_assert (PollFD::PRI == int (G_IO_PRI), "");
  static_assert (PollFD::OUT == int (G_IO_OUT), "");
  // static_assert (PollFD::RDNORM == int (G_IO_RDNORM), "");
  // static_assert (PollFD::RDBAND == int (G_IO_RDBAND), "");
  // static_assert (PollFD::WRNORM == int (G_IO_WRNORM), "");
  // static_assert (PollFD::WRBAND == int (G_IO_WRBAND), "");
  static_assert (PollFD::ERR == int (G_IO_ERR), "");
  static_assert (PollFD::HUP == int (G_IO_HUP), "");
  static_assert (PollFD::NVAL == int (G_IO_NVAL), "");
  return gpfd;
}
#endif

void
LoopImpl::unpoll_sources_U() // must be unlocked!
{
  // clear poll sources
  poll_sources_.resize (0);
}

void
LoopImpl::collect_sources_Lm (LoopState &state)
{
  // enforce clean slate
  if (UNLIKELY (!poll_sources_.empty()))
    {
      unpoll_sources_U(); // unlocked
      assert_return (poll_sources_.empty());
    }
  if (UNLIKELY (!state.seen_primary))
    state.seen_primary = true;
  // cache vector, otherwise malloc shows up in the profiles
  std::vector<LoopSourceP*> &poll_candidates = cached_poll_candidates_;
  poll_candidates.resize (0);
  // determine dispatch priority & collect sources for preparing
  dispatch_priority_ = UNDEFINED_PRIORITY; // initially, consider sources at *all* priorities
  for (SourceList::iterator lit = sources_.begin(); lit != sources_.end(); lit++)
    {
      LoopSource &source = **lit;
      if (UNLIKELY (!state.seen_primary && source.primary_))
        state.seen_primary = true;
      if (source.loop_ != this ||                               // ignore destroyed and
          (source.dispatching_ && !source.may_recurse_))        // avoid unallowed recursion
        continue;
      if (source.priority_ > dispatch_priority_ &&              // ignore lower priority sources
          source.loop_state_ == NEEDS_DISPATCH)                 // if NEEDS_DISPATCH sources remain
        dispatch_priority_ = source.priority_;                  // so raise dispatch_priority_
      if (source.priority_ > dispatch_priority_ ||              // add source if it is an eligible
          (source.priority_ == dispatch_priority_ &&            // candidate, baring future raises
           source.loop_state_ == NEEDS_DISPATCH))               // of dispatch_priority_...
        poll_candidates.push_back (&*lit);                      // collect only, adding ref() later
    }
  // ensure ref counts on all prepare sources
  assert_return (poll_sources_.empty());
  for (size_t i = 0; i < poll_candidates.size(); i++)
    if ((*poll_candidates[i])->priority_ > dispatch_priority_ || // throw away lower priority sources
        ((*poll_candidates[i])->priority_ == dispatch_priority_ &&
         (*poll_candidates[i])->loop_state_ == NEEDS_DISPATCH)) // re-poll sources that need dispatching
      poll_sources_.push_back (*poll_candidates[i]);
  /* here, poll_sources_ contains either all sources, or only the highest priority
   * NEEDS_DISPATCH sources plus higher priority sources. giving precedence to the
   * remaining NEEDS_DISPATCH sources ensures round-robin processing.
   */
}

bool
LoopImpl::prepare_sources_Lm (LoopState &state, std::vector<PollFD> &pfda)
{
  // prepare sources, up to NEEDS_DISPATCH priority
  for (auto lit = poll_sources_.begin(); lit != poll_sources_.end(); lit++)
    {
      LoopSource &source = **lit;
      if (source.loop_ != this) // test undestroyed
        continue;
      int64 timeout = -1;
      const bool need_dispatch = source.prepare (state, &timeout);
      if (source.loop_ != this)
        continue; // ignore newly destroyed sources
      if (need_dispatch)
        {
          dispatch_priority_ = std::max (dispatch_priority_, source.priority_); // upgrade dispatch priority
          source.loop_state_ = NEEDS_DISPATCH;
          continue;
        }
      source.loop_state_ = PREPARED;
      if (timeout >= 0)
        state.timeout_usecs = std::min (state.timeout_usecs, timeout);
      uint npfds = source.n_pfds();
      for (uint i = 0; i < npfds; i++)
        if (source.pfds_[i].pfd->fd >= 0)
          {
            uint idx = pfda.size();
            source.pfds_[i].idx = idx;
            pfda.push_back (*source.pfds_[i].pfd);
            pfda[idx].revents = 0;
          }
        else
          source.pfds_[i].idx = 4294967295U; // UINT_MAX
    }
  return dispatch_priority_ > UNDEFINED_PRIORITY;
}

bool
LoopImpl::check_sources_Lm (LoopState &state, const std::vector<PollFD> &pfda)
{
  // check polled sources
  for (auto lit = poll_sources_.begin(); lit != poll_sources_.end(); lit++)
    {
      LoopSource &source = **lit;
      if (source.loop_ != this && // test undestroyed
          source.loop_state_ != PREPARED)
        continue; // only check prepared sources
      uint npfds = source.n_pfds();
      for (uint i = 0; i < npfds; i++)
        {
          uint idx = source.pfds_[i].idx;
          if (idx < pfda.size() &&
              source.pfds_[i].pfd->fd == pfda[idx].fd)
            source.pfds_[i].pfd->revents = pfda[idx].revents;
          else
            source.pfds_[i].idx = 4294967295U; // UINT_MAX
        }
      bool need_dispatch = source.check (state);
      if (source.loop_ != this)
        continue; // ignore newly destroyed sources
      if (need_dispatch)
        {
          dispatch_priority_ = std::max (dispatch_priority_, source.priority_); // upgrade dispatch priority
          source.loop_state_ = NEEDS_DISPATCH;
        }
      else
        source.loop_state_ = WAITING;
    }
  return dispatch_priority_ > UNDEFINED_PRIORITY;
}

void
LoopImpl::dispatch_source_Lm (LoopState &state)
{
  // find a source to dispatch at dispatch_priority_
  LoopSourceP dispatch_source = NULL;                  // shared_ptr to keep alive even if everything else is destroyed
  for (auto lit = poll_sources_.begin(); lit != poll_sources_.end(); lit++)
    {
      LoopSourceP &source = *lit;
      if (source->loop_ == this &&                      // test undestroyed
          source->priority_ == dispatch_priority_ &&    // only dispatch at dispatch priority
          source->loop_state_ == NEEDS_DISPATCH)
        {
          dispatch_source = source;
          break;
        }
    }
  dispatch_priority_ = UNDEFINED_PRIORITY;
  // dispatch single source
  if (dispatch_source)
    {
      dispatch_source->loop_state_ = WAITING;
      const bool old_was_dispatching = dispatch_source->was_dispatching_;
      dispatch_source->was_dispatching_ = dispatch_source->dispatching_;
      dispatch_source->dispatching_ = true;
      const bool keep_alive = dispatch_source->dispatch (state);
      dispatch_source->dispatching_ = dispatch_source->was_dispatching_;
      dispatch_source->was_dispatching_ = old_was_dispatching;
      if (dispatch_source->loop_ == this && !keep_alive)
        remove_source_Lm (dispatch_source);
    }
}

bool
LoopImpl::iterate_loops_Lm (LoopState &state, bool may_block, bool may_dispatch)
{
  assert_return (state.phase == state.NONE, false);
  process_atomic_stacks ();
  std::vector<PollFD> &pfda = cached_pollfd_vector_;
  pfda.resize (0); // cache array between iterations, to reduce malloc overhead
  // allow poll wakeups
  const PollFD wakeup = { eventfd_.inputfd(), PollFD::IN, 0 };
  const uint wakeup_idx = 0; // wakeup_idx = pfda.size();
  pfda.push_back (wakeup);
  // collect
  state.phase = state.COLLECT;
  state.seen_primary = false;
  collect_sources_Lm (state);
  // prepare
  bool any_dispatchable = false;
  state.phase = state.PREPARE;
  state.timeout_usecs = INT64_MAX;
  state.current_time_usecs = timestamp_realtime();
  bool adispatchable = false;
  bool gdispatchable = false;
  adispatchable = prepare_sources_Lm (state, pfda);
  any_dispatchable |= adispatchable;
  // prepare GLib
  ASE_UNUSED const int gfirstfd = pfda.size();
  ASE_UNUSED int gpriority = INT32_MIN;
  if (ASE_UNLIKELY (gcontext_))
    {
#ifdef  __G_LIB_H__
      gdispatchable = g_main_context_prepare (gcontext_, &gpriority) != 0;
      any_dispatchable |= gdispatchable;
      int gtimeout = INT32_MAX;
      int gnfds = g_main_context_query (gcontext_, gpriority, &gtimeout, mk_gpollfd (&pfda[gfirstfd]), pfda.size() - gfirstfd);
      while (gnfds >= 0 && size_t (gnfds) != pfda.size() - gfirstfd)
        {
          pfda.resize (gfirstfd + gnfds);
          gtimeout = INT32_MAX;
          gnfds = g_main_context_query (gcontext_, gpriority, &gtimeout, mk_gpollfd (&pfda[gfirstfd]), pfda.size() - gfirstfd);
        }
      if (gtimeout >= 0)
        state.timeout_usecs = MIN (state.timeout_usecs, gtimeout * int64 (1000));
#endif
    }
  // poll file descriptors
  int64 timeout_msecs = state.timeout_usecs / 1000;
  if (state.timeout_usecs > 0 && timeout_msecs <= 0)
    timeout_msecs = 1;
  if (!may_block || any_dispatchable)
    timeout_msecs = 0;
  state.timeout_usecs = 0;
  int presult;
  do
    presult = poll ((struct pollfd*) &pfda[0], pfda.size(), std::min (timeout_msecs, int64 (2147483647))); // INT_MAX
  while (presult < 0 && errno == EAGAIN); // EINTR may indicate a signal
  if (presult < 0 && errno != EINTR)
    warning ("LoopImpl: poll() failed: %s", strerror());
  else if (pfda[wakeup_idx].revents)
    eventfd_.flush(); // restart queueing wakeups, possibly triggered by dispatching
  // check
  state.phase = state.CHECK;
  state.current_time_usecs = timestamp_realtime();
  int16 max_dispatch_priority = -32768;
  adispatchable |= check_sources_Lm (state, pfda);
  if (adispatchable)
    {
      any_dispatchable = true;
      max_dispatch_priority = std::max (max_dispatch_priority, dispatch_priority_);
    }
  // check GLib
  if (ASE_UNLIKELY (gcontext_))
    {
#ifdef  __G_LIB_H__
      gdispatchable = g_main_context_check (gcontext_, gpriority, mk_gpollfd (&pfda[gfirstfd]), pfda.size() - gfirstfd);
      any_dispatchable |= gdispatchable;
#endif
    }
  // dispatch
  if (may_dispatch && any_dispatchable)
    {
      state.phase = state.DISPATCH;
      if (gdispatchable && (!adispatchable || (rr_index_++ & 1)))
        {
#ifdef  __G_LIB_H__
          g_main_context_dispatch (gcontext_);
#endif
        }
      else if (adispatchable)
        dispatch_source_Lm (state); // passes on shared_ptr to keep alive while locked
    }
  // cleanup
  state.phase = state.NONE;
  unpoll_sources_U(); // unlocked
  return any_dispatchable; // need to dispatch or recheck
}

// === LoopSource ===
LoopSource::LoopSource () :
  loop_ (NULL),
  pfds_ (NULL),
  id_ (LoopID::INVALID),
  priority_ (UNDEFINED_PRIORITY),
  loop_state_ (0),
  may_recurse_ (0),
  dispatching_ (0),
  was_dispatching_ (0),
  primary_ (true)
{}

uint
LoopSource::n_pfds ()
{
  uint i = 0;
  if (pfds_)
    while (pfds_[i].pfd)
      i++;
  return i;
}

void
LoopSource::may_recurse (bool may_recurse)
{
  may_recurse_ = may_recurse;
}

bool
LoopSource::may_recurse () const
{
  return may_recurse_;
}

bool
LoopSource::primary () const
{
  return primary_;
}

void
LoopSource::primary (bool is_primary)
{
  primary_ = is_primary;
}

bool
LoopSource::recursion () const
{
  return dispatching_ && was_dispatching_;
}

void
LoopSource::add_poll (PollFD *const pfd)
{
  const uint idx = n_pfds();
  uint npfds = idx + 1;
  pfds_ = (typeof (pfds_)) realloc (pfds_, sizeof (pfds_[0]) * (npfds + 1));
  if (!pfds_)
    fatal_error ("LoopSource: out of memory");
  pfds_[npfds].idx = 4294967295U; // UINT_MAX
  pfds_[npfds].pfd = NULL;
  pfds_[idx].idx = 4294967295U; // UINT_MAX
  pfds_[idx].pfd = pfd;
}

void
LoopSource::remove_poll (PollFD *const pfd)
{
  uint idx, npfds = n_pfds();
  for (idx = 0; idx < npfds; idx++)
    if (pfds_[idx].pfd == pfd)
      break;
  if (idx < npfds)
    {
      pfds_[idx].idx = 4294967295U; // UINT_MAX
      pfds_[idx].pfd = pfds_[npfds - 1].pfd;
      pfds_[idx].idx = pfds_[npfds - 1].idx;
      pfds_[npfds - 1].idx = 4294967295U; // UINT_MAX
      pfds_[npfds - 1].pfd = NULL;
    }
  else
    warning ("LoopSource: unremovable PollFD: %p (fd=%d)", pfd, pfd->fd);
}

void
LoopSource::destroy ()
{}

void
LoopSource::loop_remove ()
{
  if (loop_)
    loop_->cancel (source_id());
}

LoopSource::~LoopSource ()
{
  assert_return (loop_ == NULL);
  if (pfds_)
    free (pfds_);
}

// == DispatcherSource ==
DispatcherSource::DispatcherSource (const DispatcherSlot &slot) :
  slot_ (slot)
{}

DispatcherSource::~DispatcherSource ()
{
  slot_ = NULL;
}

bool
DispatcherSource::prepare (const LoopState &state, int64 *timeout_usecs_p)
{
  return slot_ (state);
}

bool
DispatcherSource::check (const LoopState &state)
{
  return slot_ (state);
}

bool
DispatcherSource::dispatch (const LoopState &state)
{
  return slot_ (state);
}

void
DispatcherSource::destroy()
{
  LoopState state;
  state.phase = state.DESTROY;
  slot_ (state);
}

// == USignalSource ==
USignalSource::USignalSource (int8 signum, const USignalSlot &slot) :
  slot_ (slot), signum_ (signum)
{
  const uint s = 128 + signum_;
  index_ = s / 32;
  shift_ = s % 32;
}

USignalSource::~USignalSource ()
{
  slot_ = NULL;
}

static std::atomic<uint32> usignals_notified[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/// Flag a unix signal being raised, this function may be called from any thread at any time.
void
USignalSource::raise  (int8 signum)
{
  const uint s = 128 + signum;
  const uint index = s / 32;
  const uint shift = s % 32;
  usignals_notified[index] |= 1 << shift;
}

bool
USignalSource::prepare (const LoopState &state, int64 *timeout_usecs_p)
{
  return usignals_notified[index_] & (1 << shift_);
}

bool
USignalSource::check (const LoopState &state)
{
  return usignals_notified[index_] & (1 << shift_);
}

bool
USignalSource::dispatch (const LoopState &state)
{
  usignals_notified[index_] &= ~(1 << shift_);
  return slot_ (signum_);
}

void
USignalSource::destroy()
{}

static std::array<char,12>
write_uint (uint32_t i)
{
  std::array<char,12> a;
  char *c = &a.back();
  ASE_ASSERT (c>=&a[0] && c<&a[a.size()]);
  *c-- = 0;
  *c = '0' + (i % 10);
  i /= 10;
  while (i != 0) {
    *(--c) = '0' + (i % 10);
    i /= 10;
  }
  if (c > &a[0])
    memmove (&a[0], c, &a.back() + 1 - c);
  ASE_ASSERT (c>=&a[0] && c<&a[a.size()]);
  return a;
}

void
USignalSource::install_sigaction (int8 signum)
{
  struct sigaction action;
  action.sa_handler = [] (int signum) {
    if (0) { // DEBUG
      constexpr size_t N = 1024;
      char buf[N] = __FILE__ ":";
      strncat (buf, &write_uint (__LINE__)[0], N);
      strncat (buf, ": sa_handler: signal=", N);
      strncat (buf, &write_uint (signum)[0], N);
      strncat (buf, "\n", N);
      ::write (2, buf, strlen (buf));
    }
    USignalSource::raise (signum);
  };
  sigemptyset (&action.sa_mask);
  action.sa_flags = SA_NOMASK;
  sigaction (signum, &action, nullptr);
}

// === SigchldSource ===
static std::atomic<uint64_t> sigchld_counter = 0;

SigchldSource::SigchldSource (int64_t pid, const SigchldSlot &slot) :
  slot_ (slot), pid_ (pid)
{
  if (uint64_t unused = 0; sigchld_counter.compare_exchange_strong (unused, 1)) {
    struct sigaction action;
    action.sa_handler = [] (int signum) {
      sigchld_counter++;
    };
    sigemptyset (&action.sa_mask);
    action.sa_flags = SA_NOMASK;
    sigaction (SIGCHLD, &action, nullptr);
  }
}

SigchldSource::~SigchldSource()
{}

bool
SigchldSource::prepare (const LoopState &state, int64 *timeout_usecs_p)
{
  return pid_ && sigchld_counter_ != sigchld_counter;
}

bool
SigchldSource::check (const LoopState &state)
{
  return pid_ && sigchld_counter_ != sigchld_counter;
}

bool
SigchldSource::dispatch (const LoopState &state)
{
  if (pid_) {
    sigchld_counter_ = sigchld_counter;
    // Use pid_ to avoid reaping unknown children
    int status = 0;
    const pid_t child_pid = wait4 (pid_, &status, WNOHANG, nullptr);
    if (child_pid > 0) {
      slot_ (pid_, status);
#if 0
      struct rusage ru {}; // wait4 (..., &ru);
      printf ("  Child Pid %d user time: %ld.%06ld sec\n", child_pid, ru.ru_utime.tv_sec, ru.ru_utime.tv_usec);
      printf ("  System time: %ld.%06ld sec\n", ru.ru_stime.tv_sec, ru.ru_stime.tv_usec);
      printf ("  Max RSS: %ld KB\n", ru.ru_maxrss);
      printf ("  Page faults: %ld\n", ru.ru_minflt);
      printf ("  I/O operations: %ld\n", ru.ru_inblock + ru.ru_oublock);
      printf ("  Voluntary context switches: %ld\n", ru.ru_nvcsw);
      printf ("  Involuntary context switches: %ld\n", ru.ru_nivcsw);
      printf ("\n");
#endif
      if (WIFEXITED (status) || WIFSIGNALED (status)) {
        // Child exited
        pid_ = 0;
        return false; // destroy
      }
    }
  }
  return true; // keep_alive
}

void
SigchldSource::destroy ()
{
  pid_ = 0;
}

// == TimedSource ==
TimedSource::TimedSource (const VoidSlot &slot, uint initial_interval_msecs, uint repeat_interval_msecs) :
  expiration_usecs_ (timestamp_realtime() + 1000ULL * initial_interval_msecs),
  interval_msecs_ (repeat_interval_msecs), first_interval_ (true),
  oneshot_ (true), void_slot_ (slot)
{}

TimedSource::TimedSource (const BoolSlot &slot, uint initial_interval_msecs, uint repeat_interval_msecs) :
  expiration_usecs_ (timestamp_realtime() + 1000ULL * initial_interval_msecs),
  interval_msecs_ (repeat_interval_msecs), first_interval_ (true),
  oneshot_ (false), bool_slot_ (slot)
{}

bool
TimedSource::prepare (const LoopState &state, int64 *timeout_usecs_p)
{
  if (state.current_time_usecs >= expiration_usecs_)
    return true;                                            /* timeout expired */
  if (!first_interval_)
    {
      uint64 interval = interval_msecs_ * 1000ULL;
      if (state.current_time_usecs + interval < expiration_usecs_)
        expiration_usecs_ = state.current_time_usecs + interval; /* clock warped back in time */
    }
  *timeout_usecs_p = std::min (expiration_usecs_ - state.current_time_usecs, uint64 (2147483647)); // INT_MAX
  return 0 == *timeout_usecs_p;
}

bool
TimedSource::check (const LoopState &state)
{
  return state.current_time_usecs >= expiration_usecs_;
}

bool
TimedSource::dispatch (const LoopState &state)
{
  bool repeat = false;
  first_interval_ = false;
  if (oneshot_ && void_slot_ != NULL)
    void_slot_ ();
  else if (!oneshot_ && bool_slot_ != NULL)
    repeat = bool_slot_ ();
  if (repeat)
    expiration_usecs_ = timestamp_realtime() + 1000ULL * interval_msecs_;
  return repeat;
}

TimedSource::~TimedSource ()
{
  if (oneshot_)
    void_slot_.~VoidSlot();
  else
    bool_slot_.~BoolSlot();
}

// == PollFDSource ==
/*! @class PollFDSource
 * A PollFDSource can be used to execute a callback function from the main loop,
 * depending on certain file descriptor states.
 * The modes supported for polling the file descriptor are as follows:
 * @li @c "r" - poll readable (POLLIN)
 * @li @c "w" - poll writable (POLLOUT)
 * @li @c "p" - priority data (POLLPRI)
 * @li @c "d" - priority band writable (POLLWRBAND)
 * @li @c "b" - set fd blocking
 * @li @c "B" - set fd non-blocking
 * @li @c "C" - prevent auto close on destroy
 */
PollFDSource::PollFDSource (const BPfdSlot &slot, int fd, const String &mode) :
  pfd_ ((PollFD) { fd, 0, 0 }),
  never_close_ (strchr (mode.c_str(), 'C') != NULL),
  oneshot_ (false), bool_poll_slot_ (slot)
{
  construct (mode);
}

PollFDSource::PollFDSource (const VPfdSlot &slot, int fd, const String &mode) :
  pfd_ ((PollFD) { fd, 0, 0 }),
  never_close_ (strchr (mode.c_str(), 'C') != NULL),
  oneshot_ (true), void_poll_slot_ (slot)
{
  construct (mode);
}

void
PollFDSource::construct (const String &mode)
{
  add_poll (&pfd_);
  pfd_.events |= strchr (mode.c_str(), 'w') ? PollFD::OUT : 0;
  pfd_.events |= strchr (mode.c_str(), 'r') ? PollFD::IN : 0;
  pfd_.events |= strchr (mode.c_str(), 'p') ? PollFD::PRI : 0;
  pfd_.events |= strchr (mode.c_str(), 'd') ? PollFD::WRBAND : 0;
  if (pfd_.fd >= 0)
    {
      const long lflags = fcntl (pfd_.fd, F_GETFL, 0);
      long nflags = lflags;
      if (strchr (mode.c_str(), 'b'))
        nflags &= ~long (O_NONBLOCK);
      else if (strchr (mode.c_str(), 'B'))
        nflags |= O_NONBLOCK;
      if (nflags != lflags)
        {
          int err;
          do
            err = fcntl (pfd_.fd, F_SETFL, nflags);
          while (err < 0 && (errno == EINTR || errno == EAGAIN));
        }
    }
}

bool
PollFDSource::prepare (const LoopState &state, int64 *timeout_usecs_p)
{
  pfd_.revents = 0;
  return pfd_.fd < 0;
}

bool
PollFDSource::check (const LoopState &state)
{
  return pfd_.fd < 0 || pfd_.revents != 0;
}

bool
PollFDSource::dispatch (const LoopState &state)
{
  bool keep_alive = !oneshot_;
  if (oneshot_ && void_poll_slot_ != NULL)
    void_poll_slot_ (pfd_);
  else if (!oneshot_ && bool_poll_slot_ != NULL)
    keep_alive = bool_poll_slot_ (pfd_);
  /* close down */
  if (!keep_alive)
    {
      if (!never_close_ && pfd_.fd >= 0)
        close (pfd_.fd);
      pfd_.fd = -1;
    }
  return keep_alive;
}

void
PollFDSource::destroy()
{
  /* close down */
  if (!never_close_ && pfd_.fd >= 0)
    close (pfd_.fd);
  pfd_.fd = -1;
}

PollFDSource::~PollFDSource ()
{
  if (oneshot_)
    void_poll_slot_.~VPfdSlot();
  else
    bool_poll_slot_.~BPfdSlot();
}

} // Ase

// == Loop Description ==
/*! @page eventloops    Event Loops and Event Sources
  Ase <a href="http://en.wikipedia.org/wiki/Event_loop">event loops</a>
  are a programming facility to execute callback handlers (dispatch event sources) according to expiring Timers,
  IO events or arbitrary other conditions.
  A Ase::Loop is retrieved via Ase::Loop::current(), which returns a thread-local singleton loop. Callbacks or other
  event sources are added to it via Ase::Loop::add_source() and related functions like Ase::Loop::exec_once().
  Once a main loop is created and its callbacks are added, it can be run via Ase::Loop::run(): @code
  int exit_code = loop.run();
  @endcode
  Alternatively, for manual control, the loop can be iterated: @code
  * while (!loop.finishable())
  *   loop.iterate (true);
  @endcode
  Ase::Loop::iterate() finds a source that needs immediate dispatching and dispatches it.
  If no source was found, it monitors the source list's PollFD descriptors for events, and finds dispatchable
  sources based on newly incoming events on the descriptors.
  If multiple sources need dispatching, they are handled according to their priorities (see Ase::Loop::add_source())
  and at the same priority, sources are dispatched in round-robin fashion.

  Traits of the Ase::Loop class:
  @li Loops are thread safe, so any thread may add or remove sources to a loop at any time, regardless of which thread
  is currently running the loop.
  @li Sources added to a loop may be flagged as "primary" (see Ase::LoopSource::primary()),
  to keep the loop from exiting when using manual iteration with `finishable()`. This is used to distinguish background jobs,
  e.g. updating a window's progress bar,
  from primary jobs, like processing events on the main window.
  Sticking with the example, a window's event loop should be exited if the window vanishes, but not when it's
  progress bar stopped updating.

  Loop integration of a Ase::LoopSource class:
  @li First, prepare() is called on a source. Returning true flags the source as ready for immediate dispatching.
  @li Second, poll(2) monitors all PollFD file descriptors of the source (see Ase::LoopSource::add_poll()).
  @li Third, check() is called for the source to determine if dispatching is needed based on PollFD states.
  @li Fourth, the source's dispatch() method is called if it returned true from either prepare() or check(). If multiple sources are
  ready to be dispatched, the entire process may be repeated several times for other sources before a particular source is finally dispatched,
  starting with a new call to prepare().
*/
