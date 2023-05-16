// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_LOOP_HH__
#define __ASE_LOOP_HH__

#include <ase/utils.hh>

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

// === Prototypes ===
class EventSource;
typedef std::shared_ptr<EventSource> EventSourceP;
class TimedSource;
typedef std::shared_ptr<TimedSource> TimedSourceP;
class PollFDSource;
typedef std::shared_ptr<PollFDSource> PollFDSourceP;
class DispatcherSource;
typedef std::shared_ptr<DispatcherSource> DispatcherSourceP;
class USignalSource;
typedef std::shared_ptr<USignalSource> USignalSourceP;
class EventLoop;
typedef std::shared_ptr<EventLoop> EventLoopP;
class MainLoop;
typedef std::shared_ptr<MainLoop> MainLoopP;
struct LoopState;
#if     defined __G_LIB_H__ || defined DOXYGEN
typedef ::GMainContext GlibGMainContext;
#else
struct GlibGMainContext; // dummy type
#endif

// === EventLoop ===
/// Loop object, polling for events and executing callbacks in accordance.
class EventLoop : public virtual std::enable_shared_from_this<EventLoop>
{
  struct QuickPfdArray;         // pseudo vector<PollFD>
  friend class MainLoop;
protected:
  typedef std::vector<EventSourceP> SourceList;
  MainLoop     *main_loop_;
  SourceList    sources_;
  std::vector<EventSourceP> poll_sources_;
  int16         dispatch_priority_;
  bool          primary_;
  explicit      EventLoop           (MainLoop&);
  virtual      ~EventLoop           ();
  EventSourceP& find_first_L        ();
  EventSourceP& find_source_L       (uint id);
  bool          has_primary_L       (void);
  void          remove_source_Lm    (EventSourceP source);
  void          kill_sources_Lm     (void);
  void          unpoll_sources_U    ();
  void          collect_sources_Lm  (LoopState&);
  bool          prepare_sources_Lm  (LoopState&, QuickPfdArray&);
  bool          check_sources_Lm    (LoopState&, const QuickPfdArray&);
  void          dispatch_source_Lm  (LoopState&);
public:
  typedef std::function<void (void)>             VoidSlot;
  typedef std::function<bool (void)>             BoolSlot;
  typedef std::function<void (PollFD&)>          VPfdSlot;
  typedef std::function<bool (PollFD&)>          BPfdSlot;
  typedef std::function<bool (const LoopState&)> DispatcherSlot;
  typedef std::function<bool (int8)>             USignalSlot;
  static const int16 PRIORITY_CEILING = 999; ///< Internal upper limit, don't use.
  static const int16 PRIORITY_NOW     = 900; ///< Most important, used for immediate async execution.
  static const int16 PRIORITY_ASCENT  = 800; ///< Threshold for priorization across different loops.
  static const int16 PRIORITY_HIGH    = 700; ///< Very important, used for timers or IO handlers.
  static const int16 PRIORITY_NEXT    = 600; ///< Important, used for async operations and callbacks.
  static const int16 PRIORITY_NORMAL  = 500; ///< Normal importantance, GUI event processing, RPC.
  static const int16 PRIORITY_UPDATE  = 400; ///< Mildly important, used for GUI updates or user information.
  static const int16 PRIORITY_IDLE    = 200; ///< Mildly important, used for background tasks.
  static const int16 PRIORITY_LOW     = 100; ///< Unimportant, used when everything else done.
  void wakeup   ();                          ///< Wakeup loop from polling.
  // source handling
  uint add             (EventSourceP loop_source, int priority
                        = PRIORITY_NORMAL);     ///< Adds a new source to the loop with custom priority.
  void remove          (uint            id);    ///< Removes a source from loop, the source must be present.
  bool try_remove      (uint            id);    ///< Tries to remove a source, returns if successfull.
  bool clear_source    (uint *id_pointer);      ///< Remove source if `id_pointer` and `*id_pointer` are valid.
  void destroy_loop    (void);
  bool has_primary     (void);                  ///< Indicates whether loop contains primary sources.
  bool flag_primary    (bool            on);
  MainLoop* main_loop  () const;                ///< Get the main loop for this loop.
  template<class BoolVoidFunctor>
  uint exec_now        (BoolVoidFunctor &&bvf); ///< Execute a callback as primary source with priority "now" (highest), returning true repeats callback.
  template<class BoolVoidFunctor>
  uint exec_callback   (BoolVoidFunctor &&bvf, int priority
                        = PRIORITY_NORMAL);     ///< Execute a callback at user defined priority returning true repeats callback.
  template<class BoolVoidFunctor>
  uint exec_idle       (BoolVoidFunctor &&bvf); ///< Execute a callback with priority "idle", returning true repeats callback.
  uint exec_dispatcher (const DispatcherSlot &sl, int priority
                        = PRIORITY_NORMAL);     /// Execute a single dispatcher callback for prepare, check, dispatch.
  uint exec_usignal    (int8 signum, const USignalSlot &sl, int priority
                        = PRIORITY_NOW -1);     /// Execute a signal callback for prepare, check, dispatch.
  /// Execute a callback after a specified timeout with adjustable initial timeout, returning true repeats callback.
  template<class BoolVoidFunctor>
  uint exec_timer      (BoolVoidFunctor &&bvf, uint delay_ms, int64 repeat_ms = -1, int priority = PRIORITY_NORMAL);
  /// Execute a callback after polling for mode on fd, returning true repeats callback.
  template<class BoolVoidPollFunctor>
  uint exec_io_handler (BoolVoidPollFunctor &&bvf, int fd, const String &mode, int priority = PRIORITY_NORMAL);
};

