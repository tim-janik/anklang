// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"
#include <ase/api.hh>
#include <ase/main.hh>
#include <ase/path.hh>
#include <ase/storage.hh>
#include <ase/testing.hh>
#include <ase/trkn.hh>

namespace {
using namespace Ase;

constexpr int audio_rate = 44100, audio_block = 512;
constexpr auto audio_interval = std::chrono::milliseconds ((1000 * audio_block + audio_rate - 1) / audio_rate);

// Temporary API-only exception for audio I/O; replace Tracktion access with Ase APIs in a future change.
class AudioTestDevice {
  tracktion::DeviceManager &manager_ = trkn_engine()->getDeviceManager();
  tracktion::HostedAudioDeviceInterface &device_ = manager_.getHostedAudioDeviceInterface();
  juce::AudioBuffer<float> buffer_ { 2, audio_block };
  juce::MidiBuffer midi_;
public:
  AudioTestDevice()
  {
    TASSERT (!manager_.deviceManager.getCurrentAudioDevice());
    tracktion::HostedAudioDeviceInterface::Parameters params;
    params.inputChannels = 0;
    params.outputChannels = 2;
    params.sampleRate = audio_rate;
    params.blockSize = audio_block;
    device_.initialise (params);
    device_.prepareToPlay (audio_rate, audio_block);
  }
  ~AudioTestDevice()
  {
    manager_.deviceManager.closeAudioDevice();
    manager_.closeDevices();
    manager_.removeHostedAudioDeviceInterface();
    manager_.rescanWaveDeviceList();
    TASSERT (!manager_.isHostedAudioDeviceInterfaceInUse());
  }
  static void
  write_signal (const String &file)
  {
    juce::AudioBuffer<float> signal (1, 5 * audio_rate);
    std::fill_n (signal.getWritePointer (0), signal.getNumSamples(), 0.25f);
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (new juce::FileOutputStream (juce::File (file)),
                                                                           audio_rate, 1, 16, {}, 0));
    TASSERT (writer && writer->writeFromAudioSampleBuffer (signal, 0, signal.getNumSamples()));
  }
  float
  next_block()
  {
    buffer_.clear();
    midi_.clear();
    device_.processBlock (buffer_, midi_);
    return std::min (buffer_.getRMSLevel (0, 0, audio_block), buffer_.getRMSLevel (1, 0, audio_block));
  }
  CoTaskVoid
  expect_sound (ProjectP project, int blocks)
  {
    for (int i = 0; i < blocks; i++)
      {
        co_await main_loop->delay (audio_interval);
        TASSERT (project->is_playing());
        TCMP (next_block(), >, 0.01f);
      }
  }
};

static CoTaskVoid
project_directory_audio_playback()
{
  AudioTestDevice audio;
  const String root = anklang_cachedir_create();
  TASSERT (!root.empty());
  const String file = Path::join (root, "signal.wav");
  audio.write_signal (file);

  ProjectP project = Server::instance().create_project ("");
  ClipP clip = project->create_track()->insert_wave_clip ("Signal", file, 0);
  TASSERT (clip && project->unnamed());
  const String old_dir = project->project_dir();
  co_await main_loop->delay (audio_interval);
  project->start_playback();
  auto playing = project->on_event ("notify:is_playing", [&] (const Event&) { TASSERT (project->is_playing()); });
  float level = 0;
  for (int i = 0; i < 100 && level <= 0.01f; i++)
    {
      level = audio.next_block();
      co_await main_loop->delay (audio_interval);
    }
  TCMP (level, >, 0.01f);
  co_await audio.expect_sound (project, 16);

  TASSERT (!project->save_project ("Playing Audio"));
  TASSERT (project->project_dir() != old_dir && !Path::check (old_dir, "e"));
  TASSERT (clip->original_file() == Path::join (project->project_dir(), "samples", "signal.wav"));
  co_await audio.expect_sound (project, 32);

  playing.disconnect();
  project->stop_playback();
  const String dir = project->project_dir();
  TASSERT (!project->remove_project_dir() && !Path::check (dir, "e"));
  project->discard();
  anklang_cachedir_cleanup (root);
}
TEST_ADD (project_directory_audio_playback);

} // Anon
