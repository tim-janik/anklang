// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_TRANSPORT_HH__
#define __ASE_TRANSPORT_HH__

#include <ase/defs.hh>

namespace Ase {

/// Flags to indicate channel arrangements of a bus.
/// See also: https://en.wikipedia.org/wiki/Surround_sound
enum class SpeakerArrangement : uint64_t {
  NONE                  =           0,
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

/// Maximum number of sample frames to calculate in Processor::render().
constexpr const int64 TRANSPORT_PPQN = 4838400;                 // Ticks per quarter note
constexpr const int64 SEMIQUAVER_TICKS = TRANSPORT_PPQN / 4;    // 1209600 = PPQN * 4 / 16;
constexpr const int64 MIN_BPM = 10;
constexpr const int64 MAX_BPM = 1776;
constexpr const int64 MIN_SAMPLERATE = 8000;
constexpr const int64 MAX_SAMPLERATE = 192000;

/// Musical time signature and tick conversions.
struct TickSignature {
protected:
  static constexpr const int64 offset_ = 0; // currently unused
  uint8  beats_per_bar_ = 4;    ///< Upper numeral (numerator), how many beats constitute a bar.
  uint8  beat_unit_ = 4;        ///< Lower numeral (denominator in [1 2 4 8 16]), note value that represents one beat.
  int32  beat_ticks_ = 0;
  int32  bar_ticks_ = 0;
  int32  samplerate_ = 0;       ///< Sample rate (mixing frequency) in Hz.
  double bpm_ = 0;              ///< Current tempo in beats per minute.
  int64  ticks_per_minute_ = 0;
  double ticks_per_second_ = 0;
  double inv_ticks_per_second_ = 0;
  double ticks_per_sample_ = 0;
  double sample_per_ticks_ = 0;
  double inv_samplerate_;       ///< Precalculated `1.0 / samplerate`.
public:
  struct Beat {
    int32  bar = 0;             ///< Bar of tick position.
    int8   beat = 0;            ///< Beat within bar of tick position.
    double semiquaver = 0;      ///< The sixteenth with fraction within beat.
  };
  struct Time {
    int32  minutes = 0;         ///< Tick position in minutes.
    double seconds = 0;         ///< Seconds with fraction after the minute.
  };
  double       samplerate       () const      { return samplerate_; }
  double       inv_samplerate   () const      { return inv_samplerate_; }
  double       ticks_per_sample () const      { return ticks_per_sample_; }
  int64        sample_to_tick   (int64 sample) const;
  int64        sample_from_tick (int64 tick) const;
  double       bpm              () const      { return bpm_; }
  int32        bar_ticks        () const      { return bar_ticks_; }
  int32        beat_ticks       () const      { return beat_ticks_; }
  int64        start_offset     () const      { return offset_; }
  void         set_samplerate   (uint samplerate);
  void         set_bpm          (double bpm);
  Time         time_from_tick   (int64 tick) const;
  int64        time_to_tick     (const Time &time) const;
  bool         set_signature    (uint8 beats_per_bar, uint8 beat_unit);
  const uint8& beats_per_bar    () const   { return beats_per_bar_; }
  const uint8& beat_unit        () const   { return beat_unit_; }
  int32        bar_from_tick    (int64 tick) const;
  int64        bar_to_tick      (int32 bar) const;
  Beat         beat_from_tick   (int64 tick) const;
  int64        beat_to_tick     (const Beat &beat) const;
  /*dflt*/     TickSignature    ();
  /*ctor*/     TickSignature    (double bpm, uint8 beats_per_bar, uint8 beat_unit);
  /*copy*/     TickSignature    (const TickSignature &other);
  TickSignature& operator=      (const TickSignature &src);
};

/// Transport information for AudioSignal processing.
struct AudioTransport {
  static constexpr int64 ppqn = TRANSPORT_PPQN;
  const uint     samplerate;    ///< Sample rate (mixing frequency) in Hz used for rendering.
  const uint     nyquist;       ///< Half the `samplerate`.
  const double   isamplerate;   ///< Precalculated `1.0 / samplerate`.
  const double   inyquist;      ///< Precalculated `1.0 / nyquist`.
  const SpeakerArrangement speaker_arrangement; ///< Audio output configuration.
  // uint32 gap
  TickSignature  tick_sig;
  int64          current_frame = 0;             ///< Number of sample frames processed since playback start.
  int64          current_tick = 0;
  __attribute__ ((aligned (64)))        // align memory for project telemetry fields
  double         current_tick_d = 0;            ///< Current position measured via *TRANSPORT_PPQN*
  int32          current_bar =  0;              ///< Bar of *current_tick* position
  int8           current_beat = 0;              ///< Beat within bar of *current_tick* position
  double         current_semiquaver = 0;        ///< The sixteenth with fraction within beat
  float          current_bpm = 0;               ///< Running tempo in beats per minute
  int32          current_minutes = 0;           ///< Minute of *current_tick* position
  double         current_seconds = 0;           ///< Seconds of *current_tick* position
  int64          current_bar_tick = 0;
  int64          next_bar_tick = 0;
  bool     running        () const      { return current_bpm != 0; }
  void     running        (bool r);
  void     tempo          (double newbpm, uint8 newnumerator, uint8 newdenominator);
  void     tempo          (const TickSignature &ticksignature);
  void     set_tick       (int64 newtick);
  void     set_beat       (TickSignature::Beat b);
  void     advance        (uint nsamples);
  void     update_current ();
  explicit AudioTransport (SpeakerArrangement speakerarrangement, uint samplerate);
  int64    sample_to_tick  (int64 sample) const { return tick_sig.sample_to_tick (sample); }
  int64    sample_from_tick (int64 tick) const  { return tick_sig.sample_from_tick (tick); }
};

// == Implementations ==
inline int64
TickSignature::sample_to_tick (int64 sample) const
{
  return ticks_per_sample_ * sample;
}

inline int64
TickSignature::sample_from_tick (int64 tick) const
{
  return sample_per_ticks_ * tick;
}

} // Ase

#endif // __ASE_TRANSPORT_HH__