// === MainLoop ===
/// An EventLoop implementation that offers public API for running the loop.
class MainLoop : public EventLoop
{
  friend                class EventLoop;
  friend                class SubLoop;
  std::mutex            mutex_;
  uint                  rr_index_;
  std::vector<EventLoopP> loops_;
  EventFd               eventfd_;
  int8                  running_;
  int8                  has_quit_;
  int16                 quit_code_;
  GlibGMainContext     *gcontext_;
  bool                  finishable_L        ();
  void                  wakeup_poll         ();                 ///< Wakeup main loop from polling.
  void                  add_loop_L          (EventLoop &loop);  ///< Adds a sub loop to this main loop.
  void                  kill_loop_Lm        (EventLoop &loop);  ///< Destroy a sub loop and all its sources.
  void                  kill_loops_Lm       ();                 ///< Destroy this loop and all sub loops.
  bool                  iterate_loops_Lm    (LoopState&, bool b, bool d);
  explicit              MainLoop            ();
  virtual              ~MainLoop            ();
  ASE_DEFINE_MAKE_SHARED (MainLoop);
public:
  int        run             (); ///< Run loop iterations until a call to quit() or finishable becomes true.
  bool       running         (); ///< Indicates if quit() has been called already.
  bool       finishable      (); ///< Indicates wether this loop has no primary sources left to process.
  void       quit            (int quit_code = 0);    ///< Cause run() to return with @a quit_code.
  bool       pending         ();                     ///< Check if iterate() needs to be called for dispatching.
  bool       iterate         (bool block);           ///< Perform one loop iteration and return whether more iterations are needed.
  void       iterate_pending (); ///< Call iterate() until no immediate dispatching is needed.
  EventLoopP create_sub_loop (); ///< Creates a new event loop that is run as part of this main loop.
  inline std::mutex& mutex   () { return mutex_; } ///< Provide access to the mutex associated with this main loop.
  static MainLoopP   create  (); ///< Create a MainLoop shared pointer handle.
  bool    set_g_main_context (GlibGMainContext *glib_main_context); ///< Set context to integrate with a GLib @a GMainContext loop.
};

// === LoopState ===
struct LoopState {
  enum     Phase { NONE, COLLECT, PREPARE, CHECK, DISPATCH, DESTROY };
  Phase    phase = NONE;
  bool     seen_primary = false;   ///< Useful as hint for primary source presence, MainLoop::finishable() checks exhaustively
  uint64   current_time_usecs = 0; ///< Equals timestamp_realtime() as of prepare() and check().
  int64    timeout_usecs = 0;      ///< Maximum timeout for poll, queried during prepare().
};

