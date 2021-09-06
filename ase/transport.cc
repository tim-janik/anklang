// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "transport.hh"
#include "utils.hh"
#include "internal.hh"

namespace Ase {

// == TickSignature ==
constexpr double INVERSE_SEMIQUAVER = 1.0 / SEMIQUAVER_TICKS;

TickSignature::TickSignature() :
  TickSignature (60, 4, 4, 0)
{}

TickSignature::TickSignature (double bpm, uint8 beats_per_bar, uint8 beat_unit, int64 tick_offset)
{
  set_signature (beats_per_bar, beat_unit, tick_offset);
  set_bpm (bpm);
}

/// Assign sample rate.
void
TickSignature::set_samplerate (uint samplerate)
{
  assert_return (samplerate > 0);
  samplerate_ = samplerate;
  inv_samplerate_ = 1.0 / samplerate_;
  const double ticks_per_minute_d = TRANSPORT_PPQN * bpm_;
  ticks_per_sample_ = samplerate_ > 0 ? ticks_per_minute_d / (60.0 * samplerate_) : 0;
  sample_per_ticks_ = ticks_per_minute_d > 0 ? (60.0 * samplerate_) / ticks_per_minute_d : 0;
}

/// Assign tempo in beats per minute.
void
TickSignature::set_bpm (double bpm, int64 start_offset)
{
  assert_return (bpm >= 0);
  offset_ = start_offset;
  bpm_ = bpm;
  const double ticks_per_minute_d = TRANSPORT_PPQN * bpm_;
  ticks_per_minute_ = ticks_per_minute_d;
  ticks_per_second_ = ticks_per_minute_d * (1.0 / 60.0);
  inv_ticks_per_second_ = ISLIKELY (bpm_ > 0) ? 1.0 / ticks_per_second_ : 0.0;
  ticks_per_sample_ = samplerate_ > 0 ? ticks_per_minute_d / (60.0 * samplerate_) : 0;
  sample_per_ticks_ = ticks_per_minute_d > 0 ? (60.0 * samplerate_) / ticks_per_minute_d : 0;
}

/// Calculate time from tick, requires set_bpm().
TickSignature::Time
TickSignature::time_from_tick (int64 tick) const
{
  Time t;
  int64 minutereminder;
  t.minutes = divmod (tick - offset_, ticks_per_minute_, &minutereminder);
  t.seconds = minutereminder * inv_ticks_per_second_;
  return t;
}

/// Calculate tick from time, requires set_bpm().
int64
TickSignature::time_to_tick (const Time &time) const
{
  int64 tick = offset_;
  tick += time.minutes * ticks_per_minute_;
  tick += time.seconds * ticks_per_second_;
  return tick;
}

/// Assign time signature and offset for the signature to take effect.
bool
TickSignature::set_signature (uint8 beats_per_bar, uint8 beat_unit, int64 start_offset)
{
  const auto old_beats_per_bar_ = beats_per_bar_;
  const auto old_beat_unit_ = beat_unit_;
  const auto old_offset_ = offset_;
  offset_ = start_offset;
  beats_per_bar_ = CLAMP (beats_per_bar, 1, 64);
  if (beat_unit == 1 || beat_unit == 2 || beat_unit == 4 || beat_unit == 8 || beat_unit == 16)
    beat_unit_ = beat_unit;
  const int semiquavers_per_beat = 16 / beat_unit_;
  beat_ticks_ = SEMIQUAVER_TICKS * semiquavers_per_beat; // == 4 * PPQN / beat_unit_
  bar_ticks_ = beat_ticks_ * beats_per_bar_;
  return old_beats_per_bar_ != beats_per_bar_ || old_beat_unit_ != beat_unit_ || old_offset_ != offset_;
}

/// Calculate beat from tick, requires set_signature().
TickSignature::Beat
TickSignature::beat_from_tick (int64 tick) const
{
  Beat b;
  int64 bar_reminder;
  b.bar = divmod (tick - offset_, int64 (bar_ticks_), &bar_reminder);
  int32 beat_reminder;
  b.beat = divmod (int32 (bar_reminder), beat_ticks_, &beat_reminder);
  b.semiquaver = beat_reminder * INVERSE_SEMIQUAVER;
  return b;
}

/// Calculate tick from beat, requires set_signature().
int64
TickSignature::beat_to_tick (const Beat &beat) const
{
  int64 tick = offset_;
  tick += beat.bar * int64 (bar_ticks_);
  tick += beat.beat * int64 (beat_ticks_);
  tick += beat.semiquaver * SEMIQUAVER_TICKS;
  return tick;
}

/// Calculate bar from tick, requires set_signature().
int32
TickSignature::bar_from_tick (int64 tick) const
{
  return (tick - offset_) / bar_ticks_;
}

/// Calculate tick from bar, requires set_signature().
int64
TickSignature::bar_to_tick (int32 bar) const
{
  int64 tick = offset_;
  tick += bar * int64 (bar_ticks_);
  return tick;
}

// == SpeakerArrangement ==
// Count the number of channels described by the SpeakerArrangement.
uint8
speaker_arrangement_count_channels (SpeakerArrangement spa)
{
  const uint64_t bits = uint64_t (speaker_arrangement_channels (spa));
  if_constexpr (sizeof (bits) == sizeof (long))
    return __builtin_popcountl (bits);
  return __builtin_popcountll (bits);
}

// Check if the SpeakerArrangement describes auxillary channels.
bool
speaker_arrangement_is_aux (SpeakerArrangement spa)
{
  return uint64_t (spa) & uint64_t (SpeakerArrangement::AUX);
}

// Retrieve the bitmask describing the SpeakerArrangement channels.
SpeakerArrangement
speaker_arrangement_channels (SpeakerArrangement spa)
{
  const uint64_t bits = uint64_t (spa) & uint64_t (speaker_arrangement_channels_mask);
  return SpeakerArrangement (bits);
}

const char*
speaker_arrangement_bit_name (SpeakerArrangement spa)
{
  switch (spa)
    { // https://wikipedia.org/wiki/Surround_sound
    case SpeakerArrangement::NONE:              	return "-";
      // case SpeakerArrangement::MONO:                 return "Mono";
    case SpeakerArrangement::FRONT_LEFT:        	return "FL";
    case SpeakerArrangement::FRONT_RIGHT:       	return "FR";
    case SpeakerArrangement::FRONT_CENTER:      	return "FC";
    case SpeakerArrangement::LOW_FREQUENCY:     	return "LFE";
    case SpeakerArrangement::BACK_LEFT:         	return "BL";
    case SpeakerArrangement::BACK_RIGHT:                return "BR";
    case SpeakerArrangement::AUX:                       return "AUX";
    case SpeakerArrangement::STEREO:                    return "Stereo";
    case SpeakerArrangement::STEREO_21:                 return "Stereo-2.1";
    case SpeakerArrangement::STEREO_30:	                return "Stereo-3.0";
    case SpeakerArrangement::STEREO_31:	                return "Stereo-3.1";
    case SpeakerArrangement::SURROUND_50:	        return "Surround-5.0";
    case SpeakerArrangement::SURROUND_51:	        return "Surround-5.1";
#if 0 // TODO: dynamic multichannel support
    case SpeakerArrangement::FRONT_LEFT_OF_CENTER:      return "FLC";
    case SpeakerArrangement::FRONT_RIGHT_OF_CENTER:     return "FRC";
    case SpeakerArrangement::BACK_CENTER:               return "BC";
    case SpeakerArrangement::SIDE_LEFT:	                return "SL";
    case SpeakerArrangement::SIDE_RIGHT:	        return "SR";
    case SpeakerArrangement::TOP_CENTER:	        return "TC";
    case SpeakerArrangement::TOP_FRONT_LEFT:	        return "TFL";
    case SpeakerArrangement::TOP_FRONT_CENTER:	        return "TFC";
    case SpeakerArrangement::TOP_FRONT_RIGHT:	        return "TFR";
    case SpeakerArrangement::TOP_BACK_LEFT:	        return "TBL";
    case SpeakerArrangement::TOP_BACK_CENTER:	        return "TBC";
    case SpeakerArrangement::TOP_BACK_RIGHT:	        return "TBR";
    case SpeakerArrangement::SIDE_SURROUND_50:	        return "Side-Surround-5.0";
    case SpeakerArrangement::SIDE_SURROUND_51:	        return "Side-Surround-5.1";
#endif
    }
  return nullptr;
}

std::string
speaker_arrangement_desc (SpeakerArrangement spa)
{
  const bool isaux = speaker_arrangement_is_aux (spa);
  const SpeakerArrangement chan = speaker_arrangement_channels (spa);
  const char *chname = SpeakerArrangement::MONO == chan ? "Mono" : speaker_arrangement_bit_name (chan);
  std::string s (chname ? chname : "<INVALID>");
  if (isaux)
    s = std::string (speaker_arrangement_bit_name (SpeakerArrangement::AUX)) + "(" + s + ")";
  return s;
}

// == advance ==
AudioTransport::AudioTransport (SpeakerArrangement speakerarrangement, uint sample_rate) :
  samplerate (sample_rate), nyquist (sample_rate / 2),
  isamplerate (1.0 / sample_rate), inyquist (2.0 / sample_rate),
  speaker_arrangement (speakerarrangement)
{
  tick_sig.set_samplerate (sample_rate);
}

void
AudioTransport::running (bool r)
{
  current_bpm = r ? tick_sig.bpm() : 0;
}

void
AudioTransport::tickto (int64 newtick)
{
  current_tick = newtick;
  current_tick_d = current_tick;
  update_current();
}

void
AudioTransport::tempo (double newbpm, uint8 numerator, uint8 denominator)
{
  tick_sig.set_bpm (CLAMP (newbpm, 10, 999.9999), tick_sig.start_offset());
  tick_sig.set_signature (numerator, denominator, tick_sig.start_offset());
  current_bpm = current_bpm ? newbpm : 0;
  update_current();
}

void
AudioTransport::advance (uint nsamples)
{
  current_frame += nsamples;
  if (ISLIKELY (current_bpm > 0.0))
    {
      current_tick_d += nsamples * tick_sig.ticks_per_sample();
      current_tick = current_tick_d;
      update_current();
    }
}

void
AudioTransport::update_current ()
{
  TickSignature::Beat beat = tick_sig.beat_from_tick (current_tick);
  current_bar = beat.bar;
  current_beat = beat.beat;
  current_semiquaver = beat.semiquaver;
  const auto old_next = next_bar_tick;
  current_bar_tick = tick_sig.bar_to_tick (current_bar);
  next_bar_tick = current_bar_tick + tick_sig.bar_ticks();

  TickSignature::Time time = tick_sig.time_from_tick (current_tick);
  current_minutes = time.minutes;
  current_seconds = time.seconds;

  if (false && old_next != next_bar_tick)
    printerr ("%3d.%2d.%5.2f %02d:%06.3f frame=%d tick=%d next=%d bpm=%d sig=%d/%d ppqn=%d pps=%f rate=%d\n",
              current_bar, current_beat, current_semiquaver,
              current_minutes, current_seconds,
              current_frame, current_tick, next_bar_tick,
              current_bpm, tick_sig.beats_per_bar(), tick_sig.beat_unit(),
              TRANSPORT_PPQN, tick_sig.ticks_per_sample(), samplerate);
}

} // Ase

