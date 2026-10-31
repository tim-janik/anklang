// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first
#include <ase/project.hh>
#include <ase/main.hh>
#include <ase/clip.hh>
#include <ase/track.hh>
#include <ase/testing.hh>
#include <ase/path.hh>
#include <ase/platform.hh>
#include <ase/storage.hh>

namespace Ase {

// Temporary API-only exception for sample playback setup; switch to Ase APIs in a future change.
static tracktion::WaveAudioClip::Ptr
load_audio_file_as_clip (tracktion::Edit &edit, const juce::File &file)
{
  edit.ensureNumberOfAudioTracks (1);
  if (auto track = tracktion::getAudioTracks (edit)[0]) {
    auto clip = TrackImpl::from_trkn (*track)->insert_wave_clip (file.getFileNameWithoutExtension().toStdString(),
                                                                 file.getFullPathName().toStdString(), 0);
    if (clip)
      for (auto candidate : track->getClips())
        if (ClipImpl::from_trkn (*candidate) == clip)
          return dynamic_cast<tracktion::WaveAudioClip*> (candidate);
  }
  return {};
}

template<typename ClipType> static typename ClipType::Ptr
loop_around_clip (ClipType &clip)
{
  using namespace std::literals;
  auto &transport = clip.edit.getTransport();
  transport.setLoopRange (clip.getEditTimeRange());
  transport.looping = true;
  transport.setPosition (0s);
  return clip;
}

void
test_audio_sample_load()
{
  ProjectImplP project = ProjectImpl::create ("AudioSampleTest");
  TASSERT (project);

  // Verify project was created successfully
  TASSERT (project->name() == "AudioSampleTest");
  TASSERT (project->bpm() == 120.0);

  // Load audio sample as clip (moved from project.cc test_setup)
  const std::string sample01 = anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg");
  juce::File sampleFile (sample01);
  auto clip = load_audio_file_as_clip (*project->edit_, sampleFile);
  TASSERT (clip != nullptr);
  TASSERT (clip->getOriginalFile() == sampleFile && clip->getAudioFile().isValid());
  loop_around_clip (*clip);
  project->edit_->getTransport().ensureContextAllocated();

  project->discard();
}
TEST_ADD (test_audio_sample_load);

// Temporary API-only exception for rendering and recording; switch to Ase APIs in a future change.
CoTaskVoid
test_project_directory_audio()
{
  namespace te = tracktion::engine;
  ProjectImplP project = ProjectImpl::create ("");
  TrackP track = project->create_track();
  const String root = anklang_cachedir_create(), sample = Path::join (root, "piano.ogg");
  TASSERT (Path::copy_file (anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg"), sample));
  ClipP clip = track->insert_wave_clip ("Linked Piano", sample, 0);
  TASSERT (clip && project->unnamed());
  const String old_dir = project->project_dir();
  te::Edit &edit = *project->edit_;
  te::AudioTrack *audio_track = nullptr;
  for (auto candidate : te::getAudioTracks (edit))
    if (candidate->getName().toStdString() == track->name())
      audio_track = candidate;
  TASSERT (audio_track && audio_track->getClips().size() == 1);
  auto render = [&] (const String &file, int clip_index) -> CoTaskVoid {
    te::Renderer::Parameters params (edit);
    params.destFile = juce::File (file);
    params.audioFormat = edit.engine.getAudioFileFormatManager().getWavFormat();
    params.time = { {}, tracktion::TimeDuration::fromSeconds (0.1) };
    params.tracksToDo.setRange (0, te::getAllTracks (edit).size(), true);
    params.allowedClips.add (audio_track->getClips()[clip_index]);
    edit.getTransport().freePlaybackContext();
    te::Renderer::turnOffAllPlugins (edit);
    {
      te::Renderer::RenderTask task ("Project rename", params, nullptr, nullptr);
      while (task.runJob() != juce::ThreadPoolJob::jobHasFinished)
        co_await main_loop->delay (std::chrono::milliseconds (1));
      TASSERT (task.errorMessage.isEmpty());
    }
    TASSERT (params.destFile.existsAsFile());
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader (format.createReaderFor (new juce::FileInputStream (params.destFile), true));
    TASSERT (reader && reader->lengthInSamples > 0);
    juce::AudioBuffer<float> audio (2, int (reader->lengthInSamples));
    TASSERT (reader->read (&audio, 0, audio.getNumSamples(), 0, true, true));
    TASSERT (audio.getMagnitude (0, audio.getNumSamples()) > 0.001f);
  };
  auto record = [&] {
    te::EditPlaybackContext context (edit.getTransport());
    context.prepareForPlaying ({});
    te::WaveInputDevice input (edit.engine, "Test", te::WaveDeviceDescription ("Project Rename", 0, -1, true),
                               te::InputDevice::waveDevice);
    input.setOutputFormat (edit.engine.getAudioFileFormatManager().getWavFormat()->getFormatName());
    input.setFilenameMask ("%projectdir%/recording-%take%");
    input.setRecordTriggerDb (-60);
    std::unique_ptr<te::InputDeviceInstance> instance (input.createInstance (context));
    const auto duration = tracktion::TimeDuration::fromSeconds (0.1);
    auto prepared = instance->prepareToRecord ({ { {}, duration }, { audio_track->itemID } });
    TASSERT (prepared.size() == 1 && prepared[0]);
    std::vector<std::unique_ptr<te::InputDeviceInstance::RecordingContext>> contexts;
    contexts.push_back (std::move (prepared[0].value()));
    TASSERT (instance->startRecording (std::move (contexts)).empty());
    context.play();
    const String file = instance->getRecordingFile (audio_track->itemID).getFullPathName().toStdString();
    TASSERT (Path::dircontains (project->project_dir(), file));
    float samples[512];
    std::fill_n (samples, 512, 0.25f);
    const float *channels[] = { samples };
    const double stream_time = edit.engine.getDeviceManager().getCurrentStreamTime();
    for (int block = 0; block < 10; block++)
      input.consumeNextAudioBlock (channels, 1, 512, stream_time + block * 512.0 / 44100);
    context.getNodePlayHead()->setPosition (tracktion::toSamples (duration, 44100));
    auto result = instance->stopRecording ({ .unloopedTimeToEndRecording = tracktion::TimePosition::fromSeconds (0.1) });
    TASSERT (result && result->size() == 1);
    TASSERT (te::AudioFile (edit.engine, juce::File (file)).getLength() > 0);
    project->stop_playback();
    return file;
  };
  co_await render (Path::join (old_dir, "before.wav"), 0);
  const String recording = record();
  project->start_playback();
  TASSERT (project->is_playing());
  auto *playback = edit.getTransport().getCurrentPlaybackContext();
  TASSERT (!project->save_project ("Renamed Audio"));
  TASSERT (project->is_playing());
  TASSERT (edit.getTransport().getCurrentPlaybackContext() == playback);
  auto wave = dynamic_cast<te::WaveAudioClip*> (audio_track->getClips()[0]);
  TASSERT (wave && wave->getAudioFile().getFile() == juce::File (clip->original_file()));
  project->stop_playback();
  const String dir = project->project_dir();
  TASSERT (Path::check (Path::join (dir, Path::basename (recording)), "f"));
  co_await render (Path::join (dir, "after.wav"), 0);
  co_await render (Path::join (dir, "recorded.wav"), 1);
  TASSERT (Path::dircontains (dir, record()));
  project->start_playback();
  TASSERT (!project->remove_project_dir() && !project->is_playing());
  TASSERT (wave->getAudioFile().getFile() == juce::File());
  project->discard();
  anklang_cachedir_cleanup (root);
}
TEST_ADD (test_project_directory_audio);

} // Ase
