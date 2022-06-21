// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_ENGINE_HH__
#define __ASE_ENGINE_HH__

#include <ase/transport.hh>
#include <ase/platform.hh>
#include <atomic>

namespace Ase {

/// Main handle for AudioProcessor administration and audio rendering.
class AudioEngine : VirtualBase {
  std::atomic<size_t>          processor_count_ = 0;
  friend class AudioEngineThread;
  friend class AudioProcessor;
  class JobQueue {
    AudioEngine &engine_; const int flags_;
  public:
    explicit JobQueue   (AudioEngine &e, int f) : engine_ (e), flags_ (f) {}
    void     operator+= (const std::function<void()> &job) { return engine_.add_job_mt (job, flags_); }
  };
  void         add_job_mt            (const std::function<void()> &job, int flags);
protected:
  AudioTransport                    *transport_ = nullptr;
  explicit     AudioEngine           ();
  virtual void enable_output         (AudioProcessor &aproc, bool onoff) = 0;
  virtual void schedule_queue_update () = 0;
  virtual void schedule_add          (AudioProcessor &aproc, uint level) = 0;
public:
  // Owner-Thread API
  virtual void start_thread          (const VoidF &owner_wakeup) = 0;
  virtual void stop_thread           () = 0;
  virtual void wakeup_thread_mt      () = 0;
  virtual bool ipc_pending           () = 0;
  virtual void ipc_dispatch          () = 0;
  virtual AudioProcessorP get_event_source () = 0;
  // MT-Safe API
  virtual uint64_t      frame_counter       () const = 0;
  const AudioTransport& transport           () const           { return *transport_; }
  uint                  sample_rate         () const ASE_CONST { return transport().samplerate; }
  uint                  nyquist             () const ASE_CONST { return transport().nyquist; }
  double                inyquist            () const ASE_CONST { return transport().inyquist; }
  SpeakerArrangement    speaker_arrangement () const           { return transport().speaker_arrangement; }
  JobQueue              const_jobs;
  JobQueue              async_jobs;
  static const ThreadId &thread_id;
  static bool           thread_is_engine    () { return std::this_thread::get_id() == thread_id; }
};

AudioEngine&    make_audio_engine    (uint sample_rate, SpeakerArrangement speakerarrangement);

} // Ase

#endif /* __ASE_ENGINE_HH__ */
