// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first

#include "trkn.hh"
#include "platform.hh"
#include "utils.hh"
#include "main.hh"
#include "project.hh"
#include "internal.hh"

namespace te = tracktion::engine;

namespace Ase {

void // see tracktion_engine.h
trkn_tracktion_log_msg (const juce::String &msg)
{
  diag ("TRACKTION: %s", msg.toStdString());
}

void // see tracktion_engine.h
trkn_tracktion_log_error (const juce::String &errmsg)
{
  // TRACKTION_LOG_ERROR is mostly used for IO or exec errors
  warning ("TRACKTION: error: %s", errmsg.toStdString());
}

struct EngineBehaviour : te::EngineBehaviour {
  bool nodevs = false;
  bool autoInitialiseDeviceManager () override          { return !nodevs; }
};

struct TrknApp : juce::JUCEApplication {
  std::unique_ptr<tracktion::Engine> engine;
  ~TrknApp()
  {
    // juce keeps a global JUCEApplicationBase::appInstance around
    assert_return_unreached();
  }
  TrknApp ()
  {
    // leave JUCEApplicationBase::createInstance unused
  }
  void
  initialise (const juce::String &whatstring) override
  {}
  void
  start_engine (std::unique_ptr<EngineBehaviour> engine_behaviour)
  {
    assert_return (!engine);
    engine = std::make_unique<tracktion::Engine> (Ase::application_name(), std::make_unique<tracktion::UIBehaviour>(), std::move (engine_behaviour));
    auto &deviceManager = engine->getDeviceManager();
    // deviceManager.initialise (0, 2);         // 0 inputs, 2 stereo outputs
    if (engine->getEngineBehaviour().autoInitialiseDeviceManager())
      deviceManager.rescanMidiDeviceList();     // run scan in 5ms instead of 4000ms
  }
  void
  shutdown() override
  {
    engine = nullptr;
  }
  const juce::String
  getApplicationName() override
  {
    return Ase::application_name();
  }
  const juce::String
  getApplicationVersion() override
  {
    return Ase::ase_version();
  }
};
static TrknApp *trkn_app = nullptr;

void
trkn_shutdown ()
{
  return_unless (trkn_app);
  ProjectImpl::force_shutdown_all();
  main_loop->iterate_pending();
  trkn_app->shutdownApp();
}

struct AseLogger : public juce::Logger {
  void
  logMessage (const juce::String &msg) override
  {
    diag ("JUCE: %s", msg.toStdString());
  }
};

/** Setup @ref tracktion and @ref tracktion::engine.
 * Initializes juce::JUCEApplication, creates tracktion::engine::Engine, tracktion::engine::DeviceManager,
 * scans for Audio and MIDI devices to preapre for playback. Interesting tracktion classes:
 * tracktion::engine::Project tracktion::engine::Plugin tracktion::engine::BackgroundJobManager
 * tracktion::engine::Edit tracktion::engine::Track tracktion::engine::Clip tracktion::engine::TransportControl
 */
bool
trkn_init (int argc, char *argv[], bool nodevs)
{
  assert_return (!trkn_app && main_loop, false);
  static AseLogger *logger = new AseLogger();
  juce::Logger::setCurrentLogger (logger);
  // juce requires a JUCEApplicationBase instance
  trkn_app = new TrknApp();
  if (!trkn_app->initialiseApp()) {
    trkn_app->shutdownApp();
    return false;
  }
  // create 1 tracktion engine and configure PCM / MIDI device scan
  auto engine_behaviour = std::make_unique<EngineBehaviour>();
  engine_behaviour->nodevs = nodevs;
  trkn_app->start_engine (std::move (engine_behaviour));
  // process tracktion setup callbacks
  main_loop->iterate_pending();
  return true;
}

tracktion::Engine*
trkn_engine ()
{
  return trkn_app ? trkn_app->engine.get() : nullptr;
}

class TransportListener : public juce::ChangeListener, public tracktion::TransportControl::Listener
{
public:
  TransportListener (tracktion::TransportControl &tc)
    : transport (tc)
  {
    transport.addChangeListener (this); // for ChangeListener
    transport.addListener (this);       // for TransportControl::Listener
    Ase::printerr ("TransportListener attached.\n");
  }
  ~TransportListener() override
  {
    transport.removeListener (this);
    transport.removeChangeListener (this);
    Ase::printerr ("TransportListener detached.\n");
  }
  void
  changeListenerCallback (juce::ChangeBroadcaster *source) override
  {
    if (source == &transport) {
      transport_changed ("change");
    }
  }
  void autoSaveNow             () override {}
  void setAllLevelMetersActive (bool become_inactive) override {}
  void setVideoPosition        (tracktion::TimePosition pos, bool force_jump) override {}
  void recordingStarted        (tracktion::SyncPoint start, std::optional<tracktion::TimeRange> punch_range) override {}
  void recordingStopped        (tracktion::SyncPoint sync_point, bool discard_recordings) override {}
  void recordingAboutToStart   (tracktion::InputDeviceInstance &device, tracktion::EditItemID target) override {}
  void recordingAboutToStop    (tracktion::InputDeviceInstance &device, tracktion::EditItemID target) override {}
  void recordingFinished       (tracktion::InputDeviceInstance &device, tracktion::EditItemID target,
                                const juce::ReferenceCountedArray<tracktion::Clip> &recording) override {}
  void
  playbackContextChanged () override
  {
    tracktion::EditPlaybackContext *context = transport.getCurrentPlaybackContext();
    Ase::printerr ("PlaybackContextChanged: context=%p graph=%d playing=%d position=%.3fsecs\n", context,
                   context ? context->isPlaybackGraphAllocated() : 0,
                   context ? context->isPlaying() : 0,
                   context ? context->getPosition().inSeconds() : 0);
  }
  void
  startVideo () override
  {
    transport_changed ("start-video");
    if (ppt == LoopID::INVALID)
      ppt = main_loop->add ([this] { this->poll_position(); return true; }, std::chrono::milliseconds (200));
  }
  void
  stopVideo () override
  {
    transport_changed ("stop-video");
    main_loop->cancel (&ppt);
  }
  void
  transport_changed (const std::string &what)
  {
    auto position = transport.getPosition();
    Ase::printerr ("Transport: playing=%d position=%.3fsecs (%s)\n",
                   transport.isPlaying(), position.inSeconds(), what.c_str());
  }
  void
  poll_position()
  {
    transport_changed ("position");
  }
  private:
  tracktion::TransportControl &transport;
  LoopID ppt = LoopID::INVALID;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportListener)
};