// == Testing ==
#include "testing.hh"

namespace { // Anon
using namespace Ase;

TEST_INTEGRITY (transport_tests);

static void
transport_tests()
{
  static_assert (SEMIQUAVER_TICKS == int32 (SEMIQUAVER_TICKS));
  static_assert (TRANSPORT_PPQN == int32 (TRANSPORT_PPQN));
  static_assert (TRANSPORT_PPQN % 16 == 0); // needed for beat_unit and semiquaver calculations
  static_assert (TRANSPORT_PPQN % SEMIQUAVER_TICKS == 0);
  const int64 max_semiquavers_per_beat = 16;
  int64 max_beat_ticks = SEMIQUAVER_TICKS * max_semiquavers_per_beat;
  int64 max_bar_ticks = max_beat_ticks * 64 / 1;
  TASSERT (max_bar_ticks < 2147483648); // 2^31
  static_assert (TRANSPORT_PPQN < 8388608); // 8388608 = 2^31 / (4*64)
  int64 testtick = 170000000000077;
  TickSignature ts { 60, 4, 4, 0 };
  const TickSignature::Time tt = ts.time_from_tick (testtick);
  int32 hminutes, hours = divmod (tt.minutes, 60, &hminutes);
  TickSignature::Beat tb = ts.beat_from_tick (testtick);
  TCMP (ts.bar_from_tick (testtick), ==, tb.bar);
  if (false)
    printerr ("%03d.%02d.%06.3f %02d:%02d:%06.3f tick=%d\n",
              tb.bar, tb.beat, tb.semiquaver,
              hours, hminutes, tt.seconds, testtick);
  TCMP (ts.beat_to_tick (tb), ==, testtick);
  TCMP (ts.time_to_tick (tt), ==, testtick);
  tb.beat = 0;
  tb.semiquaver = 0;
  TCMP (ts.bar_to_tick (tb.bar), ==, ts.beat_to_tick (tb));
}

} // Anon