// === EventSource ===
class EventSource /// EventLoop source for callback execution.
{
  friend       class EventLoop;
  ASE_CLASS_NON_COPYABLE (EventSource);
protected:
  EventLoop   *loop_;
  struct {
    PollFD    *pfd;
    uint       idx;
  }           *pfds_;
  uint         id_;
  int16        priority_;
  uint8        loop_state_;
  uint         may_recurse_ : 1;
  uint         dispatching_ : 1;
  uint         was_dispatching_ : 1;
  uint         primary_ : 1;
  uint         n_pfds      ();
  explicit     EventSource ();
  uint         source_id   () { return loop_ ? id_ : 0; }
  virtual     ~EventSource ();
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
  MainLoop*    main_loop   () const { return loop_ ? loop_->main_loop() : NULL; } ///< Get the main loop for this source.
};

// === DispatcherSource ===
class DispatcherSource : public virtual EventSource /// EventLoop source for handler execution.
{
  typedef EventLoop::DispatcherSlot DispatcherSlot;
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
class USignalSource : public virtual EventSource /// EventLoop source for handler execution.
{
  typedef EventLoop::USignalSlot USignalSlot;
  USignalSlot slot_;
  int8           signum_ = 0, index_ = 0, shift_ = 0;
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

// === TimedSource ===
class TimedSource : public virtual EventSource /// EventLoop source for timer execution.
{
  typedef EventLoop::BoolSlot BoolSlot;
  typedef EventLoop::VoidSlot VoidSlot;
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
class PollFDSource : public virtual EventSource /// EventLoop source for IO callbacks.
{
  typedef EventLoop::BPfdSlot BPfdSlot;
  typedef EventLoop::VPfdSlot VPfdSlot;
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

// === EventLoop methods ===
template<class BoolVoidFunctor> uint
EventLoop::exec_now (BoolVoidFunctor &&bvf)
{
  typedef decltype (bvf()) ReturnType;
  std::function<ReturnType()> slot (bvf);
  TimedSourceP tsource = TimedSource::create (slot);
  tsource->primary (true);
  return add (tsource, PRIORITY_NOW);
}

template<class BoolVoidFunctor> uint
EventLoop::exec_callback (BoolVoidFunctor &&bvf, int priority)
{
  typedef decltype (bvf()) ReturnType;
  std::function<ReturnType()> slot (bvf);
  return add (TimedSource::create (slot), priority);
}

template<class BoolVoidFunctor> uint
EventLoop::exec_idle (BoolVoidFunctor &&bvf)
{
  typedef decltype (bvf()) ReturnType;
  std::function<ReturnType()> slot (bvf);
  return add (TimedSource::create (slot), PRIORITY_IDLE);
}

inline uint
EventLoop::exec_dispatcher (const DispatcherSlot &slot, int priority)
{
  return add (DispatcherSource::create (slot), priority);
}

inline uint
EventLoop::exec_usignal (int8 signum, const USignalSlot &slot, int priority)
{
  return add (USignalSource::create (signum, slot), priority);
}

template<class BoolVoidFunctor> uint
EventLoop::exec_timer (BoolVoidFunctor &&bvf, uint delay_ms, int64 repeat_ms, int priority)
{
  typedef decltype (bvf()) ReturnType;
  std::function<ReturnType()> slot (bvf);
  return add (TimedSource::create (slot, delay_ms, repeat_ms < 0 ? delay_ms : repeat_ms), priority);
}

template<class BoolVoidPollFunctor> uint
EventLoop::exec_io_handler (BoolVoidPollFunctor &&bvf, int fd, const String &mode, int priority)
{
  using ReturnType = decltype (bvf (*std::declval<PollFD*>()));
  std::function<ReturnType (PollFD&)> slot (bvf);
  return add (PollFDSource::create (slot, fd, mode), priority);
}

} // Ase

#endif  /* __ASE_LOOP_HH__ */