static std::unique_ptr<tracktion::Edit> edit;
static std::unique_ptr<TransportListener> transport_listener;

namespace EngineHelpers
{
tracktion::AudioTrack*
getOrInsertAudioTrackAt (tracktion::Edit &edit, int index)
{
  edit.ensureNumberOfAudioTracks (index + 1);
  return tracktion::getAudioTracks (edit)[index];
}
void
removeAllClips (tracktion::AudioTrack &track)
{
  auto clips = track.getClips();

  for (int i = clips.size(); --i >= 0;)
    clips.getUnchecked (i)->removeFromParent();
}
tracktion::WaveAudioClip::Ptr
loadAudioFileAsClip (tracktion::Edit &edit, const juce::File &file)
{
  // Find the first track and delete all clips from it
  if (auto track = getOrInsertAudioTrackAt (edit, 0)) {
    removeAllClips (*track);

    // Add a new clip to this track
    tracktion::AudioFile audioFile (edit.engine, file);

    if (audioFile.isValid())
      if (auto newClip = track->insertWaveClip (file.getFileNameWithoutExtension(), file,
    { { {}, tracktion::TimeDuration::fromSeconds (audioFile.getLength()) }, {} }, false))
    return newClip;
  }

  return {};
}
template<typename ClipType> typename ClipType::Ptr
loopAroundClip (ClipType &clip)
{
  using namespace std::literals;
  auto &transport = clip.edit.getTransport();
  transport.setLoopRange (clip.getEditTimeRange());
  transport.looping = true;
  transport.setPosition (0s);
  return clip;
}
} // EngineHelpers

} // Ase
