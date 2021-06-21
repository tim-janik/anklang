// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_ENGINE_HH__
#define __ASE_ENGINE_HH__

#include <ase/transport.hh>
#include <ase/platform.hh>
#include <atomic>

namespace Ase {

/// Main handle for AudioProcessor administration and audio rendering.
class AudioEngine : VirtualBase {
  const double                 nyquist_;     // Half the `sample_rate`.
  const double                 inyquist_;    // Inverse Nyquist frequency, i.e. `1.0 / nyquist_`
  const uint                   sample_rate_; // Sample rate (mixing frequency) in Hz used for rendering.
  const SpeakerArrangement     speaker_arrangement_;
  std::atomic<size_t>          processor_count_ = 0;
  ThreadId                     thread_id_ = {};
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
  explicit     AudioEngine           (uint sample_rate, SpeakerArrangement speakerarrangement);
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
  virtual uint64_t   frame_counter       () const = 0;
  uint               sample_rate         () const ASE_CONST { return sample_rate_; }
  double             nyquist             () const ASE_CONST { return nyquist_; }
  double             inyquist            () const ASE_CONST { return inyquist_; }
  ThreadId           thread_id           () const           { return thread_id_; }
  SpeakerArrangement speaker_arrangement () const;
  JobQueue     const_jobs;
  JobQueue     async_jobs;
};

AudioEngine&    make_audio_engine    (uint sample_rate, SpeakerArrangement speakerarrangement);
AudioProcessorP make_audio_processor (AudioEngine &engine, const String &uuiduri);

} // Ase

#endif /* __ASE_ENGINE_HH__ */
