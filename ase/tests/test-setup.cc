// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first
#include <ase/project.hh>
#include <ase/clip.hh>
#include <ase/track.hh>
#include <ase/testing.hh>
#include <ase/path.hh>
#include <ase/platform.hh>

namespace Ase {

static tracktion::WaveAudioClip::Ptr
load_audio_file_as_clip (tracktion::Edit &edit, const juce::File &file)
{
  edit.ensureNumberOfAudioTracks (1);
  if (auto track = tracktion::getAudioTracks (edit)[0]) {
    tracktion::AudioFile audioFile (edit.engine, file);
    if (audioFile.isValid())
      if (auto newClip =
          track->insertWaveClip (file.getFileNameWithoutExtension(), file,
                                 { { {}, tracktion::TimeDuration::fromSeconds (audioFile.getLength()) }, {} }, false))
        return newClip;
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
  loop_around_clip (*clip);
  project->edit_->getTransport().ensureContextAllocated();

  project->discard();
}
TEST_ADD (test_audio_sample_load);

} // Ase
