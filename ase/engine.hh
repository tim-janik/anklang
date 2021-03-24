// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_ENGINE_HH__
#define __ASE_ENGINE_HH__

#include <ase/platform.hh>

namespace Ase {

/// Flags to indicate channel arrangements of a bus.
/// See also: https://en.wikipedia.org/wiki/Surround_sound
enum class SpeakerArrangement : uint64_t {
  NONE                  = 0,
  FRONT_LEFT            =         0x1,  ///< Stereo Left (FL)
  FRONT_RIGHT           =         0x2,  ///< Stereo Right (FR)
  FRONT_CENTER          =         0x4,  ///< (FC)
  LOW_FREQUENCY         =         0x8,  ///< Low Frequency Effects (LFE)
  BACK_LEFT             =        0x10,  ///< (BL)
  BACK_RIGHT            =        0x20,  ///< (BR)
  // WAV reserved       =  0xyyy00000,
  AUX                   = uint64_t (1) << 63,   ///< Flag for side chain uses
  MONO                  = FRONT_LEFT,   ///< Single Channel (M)
  STEREO                = FRONT_LEFT | FRONT_RIGHT,
  STEREO_21             = STEREO | LOW_FREQUENCY,
  STEREO_30             = STEREO | FRONT_CENTER,
  STEREO_31             = STEREO_30 | LOW_FREQUENCY,
  SURROUND_50           = STEREO_30 | BACK_LEFT | BACK_RIGHT,
  SURROUND_51           = SURROUND_50 | LOW_FREQUENCY,
#if 0 // TODO: dynamic multichannel support
  FRONT_LEFT_OF_CENTER  =        0x40,  ///< (FLC)
  FRONT_RIGHT_OF_CENTER =        0x80,  ///< (FRC)
  BACK_CENTER           =       0x100,  ///< (BC)
  SIDE_LEFT             =       0x200,  ///< (SL)
  SIDE_RIGHT            =       0x400,  ///< (SR)
  TOP_CENTER            =       0x800,  ///< (TC)
  TOP_FRONT_LEFT        =      0x1000,  ///< (TFL)
  TOP_FRONT_CENTER      =      0x2000,  ///< (TFC)
  TOP_FRONT_RIGHT       =      0x4000,  ///< (TFR)
  TOP_BACK_LEFT         =      0x8000,  ///< (TBL)
  TOP_BACK_CENTER       =     0x10000,  ///< (TBC)
  TOP_BACK_RIGHT        =     0x20000,  ///< (TBR)
  SIDE_SURROUND_50      = STEREO_30 | SIDE_LEFT | SIDE_RIGHT,
  SIDE_SURROUND_51      = SIDE_SURROUND_50 | LOW_FREQUENCY,
#endif
};
constexpr SpeakerArrangement speaker_arrangement_channels_mask { ~size_t (SpeakerArrangement::AUX) };
uint8              speaker_arrangement_count_channels (SpeakerArrangement spa);
SpeakerArrangement speaker_arrangement_channels       (SpeakerArrangement spa);
bool               speaker_arrangement_is_aux         (SpeakerArrangement spa);
const char*        speaker_arrangement_bit_name       (SpeakerArrangement spa);
std::string        speaker_arrangement_desc           (SpeakerArrangement spa);

/// Main handle for AudioProcessor administration and audio rendering.
class AudioEngine : VirtualBase {
  const double nyquist_;          ///< Half the `sample_rate`.
  const double inyquist_;         ///< Inverse Nyquist frequency, i.e. `1.0 / nyquist_`
  const uint   sample_rate_;      ///< Sample rate (mixing frequency) in Hz used for rendering.
  const SpeakerArrangement speaker_arrangement_;
  uint64_t     frame_counter_;
  ThreadId     thread_id_ = {};
  friend class AudioEngineThread;
  class JobQueue {
    AudioEngine &engine_; const int flags_;
  public:
    explicit JobQueue   (AudioEngine &e, int f) : engine_ (e), flags_ (f) {}
    void     operator+= (const std::function<void()> &job) { return engine_.add_job (job, flags_); }
  };
  void         add_job               (const std::function<void()> &job, int flags);
protected:
  static void  render_block          (AudioProcessorP ap);
  explicit     AudioEngine           (uint sample_rate, SpeakerArrangement speakerarrangement);
public:
  virtual void add_output            (AudioProcessorP aproc) = 0;
  virtual void enqueue               (AudioProcessor &aproc) = 0;
  void         reschedule            ();
  // Owner-Thread API
  void         start_thread          (const VoidF &owner_wakeup);
  void         stop_thread           ();
  bool         ipc_pending           ();
  void         ipc_dispatch          ();
  // MT-Safe API
  void         ipc_wakeup_mt         ();
  uint         sample_rate           () const ASE_CONST     { return sample_rate_; }
  double       nyquist               () const ASE_CONST     { return nyquist_; }
  double       inyquist              () const ASE_CONST     { return inyquist_; }
  uint64_t     frame_counter         () const               { return frame_counter_; }
  ThreadId     thread_id             () const               { return thread_id_; }
  SpeakerArrangement speaker_arrangement () const;
  JobQueue     const_jobs;
  JobQueue     async_jobs;
};

AudioEngine&    make_audio_engine    (uint sample_rate, SpeakerArrangement speakerarrangement);
AudioProcessorP make_audio_processor (AudioEngine &engine, const String &uuiduri);

} // Ase

#endif /* __ASE_ENGINE_HH__ */
