// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_ENGINE_HH__
#define __ASE_ENGINE_HH__

#include <ase/platform.hh>

namespace Ase {

/// Main handle for AudioProcessor administration and audio rendering.
class AudioEngine : VirtualBase {
  const double nyquist_;          ///< Half the `sample_rate`.
  const double inyquist_;         ///< Inverse Nyquist frequency, i.e. `1.0 / nyquist_`
  const uint   sample_rate_;      ///< Sample rate (mixing frequency) in Hz used for rendering.
  uint64_t     frame_counter_;
  ThreadId     thread_id_ = {};
  friend class AudioEngineThread;
protected:
  static void  render_block          (AudioProcessorP ap);
  explicit     AudioEngine           (uint sample_rate);
public:
  uint         sample_rate           () const ASE_CONST     { return sample_rate_; }
  double       nyquist               () const ASE_CONST     { return nyquist_; }
  double       inyquist              () const ASE_CONST     { return inyquist_; }
  uint64_t     frame_counter         () const               { return frame_counter_; }
  ThreadId     thread_id             () const               { return thread_id_; }
  void         start_thread          (const VoidF &owner_wakeup);
  void         stop_thread           ();
  virtual void assign_root           (AudioProcessorP aproc) = 0;
  virtual void enqueue               (AudioProcessor &aproc) = 0;
  void         reschedule            ();
  bool         ipc_pending           ();
  void         ipc_dispatch          ();
  void         ipc_wakeup_mt         ();
  void         operator+=            (const std::function< void()> &job);
};

AudioEngine&    make_audio_engine    (uint sample_rate);
AudioProcessorP make_audio_processor (AudioEngine &engine, const String &uuiduri);

} // Ase

#endif /* __ASE_ENGINE_HH__ */
