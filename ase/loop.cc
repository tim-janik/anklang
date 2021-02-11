// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "loop.hh"
#include "utils.hh"
#include "platform.hh"
#include "internal.hh"
#include <sys/poll.h>
#include <errno.h>
#include <atomic>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>
#include <list>
#include <cstring>

namespace Ase {

enum {
  WAITING             = 0,
  PREPARED,
  NEEDS_DISPATCH,
};

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
static volatile int global_id_counter = 65536;
static uint
alloc_id ()
{
  uint id = __sync_fetch_and_add (&global_id_counter, +1);
  if (!id)
    fatal_error ("EventLoop: id counter overflow, please report"); // TODO: use global ID allcoator?
  return id;
}

static void
release_id (uint id)
{
  assert_return (id != 0);
  // TODO: use global ID allcoator?
}

// === QuickArray ===
template<class Data>
class QuickArray {
  Data  *data_;
  uint   n_elements_;
  uint   n_reserved_;
  Data  *reserved_;
  template<class D> void swap (D &a, D &b) { D t = a; a = b; b = t; }
public:
  typedef Data* iterator;
  QuickArray (uint n_reserved, Data *reserved) : data_ (reserved), n_elements_ (0), n_reserved_ (n_reserved), reserved_ (reserved) {}
  ~QuickArray()                         { if (ISLIKELY (data_) && UNLIKELY (data_ != reserved_)) free (data_); }
  uint        size       () const       { return n_elements_; }
  uint        capacity   () const       { return std::max (n_elements_, n_reserved_); }
  bool        empty      () const       { return n_elements_ == 0; }
  Data*       data       () const       { return data_; }
  Data&       operator[] (uint n)       { return data_[n]; }
  const Data& operator[] (uint n) const { return data_[n]; }
  iterator    begin      ()             { return &data_[0]; }
  iterator    end        ()             { return &data_[n_elements_]; }
  void        shrink     (uint n)       { n_elements_ = std::min (n_elements_, n); }
  void        swap       (QuickArray &o)
  {
    swap (data_,       o.data_);
    swap (n_elements_, o.n_elements_);
    swap (n_reserved_, o.n_reserved_);
    swap (reserved_,   o.reserved_);
  }
  void        push       (const Data &d)
  {
    const uint idx = n_elements_;
    resize (idx + 1);
    data_[idx] = d;
  }
  void        resize     (uint n)
  {
    if (n <= capacity())
      {
        n_elements_ = n;
        return;
      }
    // n > n_reserved_ && n > n_elements_
    const size_t sz = n * sizeof (Data);
    const bool migrate_reserved = UNLIKELY (data_ == reserved_);        // migrate from reserved to malloced
    Data *mem = (Data*) (migrate_reserved ? malloc (sz) : realloc (data_, sz));
    if (UNLIKELY (!mem))
      fatal_error ("OOM");
    if (migrate_reserved)
      {
        memcpy (mem, data_, n_elements_ * sizeof (Data));
        reserved_ = NULL;
        n_reserved_ = 0;
      }
    data_ = mem;
    n_elements_ = n;
  }
};
struct EventLoop::QuickPfdArray : public QuickArray<PollFD> {
  QuickPfdArray (uint n_reserved, PollFD *reserved) : QuickArray (n_reserved, reserved) {}
};
struct QuickSourcePArray : public QuickArray<EventSourceP*> {
  QuickSourcePArray (uint n_reserved, EventSourceP **reserved) : QuickArray (n_reserved, reserved) {}
};

// === EventLoop ===
EventLoop::EventLoop (MainLoop &main) :
  main_loop_ (&main), dispatch_priority_ (0), primary_ (false)
{
  poll_sources_.reserve (7);
  // we cannot *use* main_loop_ yet, because we might be called from within MainLoop::MainLoop(), see SubLoop()
  assert_return (main_loop_ && main_loop_->main_loop_); // sanity checks
}

EventLoop::~EventLoop ()
{
  unpoll_sources_U();
  // we cannot *use* main_loop_ anymore, because we might be called from within MainLoop::MainLoop(), see ~SubLoop()
}

inline EventSourceP&
EventLoop::find_first_L()
{
  static EventSourceP null_source;
  return sources_.empty() ? null_source : sources_[0];
}

inline EventSourceP&
EventLoop::find_source_L (uint id)
{
  for (SourceList::iterator lit = sources_.begin(); lit != sources_.end(); lit++)
    if (id == (*lit)->id_)
      return *lit;
  static EventSourceP null_source;
  return null_source;
}

bool
EventLoop::has_primary_L()
{
  if (primary_)
    return true;
  for (SourceList::iterator lit = sources_.begin(); lit != sources_.end(); lit++)
    if ((*lit)->primary())
      return true;
  return false;
}

bool
EventLoop::has_primary()
{
  std::lock_guard<std::mutex> locker (main_loop_->mutex());
  return has_primary_L();
}

bool
EventLoop::flag_primary (bool on)
{
  std::lock_guard<std::mutex> locker (main_loop_->mutex());
  const bool was_primary = primary_;
  primary_ = on;
  if (primary_ != was_primary)
    wakeup();
  return was_primary;
}

MainLoop*
EventLoop::main_loop () const
{
  return main_loop_;
}

static const int16 UNDEFINED_PRIORITY = -32768;

uint
EventLoop::add (EventSourceP source, int priority)
{
  static_assert (UNDEFINED_PRIORITY < 1, "");
  assert_return (priority >= 1 && priority <= PRIORITY_CEILING, 0);
  assert_return (source != NULL, 0);
  assert_return (source->loop_ == NULL, 0);
  source->loop_ = this;
  source->id_ = alloc_id();
  source->loop_state_ = WAITING;
  source->priority_ = priority;
  {
    std::lock_guard<std::mutex> locker (main_loop_->mutex());
    sources_.push_back (source);
  }
  wakeup();
  return source->id_;
}

void
EventLoop::remove_source_Lm (EventSourceP source)
{
  std::mutex &LOCK = main_loop_->mutex();
  assert_return (source->loop_ == this);
  source->loop_ = NULL;
  source->loop_state_ = WAITING;
  auto pos = find (sources_.begin(), sources_.end(), source);
  assert_return (pos != sources_.end());
  sources_.erase (pos);
  release_id (source->id_);
  source->id_ = 0;
  LOCK.unlock();
  source->destroy();
  LOCK.lock();
}

bool
EventLoop::try_remove (uint id)
{
  {
    std::lock_guard<std::mutex> locker (main_loop_->mutex());
    EventSourceP &source = find_source_L (id);
    if (!source)
      return false;
    remove_source_Lm (source);
  }
  wakeup();
  return true;
}

void
EventLoop::remove (uint id)
{
  if (!try_remove (id))
    warning ("%s: failed to remove loop source: %u", __func__, id);
}

/* void EventLoop::change_priority (EventSource *source, int priority) {
 * // ensure that source belongs to this
 * // reset all source->pfds[].idx = UINT_MAX
 * // unlink source
 * // poke priority
 * // re-add source
 */

void
EventLoop::kill_sources_Lm()
{
  for (;;)
    {
      EventSourceP &source = find_first_L();
      if (source == NULL)
        break;
      remove_source_Lm (source);
    }
  std::mutex &LOCK = main_loop_->mutex();
  LOCK.unlock();
  unpoll_sources_U(); // unlocked
  LOCK.lock();
}

/** Remove all sources from a loop and prevent any further execution.
 * The destroy_loop() method removes all sources from a loop and in
 * case of a sub EventLoop (see create_sub_loop()) removes it from its
 * associated main loop. Calling destroy_loop() on a main loop also
 * calls destroy_loop() for all its sub loops.
 * Note that MainLoop objects are artificially kept alive until
 * MainLoop::destroy_loop() is called, so calling destroy_loop() is
 * mandatory for MainLoop objects to prevent object leaks.
 * This method must be called only once on a loop.
 */
void
EventLoop::destroy_loop()
{
  assert_return (main_loop_ != NULL);
  // guard main_loop_ pointer *before* locking, so dtor is called after unlock
  EventLoopP main_loop_guard = shared_ptr_cast<EventLoop*> (main_loop_);
  std::lock_guard<std::mutex> locker (main_loop_->mutex());
  if (this != main_loop_)
    main_loop_->kill_loop_Lm (*this);
  else
    main_loop_->kill_loops_Lm();
  assert_return (main_loop_ == NULL);
}

void
EventLoop::wakeup ()
{
  // this needs to work unlocked
  main_loop_->wakeup_poll();
}

// === MainLoop ===
MainLoop::MainLoop() :
  EventLoop (*this), // sets *this as MainLoop on self
  rr_index_ (0), running_ (false), has_quit_ (false), quit_code_ (0), gcontext_ (NULL)
{
  std::lock_guard<std::mutex> locker (main_loop_->mutex());
  const int err = eventfd_.open();
  if (err < 0)
    fatal_error ("MainLoop: failed to create wakeup pipe: %s", strerror (-err));
  // has_quit_ and eventfd_ need to be setup here, so calling quit() before run() works
}

/** Create a new main loop object, users can run or iterate this loop directly.
 * Note that MainLoop objects have special lifetime semantics that keep them
 * alive until they are explicitely destroyed with destroy_loop().
 */
MainLoopP
MainLoop::create ()
{
  MainLoopP main_loop = FriendAllocator<MainLoop>::make_shared();
  std::lock_guard<std::mutex> locker (main_loop->mutex());
  main_loop->add_loop_L (*main_loop);
  return main_loop;
}

MainLoop::~MainLoop()
{
  set_g_main_context (NULL); // acquires mutex_
  std::lock_guard<std::mutex> locker (mutex_);
  if (main_loop_)
    kill_loops_Lm();
  assert_return (loops_.empty() == true);
}

void
MainLoop::wakeup_poll()
{
  if (eventfd_.opened())
    eventfd_.wakeup();
}

void
MainLoop::add_loop_L (EventLoop &loop)
{
  assert_return (this == loop.main_loop_);
  loops_.push_back (shared_ptr_cast<EventLoop> (&loop));
  wakeup_poll();
}

void
MainLoop::kill_loop_Lm (EventLoop &loop)
{
  assert_return (this == loop.main_loop_);
  loop.kill_sources_Lm();
  if (loop.main_loop_) // guard against nested kill_loop_Lm (same) calls
    {
      loop.main_loop_ = NULL;
      vector<EventLoopP>::iterator it = std::find_if (loops_.begin(), loops_.end(),
                                                      [&loop] (EventLoopP &lp) { return lp.get() == &loop; });
      if (this == &loop) // MainLoop->destroy_loop()
        {
          if (loops_.size()) // MainLoop must be the last loop to be destroyed
            assert_return (loops_[0].get() == this);
        }
      else
        assert_return (it != loops_.end());
      if (it != loops_.end())
        loops_.erase (it);
      wakeup_poll();
    }
}

void
MainLoop::kill_loops_Lm()
{
  while (loops_.size() > 1 || loops_[0].get() != this)
    {
      EventLoopP loop = loops_[0].get() != this ? loops_[0] : loops_[loops_.size() - 1];
      kill_loop_Lm (*loop);
    }
  kill_loop_Lm (*this);
}

int
MainLoop::run ()
{
  EventLoopP main_loop_guard = shared_ptr_cast<EventLoop> (this);
  std::lock_guard<std::mutex> locker (mutex_);
  LoopState state;
  running_ = !has_quit_;
  while (ISLIKELY (running_))
    iterate_loops_Lm (state, true, true);
  const int last_quit_code = quit_code_;
  running_ = false;
  has_quit_ = false;    // allow loop resumption
  quit_code_ = 0;       // allow loop resumption
  return last_quit_code;
}

bool
MainLoop::running ()
{
  std::lock_guard<std::mutex> locker (mutex_);
  return running_;
}

void
MainLoop::quit (int quit_code)
{
  std::lock_guard<std::mutex> locker (mutex_);
  quit_code_ = quit_code;
  has_quit_ = true;     // cancel run() upfront, or
  running_ = false;     // interrupt current iteration
  wakeup();
}

bool
MainLoop::finishable_L()
{
  // loop list shouldn't be modified by querying finishable state
  bool found_primary = primary_;
  for (size_t i = 0; !found_primary && i < loops_.size(); i++)
    if (loops_[i]->has_primary_L())
      found_primary = true;
  return !found_primary; // finishable if no primary sources remain
}

bool
MainLoop::finishable()
{
  std::lock_guard<std::mutex> locker (mutex_);
  return finishable_L();
}

/**
 * @param may_block     If true, iterate() will wait for events occour.
 *
 * MainLoop::iterate() is the heart of the main event loop. For loop iteration,
 * all event sources are polled for incoming events. Then dispatchable sources are
 * picked one per iteration and dispatched in round-robin fashion.
 * If no sources need immediate dispatching and @a may_block is true, iterate() will
 * wait for events to become available.
 * @returns Whether more sources need immediate dispatching.
 */
bool
MainLoop::iterate (bool may_block)
{
  EventLoopP main_loop_guard = shared_ptr_cast<EventLoop> (this);
  std::lock_guard<std::mutex> locker (mutex_);
  LoopState state;
  const bool was_running = running_;    // guard for recursion
  running_ = true;
  const bool sources_pending = iterate_loops_Lm (state, may_block, true);
  running_ = was_running && !has_quit_;
  return sources_pending;
}

void
MainLoop::iterate_pending()
{
  EventLoopP main_loop_guard = shared_ptr_cast<EventLoop> (this);
  std::lock_guard<std::mutex> locker (mutex_);
  LoopState state;
  const bool was_running = running_;    // guard for recursion
  running_ = true;
  while (ISLIKELY (running_))
    if (!iterate_loops_Lm (state, false, true))
      break;
  running_ = was_running && !has_quit_;
}

bool
MainLoop::pending()
{
  std::lock_guard<std::mutex> locker (mutex_);
  LoopState state;
  EventLoopP main_loop_guard = shared_ptr_cast<EventLoop> (this);
  return iterate_loops_Lm (state, false, false);
}

bool
MainLoop::set_g_main_context (GlibGMainContext *glib_main_context)
{
#ifdef  __G_LIB_H__
  std::lock_guard<std::mutex> locker (mutex_);
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
EventLoop::unpoll_sources_U() // must be unlocked!
{
  // clear poll sources
  poll_sources_.resize (0);
}

void
EventLoop::collect_sources_Lm (LoopState &state)
{
  // enforce clean slate
  if (UNLIKELY (!poll_sources_.empty()))
    {
      std::mutex &LOCK = main_loop_->mutex();
      LOCK.unlock();
      unpoll_sources_U(); // unlocked
      LOCK.lock();
      assert_return (poll_sources_.empty());
    }
  if (UNLIKELY (!state.seen_primary && primary_))
    state.seen_primary = true;
  EventSourceP* arraymem[7]; // using a vector+malloc here shows up in the profiles
  QuickSourcePArray poll_candidates (ARRAY_SIZE (arraymem), arraymem);
  // determine dispatch priority & collect sources for preparing
  dispatch_priority_ = UNDEFINED_PRIORITY; // initially, consider sources at *all* priorities
  for (SourceList::iterator lit = sources_.begin(); lit != sources_.end(); lit++)
    {
      EventSource &source = **lit;
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
        poll_candidates.push (&*lit);                           // collect only, adding ref() later
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
EventLoop::prepare_sources_Lm (LoopState &state, int64 *timeout_usecs, QuickPfdArray &pfda)
{
  std::mutex &LOCK = main_loop_->mutex();
  // prepare sources, up to NEEDS_DISPATCH priority
  for (auto lit = poll_sources_.begin(); lit != poll_sources_.end(); lit++)
    {
      EventSource &source = **lit;
      if (source.loop_ != this) // test undestroyed
        continue;
      int64 timeout = -1;
      LOCK.unlock();
      const bool need_dispatch = source.prepare (state, &timeout);
      LOCK.lock();
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
        *timeout_usecs = std::min (*timeout_usecs, timeout);
      uint npfds = source.n_pfds();
      for (uint i = 0; i < npfds; i++)
        if (source.pfds_[i].pfd->fd >= 0)
          {
            uint idx = pfda.size();
            source.pfds_[i].idx = idx;
            pfda.push (*source.pfds_[i].pfd);
            pfda[idx].revents = 0;
          }
        else
          source.pfds_[i].idx = 4294967295U; // UINT_MAX
    }
  return dispatch_priority_ > UNDEFINED_PRIORITY;
}

bool
EventLoop::check_sources_Lm (LoopState &state, const QuickPfdArray &pfda)
{
  std::mutex &LOCK = main_loop_->mutex();
  // check polled sources
  for (auto lit = poll_sources_.begin(); lit != poll_sources_.end(); lit++)
    {
      EventSource &source = **lit;
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
      LOCK.unlock();
      bool need_dispatch = source.check (state);
      LOCK.lock();
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
EventLoop::dispatch_source_Lm (LoopState &state)
{
  std::mutex &LOCK = main_loop_->mutex();
  // find a source to dispatch at dispatch_priority_
  EventSourceP dispatch_source = NULL;                  // shared_ptr to keep alive even if everything else is destroyed
  for (auto lit = poll_sources_.begin(); lit != poll_sources_.end(); lit++)
    {
      EventSourceP &source = *lit;
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
      LOCK.unlock();
      const bool keep_alive = dispatch_source->dispatch (state);
      LOCK.lock();
      dispatch_source->dispatching_ = dispatch_source->was_dispatching_;
      dispatch_source->was_dispatching_ = old_was_dispatching;
      if (dispatch_source->loop_ == this && !keep_alive)
        remove_source_Lm (dispatch_source);
    }
}

bool
MainLoop::iterate_loops_Lm (LoopState &state, bool may_block, bool may_dispatch)
{
  assert_return (state.phase == state.NONE, false);
  std::mutex &LOCK = main_loop_->mutex();
  int64 timeout_usecs = INT64_MAX;
  PollFD reserved_pfd_mem[7];   // store PollFD array in stack memory, to reduce malloc overhead
  QuickPfdArray pfda (ARRAY_SIZE (reserved_pfd_mem), reserved_pfd_mem); // pfda.size() == 0
  // allow poll wakeups
  const PollFD wakeup = { eventfd_.inputfd(), PollFD::IN, 0 };
  const uint wakeup_idx = 0; // wakeup_idx = pfda.size();
  pfda.push (wakeup);
  // create pollable loop list
  const size_t nrloops = loops_.size(); // number of Ase loops, *without* gcontext_
  ASE_DECLARE_VLA (EventLoopP, loops, nrloops); // EventLoopP loops[nrloops];
  for (size_t i = 0; i < nrloops; i++)
    loops[i] = loops_[i];
  // collect
  state.phase = state.COLLECT;
  state.seen_primary = false;
  for (size_t i = 0; i < nrloops; i++)
    loops[i]->collect_sources_Lm (state);
  // prepare
  bool any_dispatchable = false;
  state.phase = state.PREPARE;
  state.current_time_usecs = timestamp_realtime();
  bool dispatchable[nrloops + 1];        // +1 for gcontext_
  for (size_t i = 0; i < nrloops; i++)
    {
      dispatchable[i] = loops[i]->prepare_sources_Lm (state, &timeout_usecs, pfda);
      any_dispatchable |= dispatchable[i];
    }
  // prepare GLib
  ASE_UNUSED const int gfirstfd = pfda.size();
  ASE_UNUSED int gpriority = INT32_MIN;
  if (ASE_UNLIKELY (gcontext_))
    {
#ifdef  __G_LIB_H__
      dispatchable[nrloops] = g_main_context_prepare (gcontext_, &gpriority) != 0;
      any_dispatchable |= dispatchable[nrloops];
      int gtimeout = INT32_MAX;
      pfda.resize (pfda.capacity());
      int gnfds = g_main_context_query (gcontext_, gpriority, &gtimeout, mk_gpollfd (&pfda[gfirstfd]), pfda.size() - gfirstfd);
      while (gnfds >= 0 && size_t (gnfds) != pfda.size() - gfirstfd)
        {
          pfda.resize (gfirstfd + gnfds);
          gtimeout = INT32_MAX;
          gnfds = g_main_context_query (gcontext_, gpriority, &gtimeout, mk_gpollfd (&pfda[gfirstfd]), pfda.size() - gfirstfd);
        }
      if (gtimeout >= 0)
        timeout_usecs = MIN (timeout_usecs, gtimeout * int64 (1000));
#endif
    }
  else
    dispatchable[nrloops] = false;
  // poll file descriptors
  int64 timeout_msecs = timeout_usecs / 1000;
  if (timeout_usecs > 0 && timeout_msecs <= 0)
    timeout_msecs = 1;
  if (!may_block || any_dispatchable)
    timeout_msecs = 0;
  LOCK.unlock();
  int presult;
  do
    presult = poll ((struct pollfd*) &pfda[0], pfda.size(), std::min (timeout_msecs, int64 (2147483647))); // INT_MAX
  while (presult < 0 && errno == EAGAIN); // EINTR may indicate a signal
  LOCK.lock();
  if (presult < 0 && errno != EINTR)
    warning ("MainLoop: poll() failed: %s", strerror());
  else if (pfda[wakeup_idx].revents)
    eventfd_.flush(); // restart queueing wakeups, possibly triggered by dispatching
  // check
  state.phase = state.CHECK;
  state.current_time_usecs = timestamp_realtime();
  int16 max_dispatch_priority = -32768;
  for (size_t i = 0; i < nrloops; i++)
    {
      dispatchable[i] |= loops[i]->check_sources_Lm (state, pfda);
      if (!dispatchable[i])
        continue;
      any_dispatchable = true;
      max_dispatch_priority = std::max (max_dispatch_priority, loops[i]->dispatch_priority_);
    }
  bool priority_ascension = false;      // flag for priority elevation between loops
  if (UNLIKELY (max_dispatch_priority >= PRIORITY_ASCENT) && any_dispatchable)
    priority_ascension = true;
  // check GLib
  if (ASE_UNLIKELY (gcontext_))
    {
#ifdef  __G_LIB_H__
      dispatchable[nrloops] = g_main_context_check (gcontext_, gpriority, mk_gpollfd (&pfda[gfirstfd]), pfda.size() - gfirstfd);
      any_dispatchable |= dispatchable[nrloops];
#endif
    }
  // dispatch
  if (may_dispatch && any_dispatchable)
    {
      const size_t gloop = ASE_UNLIKELY (gcontext_) && dispatchable[nrloops] ? 1 : 0;
      const size_t maxloops = nrloops + gloop; // +1 for gcontext_
      size_t index;
      while (true)    // pick loops in round-robin fashion
        {
          index = rr_index_++ % maxloops; // round-robin step
          if (!dispatchable[index])
            continue; // need to find next dispatchable
          if (index < nrloops && loops[index]->dispatch_priority_ < max_dispatch_priority)
            continue; // ignore loops without max-priority source, except for gcontext_ which can still benefit from round-robin
          if (priority_ascension && index >= nrloops)
            continue; // but there's no priority_ascension support for gcontext_
          break;      // DONE: index points to a dispatchable loop which meets priority requirements
        }
      state.phase = state.DISPATCH;
      if (index >= nrloops)
        {
#ifdef  __G_LIB_H__
          g_main_context_dispatch (gcontext_);
#endif
        }
      else
        loops[index]->dispatch_source_Lm (state); // passes on shared_ptr to keep alive wihle locked
    }
  // cleanup
  state.phase = state.NONE;
  LOCK.unlock();
  for (size_t i = 0; i < nrloops; i++)
    {
      loops[i]->unpoll_sources_U(); // unlocked
      loops[i] = NULL; // dtor, unlocked
    }
  LOCK.lock();
  return any_dispatchable; // need to dispatch or recheck
}

class SubLoop : public EventLoop {
  friend class FriendAllocator<SubLoop>;
public:
  SubLoop (MainLoopP main) :
    EventLoop (*main)
  {}
  ~SubLoop()
  {
    assert_return (main_loop_ == NULL);
  }
};

EventLoopP
MainLoop::create_sub_loop()
{
  std::lock_guard<std::mutex> locker (mutex());
  EventLoopP sub_loop = FriendAllocator<SubLoop>::make_shared (shared_ptr_cast<MainLoop> (this));
  this->add_loop_L (*sub_loop);
  return sub_loop;
}

// === EventLoop::LoopState ===
LoopState::LoopState() :
  current_time_usecs (0), phase (NONE), seen_primary (false)
{}

// === EventSource ===
EventSource::EventSource () :
  loop_ (NULL),
  pfds_ (NULL),
  id_ (0),
  priority_ (UNDEFINED_PRIORITY),
  loop_state_ (0),
  may_recurse_ (0),
  dispatching_ (0),
  was_dispatching_ (0),
  primary_ (0)
{}

uint
EventSource::n_pfds ()
{
  uint i = 0;
  if (pfds_)
    while (pfds_[i].pfd)
      i++;
  return i;
}

void
EventSource::may_recurse (bool may_recurse)
{
  may_recurse_ = may_recurse;
}

bool
EventSource::may_recurse () const
{
  return may_recurse_;
}

bool
EventSource::primary () const
{
  return primary_;
}

void
EventSource::primary (bool is_primary)
{
  primary_ = is_primary;
}

bool
EventSource::recursion () const
{
  return dispatching_ && was_dispatching_;
}

void
EventSource::add_poll (PollFD *const pfd)
{
  const uint idx = n_pfds();
  uint npfds = idx + 1;
  pfds_ = (typeof (pfds_)) realloc (pfds_, sizeof (pfds_[0]) * (npfds + 1));
  if (!pfds_)
    fatal_error ("EventSource: out of memory");
  pfds_[npfds].idx = 4294967295U; // UINT_MAX
  pfds_[npfds].pfd = NULL;
  pfds_[idx].idx = 4294967295U; // UINT_MAX
  pfds_[idx].pfd = pfd;
}

void
EventSource::remove_poll (PollFD *const pfd)
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
    warning ("EventSource: unremovable PollFD: %p (fd=%d)", pfd, pfd->fd);
}

void
EventSource::destroy ()
{}

void
EventSource::loop_remove ()
{
  if (loop_)
    loop_->try_remove (source_id());
}

EventSource::~EventSource ()
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

/// Flag a unix signalbeing raised, this function may be called from any thread at any time.
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
 * @li @c "w" - poll writable
 * @li @c "r" - poll readable
 * @li @c "p" - poll urgent redable
 * @li @c "b" - set fd blocking
 * @li @c "B" - set fd non-blocking
 * @li @c "E" - ignore erros (or auto destroy)
 * @li @c "H" - ignore hangup (or auto destroy)
 * @li @c "C" - prevent auto close on destroy
 */
PollFDSource::PollFDSource (const BPfdSlot &slot, int fd, const String &mode) :
  pfd_ ((PollFD) { fd, 0, 0 }),
  ignore_errors_ (strchr (mode.c_str(), 'E') != NULL),
  ignore_hangup_ (strchr (mode.c_str(), 'H') != NULL),
  never_close_ (strchr (mode.c_str(), 'C') != NULL),
  oneshot_ (false), bool_poll_slot_ (slot)
{
  construct (mode);
}

PollFDSource::PollFDSource (const VPfdSlot &slot, int fd, const String &mode) :
  pfd_ ((PollFD) { fd, 0, 0 }),
  ignore_errors_ (strchr (mode.c_str(), 'E') != NULL),
  ignore_hangup_ (strchr (mode.c_str(), 'H') != NULL),
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
  bool keep_alive = false;
  if (pfd_.fd >= 0 && (pfd_.revents & PollFD::NVAL))
    ; // close down
  else if (pfd_.fd >= 0 && !ignore_errors_ && (pfd_.revents & PollFD::ERR))
    ; // close down
  else if (pfd_.fd >= 0 && !ignore_hangup_ && (pfd_.revents & PollFD::HUP))
    ; // close down
  else if (oneshot_ && void_poll_slot_ != NULL)
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
  A Ase::EventLoop is created with Ase::MainLoop::_new() or Ase::MainLoop::create_sub_loop(). Callbacks or other
  event sources are added to it via Ase::EventLoop::add(), Ase::EventLoop::exec_normal() and related functions.
  Once a main loop is created and its callbacks are added, it can be run as: @code
  * while (!loop.finishable())
  *   loop.iterate (true);
  @endcode
  Ase::MainLoop::iterate() finds a source that immediately need dispatching and starts to dispatch it.
  If no source was found, it monitors the source list's PollFD descriptors for events, and finds dispatchable
  sources based on newly incoming events on the descriptors.
  If multiple sources need dispatching, they are handled according to their priorities (see Ase::EventLoop::add())
  and at the same priority, sources are dispatched in round-robin fashion.
  Calling Ase::MainLoop::iterate() also iterates over its sub loops, which allows to handle sources
  on several independently running loops within the same thread, usually used to associate one event loop with one window.

  Traits of the Ase::EventLoop class:
  @li The main loop and its sub loops are handled in round-robin fahsion, priorities below Ase::EventLoop::PRIORITY_ASCENT only apply internally to a loop.
  @li Loops are thread safe, so any thready may add or remove sources to a loop at any time, regardless of which thread
  is currently running the loop.
  @li Sources added to a loop may be flagged as "primary" (see Ase::EventSource::primary()),
  to keep the loop from exiting. This is used to distinguish background jobs, e.g. updating a window's progress bar,
  from primary jobs, like processing events on the main window.
  Sticking with the example, a window's event loop should be exited if the window vanishes, but not when it's
  progress bar stoped updating.

  Loop integration of a Ase::EventSource class:
  @li First, prepare() is called on a source, returning true here flags the source to be ready for immediate dispatching.
  @li Second, poll(2) monitors all PollFD file descriptors of the source (see Ase::EventSource::add_poll()).
  @li Third, check() is called for the source to check whether dispatching is needed depending on PollFD states.
  @li Fourth, the source is dispatched if it returened true from either prepare() or check(). If multiple sources are
  ready to be dispatched, the entire process may be repeated several times (after dispatching other sources),
  starting with a new call to prepare() before a particular source is finally dispatched.
 */
