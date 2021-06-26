// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_TRANSPORT_HH__
#define __ASE_TRANSPORT_HH__

#include <ase/defs.hh>

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

/// Maximum number of sample frames to calculate in Processor::render().
constexpr const uint AUDIO_BLOCK_MAX_RENDER_SIZE = 128;

/// Transport information for AudioSignal processing.
struct AudioTransport {
  static_assert (AUDIO_BLOCK_MAX_RENDER_SIZE == 128);
  const uint     samplerate;    ///< Sample rate (mixing frequency) in Hz used for rendering.
  const uint     nyquist;       ///< Half the `samplerate`.
  const double   isamplerate;   ///< Precalculated `1.0 / samplerate`.
  const double   inyquist;      ///< Precalculated `1.0 / nyquist`.
  const int64    ppqn;
  const SpeakerArrangement speaker_arrangement; ///< Audio output configuration.
  const uint32_t dummy;
  float          current_bpm;   ///< Current tempo in beats per minute.
  uint64         frame_stamp;   ///< Number of sample frames processed since playback start.
};

} // Ase

#endif // __ASE_TRANSPORT_HH__
