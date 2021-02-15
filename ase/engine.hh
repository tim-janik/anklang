// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_ENGINE_HH__
#define __ASE_ENGINE_HH__

#include <ase/platform.hh>

namespace Ase {

class AudioEngineThread;

class AudioEngine : VirtualBase {
  const double nyquist_;          ///< Half the `sample_rate`.
  const double inyquist_;         ///< Inverse Nyquist frequency, i.e. `1.0 / nyquist_`
  const uint   sample_rate_;      ///< Sample rate (mixing frequency) in Hz used for rendering.
  uint64_t     frame_counter_;
  ThreadId     thread_id_ = {};
  friend class AudioEngineThread;
protected:
  explicit AudioEngine   (uint sample_rate);
public:
  uint     sample_rate   () const ASE_CONST     { return sample_rate_; }
  double   nyquist       () const ASE_CONST     { return nyquist_; }
  double   inyquist      () const ASE_CONST     { return inyquist_; }
  uint64_t frame_counter () const               { return frame_counter_; }
  ThreadId thread_id     () const               { return thread_id_; }
  void     start_thread  (const VoidF &owner_wakeup);
  void     stop_thread   ();
};

AudioEngine& make_audio_engine (uint sample_rate);

} // Ase

#endif /* __ASE_ENGINE_HH__ */
