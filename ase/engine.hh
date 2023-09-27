// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_ENGINE_HH__
#define __ASE_ENGINE_HH__

#include <ase/transport.hh>
#include <ase/platform.hh>
#include <atomic>

namespace Ase {

struct AudioEngineJob {
  virtual ~AudioEngineJob();
  void *voidp = nullptr;
};

/** Main handle for AudioProcessor administration and audio rendering.
 * Use async_jobs to have the engine execute arbitrary code.
 * Use const_jobs for synchronous read-only data gathering, this may take quite long.
 * Use main_rt_jobs (see main.hh) for obstruction free enqueueing of main_loop callbacks.
 */
class AudioEngine : VirtualBase {
  std::atomic<size_t>          processor_count_ = 0;
  friend class AudioEngineThread;
  friend class AudioProcessor;
protected:
  AudioTransport *transport_ = nullptr;
  explicit AudioEngine           () {}
  void     enable_output         (AudioProcessor &aproc, bool onoff);
  void     schedule_queue_update ();
  void     schedule_add          (AudioProcessor &aproc, uint level);
public:
  // Owner-Thread API
  void            start_threads    ();
  void            stop_threads     ();
  void            wakeup_thread_mt ();
  bool            ipc_pending      ();
  void            ipc_dispatch     ();
  AudioProcessorP get_event_source ();
  void            set_project      (ProjectImplP project);
  ProjectImplP    get_project      ();
  // MT-Safe API
  uint64_t               frame_counter       () const;
  const AudioTransport&  transport           () const           { return *transport_; }
  uint                   sample_rate         () const ASE_CONST { return transport().samplerate; }
  uint                   nyquist             () const ASE_CONST { return transport().nyquist; }
  double                 inyquist            () const ASE_CONST { return transport().inyquist; }
  SpeakerArrangement     speaker_arrangement () const           { return transport().speaker_arrangement; }
  void                   set_autostop        (uint64_t nsamples);
  void                   queue_capture_start (CallbackS&, const String &filename, bool needsrunning);
  void                   queue_capture_stop  (CallbackS&);
  static AudioEngineJob* new_engine_job      (const std::function<void()> &jobfunc);
  static bool            thread_is_engine    () { return std::this_thread::get_id() == thread_id; }
  static const ThreadId &thread_id;
  // JobQueues
  struct JobQueue {
    void                 operator+= (const std::function<void()> &job);
    void                 operator+= (AudioEngineJob *job);
  };
  static JobQueue        async_jobs;    ///< Executed asynchronously, may modify AudioProcessor objects
  static JobQueue        const_jobs;    ///< Blocks during execution, must treat AudioProcessor objects read-only
};

AudioEngine&     make_audio_engine    (const VoidF &owner_wakeup, uint sample_rate, SpeakerArrangement speakerarrangement);

/// A BorrowedPtr wraps an object pointer until it is disposed, i.e. returned to the main-thread and deleted.
template<typename T>
class BorrowedPtr {
  AudioEngineJob   *job_ = nullptr;
  T*                ptr           () const { return job_ ? (T*) job_->voidp : nullptr; }
public:
  explicit          BorrowedPtr   (T *ptr = nullptr, const std::function<void(T*)> &deleter = std::default_delete<T>());
  /*copy*/          BorrowedPtr   (const BorrowedPtr &other) { *this = other; }
  BorrowedPtr&      operator=     (const BorrowedPtr &other) noexcept;
  explicit          operator bool () const { return ptr() != nullptr; }
  T&                operator*     () const { return *ptr(); }
  T*                operator->    () const { return ptr(); }
  T*                get           () const { return ptr(); }
  void              dispose       (AudioEngine &engine);
};

// == implementations ==
template<typename T> BorrowedPtr<T>&
BorrowedPtr<T>::operator= (const BorrowedPtr &other) noexcept
{
  job_ = other.job_;
  return *this;
}

template<typename T> void
BorrowedPtr<T>::dispose (AudioEngine &engine)
{
  ASE_ASSERT_RETURN (ptr() != nullptr);
  if (job_) {
    job_->voidp = nullptr;
    engine.async_jobs += job_;
    job_ = nullptr;
  }
}
template<typename T>
BorrowedPtr<T>::BorrowedPtr (T *ptr, const std::function<void(T*)> &deleter)
{
  if (!ptr)
    return;
  std::shared_ptr<void> deleter_p (nullptr, [ptr, deleter] (void*) {
    // printerr ("%s::deleter_p: is_main_thread=%d\n", typeid_name<BorrowedPtr<T>>(), this_thread_is_ase());
    deleter (ptr);
  });                                                   // destroyed in [main-thread]
  auto jobfunc = [deleter_p] () {
    // printerr ("%s::jobfunc: thread_is_engine_thread=%d\n", typeid_name<BorrowedPtr<T>>(), AudioEngine::thread_is_engine());
  };                                                    // runs in [audio-thread]
  job_ = AudioEngine::new_engine_job (jobfunc);
  ASE_ASSERT_RETURN (job_->voidp == nullptr);           // user data
  job_->voidp = ptr;
  ASE_ASSERT_RETURN (job_->voidp != nullptr);
}

} // Ase

#endif /* __ASE_ENGINE_HH__ */
