// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_ENGINE_HH__
#define __ASE_ENGINE_HH__

#include <ase/transport.hh>
#include <ase/platform.hh>
#include <atomic>

namespace Ase {

class AudioEngineThread;

/** Main handle for AudioProcessor administration and audio rendering.
 * Use make_audio_engine() to create a new engine and start_threads() to run
 * its synthesis threads. AudioEngine objects cannot be deleted, because other
 * ref-counted objects may hold `AudioEngine&` members until after main().
 * Use async_jobs to have the engine execute arbitrary code.
 * Use const_jobs for synchronous read-only data gathering, this may take quite long.
 * Use main_rt_jobs (see main.hh) for obstruction free enqueueing of main_loop callbacks.
 */
class AudioEngine : VirtualBase {
protected:
  friend class AudioProcessor;
  std::atomic<size_t> processor_count_ alignas (64) = 0;
  std::atomic<uint64_t> render_stamp_ = 0;
  AudioTransport     &transport_;
  explicit AudioEngine           (AudioEngineThread&, AudioTransport&);
  virtual ~AudioEngine           ();
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
  uint64_t               frame_counter       () const           { return render_stamp_; }
  uint64_t               block_size          () const;
  const AudioTransport&  transport           () const           { return transport_; }
  uint                   sample_rate         () const ASE_CONST { return transport().samplerate; }
  uint                   nyquist             () const ASE_CONST { return transport().nyquist; }
  double                 inyquist            () const ASE_CONST { return transport().inyquist; }
  SpeakerArrangement     speaker_arrangement () const           { return transport().speaker_arrangement; }
  void                   set_autostop        (uint64_t nsamples);
  void                   queue_capture_start (CallbackS&, const String &filename, bool needsrunning);
  void                   queue_capture_stop  (CallbackS&);
  bool                   update_drivers      (const String &pcm, uint latency_ms, const StringS &midis);
  String                 engine_stats        (uint64_t stats) const;
  static bool            thread_is_engine    () { return std::this_thread::get_id() == thread_id; }
  static const ThreadId &thread_id;
  // JobQueues
  class JobQueue {
    friend class AudioEngine;
    const uint8_t        queue_tag_;
    explicit             JobQueue (AudioEngine&);
  public:
    void                 operator+= (const std::function<void()> &job);
  };
  JobQueue               async_jobs;    ///< Executed asynchronously, may modify AudioProcessor objects
  JobQueue               const_jobs;    ///< Blocks during execution, must treat AudioProcessor objects read-only
protected:
  JobQueue               synchronized_jobs;
};

AudioEngine& make_audio_engine (const VoidF &owner_wakeup, uint sample_rate, SpeakerArrangement speakerarrangement);

/// Helper to modify const struct contents, e.g. asyn job lambda members.
template<class T>
struct Mutable {
  mutable T value;
  Mutable (const T &v) : value (v) {}
  operator T& () { return value; }
};

} // Ase

#endif /* __ASE_ENGINE_HH__ */
