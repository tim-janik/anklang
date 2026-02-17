// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#pragma once

#include <ase/utils.hh>
#include <chrono>

namespace Ase {

// === PollFD ===
struct PollFD   /// Mirrors struct pollfd for poll(3posix)
{
  int           fd;
  uint16        events;
  uint16        revents;
  /// Event types that can be polled for, set in .events, updated in .revents
  enum : uint {
    IN          = ASE_SYSVAL_POLLINIT[0],  ///< RDNORM || RDBAND
    PRI         = ASE_SYSVAL_POLLINIT[1],  ///< urgent data available
    OUT         = ASE_SYSVAL_POLLINIT[2],  ///< writing data will not block
    RDNORM      = ASE_SYSVAL_POLLINIT[3],  ///< reading data will not block
    RDBAND      = ASE_SYSVAL_POLLINIT[4],  ///< reading priority data will not block
    WRNORM      = ASE_SYSVAL_POLLINIT[5],  ///< writing data will not block
    WRBAND      = ASE_SYSVAL_POLLINIT[6],  ///< writing priority data will not block
    // Event types unconditionally updated in .revents
    ERR         = ASE_SYSVAL_POLLINIT[7],  ///< error condition
    HUP         = ASE_SYSVAL_POLLINIT[8],  ///< file descriptor closed
    NVAL        = ASE_SYSVAL_POLLINIT[9],  ///< invalid PollFD
  };
};

// == LoopPriority ==
enum class LoopPriority : uint16_t {
  SYSALLOC = 900, ///< Internal maintenance, don't use.
  RTAUDIO  = 800, ///< Threshold for priorization across different loops.
  USIGNAL  = 700, ///< Used for unix signal delivery.
  COAWAIT  = 600, ///< Used for coroutines, continuations of functions.
  NOTIFY   = 500, ///< For async notification, delivery of change notifications.
  AFRAME   = 400, ///< Animation frame timers, prioritized over normal event processing.
  NORMAL   = 300, ///< Normal importantance, GUI event processing, RPC.
  IDLE     = 200, ///< Mildly important, used for background tasks.
  LOW      = 100, ///< Unimportant, used when everything else done.
};

// === Prototypes ===
class LoopSource;
typedef std::shared_ptr<LoopSource> LoopSourceP;
class TimedSource;
typedef std::shared_ptr<TimedSource> TimedSourceP;
class PollFDSource;
typedef std::shared_ptr<PollFDSource> PollFDSourceP;
class DispatcherSource;
typedef std::shared_ptr<DispatcherSource> DispatcherSourceP;
class USignalSource;
typedef std::shared_ptr<USignalSource> USignalSourceP;
class SigchldSource;
typedef std::shared_ptr<SigchldSource> SigchldSourceP;
class Loop;
typedef std::shared_ptr<Loop> LoopP;
struct LoopState;
#if     defined __G_LIB_H__ || defined DOXYGEN
typedef ::GMainContext GlibGMainContext;
#else
struct GlibGMainContext; // dummy type
#endif

// === Loop ===
/// Loop object, polling for events and executing callbacks in accordance.
class Loop : public virtual std::enable_shared_from_this<Loop>
{
  friend class LoopImpl;
  ASE_CLASS_NON_COPYABLE (Loop);
protected:
  explicit      Loop                ();
  virtual      ~Loop                ();
  virtual void  destroy_loop        () = 0;
public:
  typedef std::function<void (void)>             VoidSlot;
  typedef std::function<bool (void)>             BoolSlot;
  typedef std::function<void (PollFD&)>          VPfdSlot;
  typedef std::function<bool (PollFD&)>          BPfdSlot;
  typedef std::function<bool (const LoopState&)> DispatcherSlot;
  typedef std::function<bool (int8)>             USignalSlot;
  typedef std::function<void (int,int)>          SigchldSlot;
  virtual void wakeup  () = 0;                ///< Wakeup loop from polling.
  // source handling
  virtual LoopID add_source    (LoopSourceP loop_source, LoopPriority priority
                                  = LoopPriority::NORMAL) = 0;     ///< Adds a new source to the loop with custom priority.
  virtual void cancel          (LoopID            id) = 0;    ///< Cancel a source and remove it from the  loop.
  virtual void cancel          (LoopID           *idp) = 0;   ///< Cancel a source by id if present and resets the id.
  virtual bool has_primary     (void) = 0;                  ///< Indicates whether loop contains primary sources.
  virtual bool flag_primary    (bool            on) = 0;
  template<class BoolVoidFunctor>
  LoopID exec_callback   (BoolVoidFunctor &&bvf, LoopPriority priority
                         = LoopPriority::NORMAL);     ///< Execute a callback at user defined priority returning true repeats callback.
  template<class BoolVoidFunctor>
  LoopID exec_idle       (BoolVoidFunctor &&bvf); ///< Execute a callback with priority "idle", returning true repeats callback.
  LoopID exec_dispatcher (const DispatcherSlot &sl, LoopPriority priority
                         = LoopPriority::NORMAL);     /// Execute a single dispatcher callback for prepare, check, dispatch.
  LoopID exec_usignal    (int8 signum, const USignalSlot &sl, LoopPriority priority
                         = LoopPriority::USIGNAL);     /// Execute a signal callback for prepare, check, dispatch.
  virtual LoopID exec_sigchld    (int64_t pid, const SigchldSlot &vfunc, LoopPriority priority
                                 = LoopPriority::NORMAL) = 0;     /// Execute a callback once on SIGCHLD for `pid`.
  virtual bool exec_once       (uint delay_ms, LoopID *once_id, const VoidSlot &vfunc, LoopPriority priority
                                 = LoopPriority::NORMAL) = 0;     ///< Execute a callback once, re-schedules the callback if `0 != *once_id`.
  /// Execute a callback after a specified timeout with adjustable initial timeout, returning true repeats callback.
  template<class BoolVoidFunctor>
  LoopID exec_timer      (BoolVoidFunctor &&bvf, uint delay_ms, int64 repeat_ms = -1, LoopPriority priority = LoopPriority::NORMAL);
  LoopID add             (auto &&func, std::chrono::milliseconds interval = std::chrono::milliseconds (0), LoopPriority priority = LoopPriority::NORMAL);
  LoopID add             (auto &&func, LoopPriority priority);
  /// Execute a callback after polling for mode on fd, returning true repeats callback.
  template<class BoolVoidPollFunctor>
  LoopID exec_io_handler (BoolVoidPollFunctor &&bvf, int fd, const String &mode, LoopPriority priority = LoopPriority::NORMAL);
  // Event processing
  virtual int  run           () = 0; ///< Run loop iterations until a call to quit() or finishable becomes true.
  virtual bool running       () = 0; ///< Indicates if quit() has been called already.
  virtual bool finishable    () = 0; ///< Indicates wether this loop has no primary sources left to process.
  virtual void quit          (int quit_code = 0) = 0; ///< Cause run() to return with @a quit_code.
  virtual bool pending       () = 0; ///< Check if iterate() needs to be called for dispatching.
  virtual bool iterate       (bool block) = 0; ///< Perform one loop iteration and return whether more iterations are needed.
  virtual void iterate_pending () = 0; ///< Call iterate() until no immediate dispatching is needed.
  virtual bool set_g_main_context (GlibGMainContext *glib_main_context) = 0; ///< Set context to integrate with a GLib @a GMainContext loop.
  static LoopP create        (); ///< Create a MainLoop shared pointer handle.
};

// === LoopState ===
struct LoopState {
  enum     Phase { NONE, COLLECT, PREPARE, CHECK, DISPATCH, DESTROY };
  Phase    phase = NONE;
  bool     seen_primary = false;   ///< Useful as hint for primary source presence, MainLoop::finishable() checks exhaustively
  uint64   current_time_usecs = 0; ///< Equals timestamp_realtime() as of prepare() and check().
  int64    timeout_usecs = 0;      ///< Maximum timeout for poll, queried during prepare().
};

// === LoopSource ===
class LoopSource /// Loop source for callback execution.
{
  friend       class Loop;
  friend       class LoopImpl;
  ASE_CLASS_NON_COPYABLE (LoopSource);
protected:
  Loop   *loop_;
  struct {
    PollFD    *pfd;
    uint       idx;
  }           *pfds_;
  LoopID       id_;
  int16        priority_;
  uint8        loop_state_;
  uint         may_recurse_ : 1;
  uint         dispatching_ : 1;
  uint         was_dispatching_ : 1;
  uint         primary_ : 1;
  uint         n_pfds      ();
  explicit     LoopSource ();
  LoopID       source_id   () { return loop_ ? id_ : LoopID::INVALID; }
  virtual     ~LoopSource ();
public:
  virtual bool prepare     (const LoopState &state,
                            int64 *timeout_usecs_p) = 0;    ///< Prepare the source for dispatching (true return) or polling (false).
  virtual bool check       (const LoopState &state) = 0;    ///< Check the source and its PollFD descriptors for dispatching (true return).
  virtual bool dispatch    (const LoopState &state) = 0;    ///< Dispatch source, returns if it should be kept alive.
  virtual void destroy     ();
  bool         recursion   () const;                        ///< Indicates wether the source is currently in recursion.
  bool         may_recurse () const;                        ///< Indicates if this source may recurse.
  void         may_recurse (bool           may_recurse);    ///< Dispatch this source if its running recursively.
  bool         primary     () const;                        ///< Indicate whether this source is primary.
  void         primary     (bool           is_primary);     ///< Set whether this source prevents its loop from exiting.
  void         add_poll    (PollFD * const pfd);            ///< Add a PollFD descriptors for poll(2) and check().
  void         remove_poll (PollFD * const pfd);            ///< Remove a previously added PollFD.
  void         loop_remove ();                              ///< Remove this source from its event loop if any.
  Loop*        loop        () const { return loop_; }       ///< Get the main loop for this source.
};

// === DispatcherSource ===
class DispatcherSource : public virtual LoopSource /// Loop source for handler execution.
{
  typedef Loop::DispatcherSlot DispatcherSlot;
  DispatcherSlot slot_;
  ASE_DEFINE_MAKE_SHARED (DispatcherSource);
protected:
  virtual     ~DispatcherSource ();
  virtual bool prepare          (const LoopState &state, int64 *timeout_usecs_p);
  virtual bool check            (const LoopState &state);
  virtual bool dispatch         (const LoopState &state);
  virtual void destroy          ();
  explicit     DispatcherSource (const DispatcherSlot &slot);
public:
  static DispatcherSourceP create (const DispatcherSlot &slot)
  { return make_shared (slot); }
};

// === USignalSource ===
class USignalSource : public virtual LoopSource /// Loop source for handler execution.
{
  typedef Loop::USignalSlot USignalSlot;
  USignalSlot slot_;
  int8        signum_ = 0, index_ = 0, shift_ = 0;
  ASE_DEFINE_MAKE_SHARED (USignalSource);
protected:
  virtual     ~USignalSource  ();
  virtual bool prepare        (const LoopState &state, int64 *timeout_usecs_p);
  virtual bool check          (const LoopState &state);
  virtual bool dispatch       (const LoopState &state);
  virtual void destroy        ();
  explicit     USignalSource  (int8 signum, const USignalSlot &slot);
public:
  static void           raise  (int8 signum);
  static USignalSourceP create (int8 signum, const USignalSlot &slot)
  { return make_shared (signum, slot); }
  static void install_sigaction (int8);
};

// === SigchldSource ===
class SigchldSource : public virtual LoopSource /// Loop source for handler execution.
{
  ASE_DEFINE_MAKE_SHARED (SigchldSource);
  typedef Loop::SigchldSlot SigchldSlot;
  SigchldSlot slot_;
  uint64_t    sigchld_counter_ = 0;
  int64_t     pid_ = 0;
protected:
  virtual     ~SigchldSource   ();
  virtual bool prepare         (const LoopState &state, int64 *timeout_usecs_p);
  virtual bool check           (const LoopState &state);
  virtual bool dispatch        (const LoopState &state);
  virtual void destroy         ();
  explicit     SigchldSource   (int64_t pid, const SigchldSlot &slot);
public:
  static SigchldSourceP create (int64_t pid, const SigchldSlot &slot)
  { return make_shared (pid, slot); }
};

// === TimedSource ===
class TimedSource : public virtual LoopSource /// Loop source for timer execution.
{
  typedef Loop::BoolSlot BoolSlot;
  typedef Loop::VoidSlot VoidSlot;
  uint64     expiration_usecs_;
  uint       interval_msecs_;
  bool       first_interval_;
  const bool oneshot_;
  union {
    BoolSlot bool_slot_;
    VoidSlot void_slot_;
  };
  ASE_DEFINE_MAKE_SHARED (TimedSource);
protected:
  virtual     ~TimedSource  ();
  virtual bool prepare      (const LoopState &state, int64 *timeout_usecs_p);
  virtual bool check        (const LoopState &state);
  virtual bool dispatch     (const LoopState &state);
  explicit     TimedSource  (const BoolSlot &slot, uint initial_interval_msecs, uint repeat_interval_msecs);
  explicit     TimedSource  (const VoidSlot &slot, uint initial_interval_msecs, uint repeat_interval_msecs);
public:
  static TimedSourceP create (const BoolSlot &slot, uint initial_interval_msecs = 0, uint repeat_interval_msecs = 0)
  { return make_shared (slot, initial_interval_msecs, repeat_interval_msecs); }
  static TimedSourceP create (const VoidSlot &slot, uint initial_interval_msecs = 0, uint repeat_interval_msecs = 0)
  { return make_shared (slot, initial_interval_msecs, repeat_interval_msecs); }
};

// === PollFDSource ===
class PollFDSource : public virtual LoopSource /// Loop source for IO callbacks.
{
  typedef Loop::BPfdSlot BPfdSlot;
  typedef Loop::VPfdSlot VPfdSlot;
protected:
  void          construct       (const String &mode);
  virtual      ~PollFDSource    ();
  virtual bool  prepare         (const LoopState &state, int64 *timeout_usecs_p);
  virtual bool  check           (const LoopState &state);
  virtual bool  dispatch        (const LoopState &state);
  virtual void  destroy         ();
  PollFD        pfd_;
  uint          never_close_ : 1;      // 'C'
private:
  const uint    oneshot_ : 1;
  union {
    BPfdSlot bool_poll_slot_;
    VPfdSlot void_poll_slot_;
  };
  explicit      PollFDSource    (const BPfdSlot &slot, int fd, const String &mode);
  explicit      PollFDSource    (const VPfdSlot &slot, int fd, const String &mode);
  ASE_DEFINE_MAKE_SHARED (PollFDSource);
public:
  static PollFDSourceP create (const BPfdSlot &slot, int fd, const String &mode)
  { return make_shared (slot, fd, mode); }
  static PollFDSourceP create (const VPfdSlot &slot, int fd, const String &mode)
  { return make_shared (slot, fd, mode); }
};

// === Loop methods ===
template<class BoolVoidFunctor> LoopID
Loop::exec_callback (BoolVoidFunctor &&bvf, LoopPriority priority)
{
  typedef decltype (bvf()) ReturnType;
  std::function<ReturnType()> slot (bvf);
  return add_source (TimedSource::create (slot), priority);
}

template<class BoolVoidFunctor> LoopID
Loop::exec_idle (BoolVoidFunctor &&bvf)
{
  typedef decltype (bvf()) ReturnType;
  std::function<ReturnType()> slot (bvf);
  TimedSourceP sourcep = TimedSource::create (slot);
  sourcep->primary (false);
  return add_source (sourcep, LoopPriority::IDLE);
}

inline LoopID
Loop::exec_dispatcher (const DispatcherSlot &slot, LoopPriority priority)
{
  return add_source (DispatcherSource::create (slot), priority);
}

inline LoopID
Loop::exec_usignal (int8 signum, const USignalSlot &slot, LoopPriority priority)
{
  return add_source (USignalSource::create (signum, slot), priority);
}

template<class BoolVoidFunctor> LoopID
Loop::exec_timer (BoolVoidFunctor &&bvf, uint delay_ms, int64 repeat_ms, LoopPriority priority)
{
  typedef decltype (bvf()) ReturnType;
  std::function<ReturnType()> slot (bvf);
  return add_source (TimedSource::create (slot, delay_ms, repeat_ms < 0 ? delay_ms : repeat_ms), priority);
}

LoopID
Loop::add (auto &&func, std::chrono::milliseconds interval, LoopPriority priority)
{
  const uint interval_ms = interval.count();
  return exec_timer (std::forward<decltype (func)> (func), interval_ms, interval_ms, priority);
}

LoopID
Loop::add (auto &&func, LoopPriority priority)
{
  return add (std::forward<decltype (func)> (func), std::chrono::milliseconds (0), priority);
}

template<class BoolVoidPollFunctor> LoopID
Loop::exec_io_handler (BoolVoidPollFunctor &&bvf, int fd, const String &mode, LoopPriority priority)
{
  using ReturnType = decltype (bvf (*std::declval<PollFD*>()));
  std::function<ReturnType (PollFD&)> slot (bvf);
  return add_source (PollFDSource::create (slot, fd, mode), priority);
}

} // Ase
