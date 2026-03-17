// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "liquidsfzplugin.hh"

namespace Ase
{

/*
 * do not use a std::mutex here because it may not be hard RT safe to
 * try_lock() / unlock() it (depending on how the mutex is implemented)
 */
bool
LiquidSFZPlugin::RTMutex::try_lock()
{
  return !locked_flag.test_and_set();
}

void
LiquidSFZPlugin::RTMutex::wait_for_lock()
{
  while (!try_lock())
    {
      // this doesn't happen very often and we are in a non-RT thread, so we
      // can block it for some time
      //  => wait for less than one frame drawing time until trying again
      float fps = 240;
      usleep (1000 * 1000 / fps);
    }
}

void
LiquidSFZPlugin::RTMutex::unlock()
{
  locked_flag.clear();
}

LiquidSFZPlugin::LiquidSFZPlugin (tracktion::PluginCreationInfo info) : Plugin (info)
{
}

/* TODO: should load in worker thread */
bool
LiquidSFZPlugin::load (const String& filename)
{
  rt_mutex_.wait_for_lock();

  bool result;
  if (synth_.is_bank (filename))
    result = synth_.load_bank (filename) && synth_.select_program (0);
  else
    result = synth_.load (filename);

  rt_mutex_.unlock();

  return result;
}

void
LiquidSFZPlugin::initialise (const tracktion::PluginInitialisationInfo& info)
{
  rt_mutex_.wait_for_lock();
  synth_.set_sample_rate (info.sampleRate);
  synth_.all_sound_off();
  rt_mutex_.unlock();
}

void
LiquidSFZPlugin::deinitialise()
{
  rt_mutex_.wait_for_lock();
  synth_.all_sound_off();
  rt_mutex_.unlock();
}

void
LiquidSFZPlugin::applyToBuffer (const tracktion::PluginRenderContext& fc)
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
       */

      float *left = fc.destBuffer->getWritePointer (0, fc.bufferStartSample);
      float *right = fc.destBuffer->getWritePointer (1, fc.bufferStartSample);
      float *output[2] = { left, right };

      synth_.process (output, fc.bufferNumSamples);

      rt_mutex_.unlock();
    }
}

const char* LiquidSFZPlugin::xmlTypeName = "liquidsfz";

}
