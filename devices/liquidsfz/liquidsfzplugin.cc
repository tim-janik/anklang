// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "liquidsfzplugin.hh"

namespace Ase
{

/*
 * do not use a std::mutex here because it may not be hard RT safe to
 * try_lock() / unlock() it (depending on how the mutex is implemented)
 */
bool
LiquidSFZTracktionPlugin::RTMutex::try_lock()
{
  return !locked_flag.test_and_set();
}

/* TODO: either get gid of RTMutex or use RAII */
void
LiquidSFZTracktionPlugin::RTMutex::wait_for_lock()
{
  while (!try_lock())
    {
      // this doesn't happen very often and we are in a non-RT thread, so we
      // can block it for some time
      //  => wait for less than one frame drawing time until trying again
      constexpr int fps = 240;
      std::this_thread::sleep_for (std::chrono::microseconds (1000 * 1000 / fps));
    }
}

void
LiquidSFZTracktionPlugin::RTMutex::unlock()
{
  locked_flag.clear();
}

LiquidSFZTracktionPlugin::LiquidSFZTracktionPlugin (tracktion::PluginCreationInfo info) : Plugin (info)
{
}

/* TODO: should load in worker thread */
bool
LiquidSFZTracktionPlugin::load (const String& filename)
{
  rt_mutex_.wait_for_lock();

  bool result;
  if (synth_.is_bank (filename))
    result = synth_.load_bank (filename) && synth_.select_program (0);
  else
    result = synth_.load (filename);

  /* TODO:
   * - synth_.live_mode() needs to be set for offline rendering
   * - synth_.set_gain() could be mapped to a property
   * - synth_.list_programs(), select_program() needs UI and save/restore support
   * - synth_.list_ccs() needs synthesis and UI support
   * - synth_.list_keys() needs UI support
   * - synth_.set_log_function() needs engine support
   */

  rt_mutex_.unlock();

  return result;
}

void
LiquidSFZTracktionPlugin::initialise (const tracktion::PluginInitialisationInfo& info)
{
  rt_mutex_.wait_for_lock();
  synth_.set_sample_rate (info.sampleRate);
  synth_.all_sound_off();
  rt_mutex_.unlock();
}

void
LiquidSFZTracktionPlugin::deinitialise()
{
  rt_mutex_.wait_for_lock();
  synth_.all_sound_off();
  rt_mutex_.unlock();
}

void
LiquidSFZTracktionPlugin::applyToBuffer (const tracktion::PluginRenderContext& fc)
{
  if (fc.destBuffer != nullptr)
    {
      /* try_lock is non-blocking and RT safe
       *
       *  - if the mutex cannot be locked this means we're currently loading new SFZ data,
       *    so in this case we fill the output buffers with zeros
       */
      if (!rt_mutex_.try_lock())
        {
          tracktion::clearChannels (*fc.destBuffer, 0, -1, fc.bufferStartSample, fc.bufferNumSamples);
          return;
        }

      tracktion::clearChannels (*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);
      if (fc.bufferForMidiMessages != nullptr)
        {
          for (auto& m : *fc.bufferForMidiMessages)
            {
              int channel = m.getChannel();
              /* juce::MidiMessage::getChannel starts at 1, LiquidSFZ channels are in range [0:15] */
              if (channel > 0)
                channel--;

              if (m.isNoteOn())
                {
                  const int note = m.getNoteNumber();
                  const int noteTimeSample = std::clamp (juce::roundToInt (m.getTimeStamp() * sampleRate), 0, fc.bufferNumSamples);

                  synth_.add_event_note_on (noteTimeSample, channel, note, m.getVelocity());
                }
              else if (m.isNoteOff())
                {
                  const int note = m.getNoteNumber();
                  const int noteTimeSample = std::clamp (juce::roundToInt (m.getTimeStamp() * sampleRate), 0, fc.bufferNumSamples);

                  synth_.add_event_note_off (noteTimeSample, channel, note);
                }
              else if (m.isAllNotesOff() || m.isAllSoundOff())
                {
                  synth_.all_sound_off();    // NOTE: there is no extra "all notes off" in liquidsfz
                }
              /* TODO: synth_.add_event_pitch_bend */
            }
        }
      /* TODO: make a decision here how UI parameters (which ought to be automatable) map to CC values
       *
       *  - should the liquidsfz synth maintain different CC state for different midi channels?
       *  - should automating a parameter affect all notes (or only these on specific channels)?
       *  - should CC events affect all notes (or only those on specific channels)?
       *  - should we support per note modulation?
       *
       * synth_.add_event_cc (frame, channel, cc, cc_value);
       *
       * What proprietary sforzando SFZ player vst3 seems to do:
       *
       *  - it does not maintain different CC state for different midi channels (changing
       *    CC on midi channel 1 affects notes on midi channel 2)
       *  - automating a parameter affects all notes, regardless of midi channel
       *  - CC events affect all notes, regardless of midi channel
       *  - no support for per-note modulation
       */

      float *left = fc.destBuffer->getWritePointer (0, fc.bufferStartSample);
      float *right = fc.destBuffer->getWritePointer (1, fc.bufferStartSample);
      float *output[2] = { left, right };

      synth_.process (output, fc.bufferNumSamples);

      rt_mutex_.unlock();
    }
}

const char* LiquidSFZTracktionPlugin::xmlTypeName = "liquidsfz";

}
