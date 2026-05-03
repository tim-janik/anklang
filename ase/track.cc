// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first

#include "track.hh"
#include "project.hh"
#include "clip.hh"
#include "plugin.hh"
#include "server.hh"
#include "main.hh"
#include "jsonipc/jsonipc.hh"
#include "internal.hh"

namespace te = tracktion::engine;

namespace Ase {

// == TrackStateListener ==
class TrackImpl::TrackStateListener : public juce::ValueTree::Listener {
  TrackImpl &asetrack_;
  juce::ValueTree track_state_;
  juce::ValueTree volume_plugin_state_;
  te::LevelMeasurer::Client meter_client_;
  te::LevelMeasurer *measurer_ = nullptr;
  FastMemory::Block telemetry_block_;
public:
  struct Telemetry {
    float dbspl0 = -100.0f;
    float dbspl1 = -100.0f;
  } &telemetry;
  TrackStateListener (TrackImpl &asetrack) :
    asetrack_ (asetrack), track_state_ (asetrack_.track_->state),
    telemetry_block_ (SERVER->telemem_allocate (sizeof (Telemetry))),
    telemetry (*new (telemetry_block_.block_start) Telemetry{})
  {
    track_state_.addListener (this);
    if (auto t = asetrack_.track_.get()) {
      if (auto at = dynamic_cast<te::AudioTrack *> (t)) {
        if (auto lmp = at->getLevelMeterPlugin()) {
          measurer_ = &lmp->measurer;
          measurer_->addClient (meter_client_);
        }
        if (auto vol = at->getVolumePlugin()) {
          volume_plugin_state_ = vol->state;
          volume_plugin_state_.addListener (this);
        }
      }
    }
  }
  ~TrackStateListener() override
  {
    if (volume_plugin_state_.isValid())
      volume_plugin_state_.removeListener (this);
    if (measurer_)
      measurer_->removeClient (meter_client_);
    track_state_.removeListener (this);
    SERVER->telemem_release (telemetry_block_);
  }
  std::pair<float,float>
  get_levels()
  {
    if (!measurer_)
      return { -100.0f, -100.0f };
    te::DbTimePair left = meter_client_.getAndClearAudioLevel (0);
    te::DbTimePair right = meter_client_.getAndClearAudioLevel (1);
    return { left.dB, right.dB };
  }
  void
  valueTreePropertyChanged (juce::ValueTree &tree, const juce::Identifier &property) override
  {
    if (tree == track_state_) {
      if (property == tracktion::engine::IDs::name)
        asetrack_.emit_notify ("name");
      if (property == tracktion::engine::IDs::mute)
        asetrack_.emit_notify ("muted");
      if (property == tracktion::engine::IDs::hidden)
        asetrack_.emit_notify ("hidden");
      if (property == tracktion::engine::IDs::solo)
        asetrack_.emit_notify ("solo");
    }
    if (tree == volume_plugin_state_) {
      if (property == tracktion::engine::IDs::volume)
        asetrack_.emit_notify ("volume");
      if (property == tracktion::engine::IDs::pan)
        asetrack_.emit_notify ("pan");
    }
    asetrack_.update_telemetry();
  }
  void
  valueTreeChildAdded (juce::ValueTree &parent, juce::ValueTree &child) override
  {
    if (parent == track_state_ && te::Clip::isClipState (child))
      asetrack_.emit_notify ("launcher_clips");
  }
  void
  valueTreeChildRemoved (juce::ValueTree &parent, juce::ValueTree &child, int) override
  {
    if (parent == track_state_ && te::Clip::isClipState (child))
      asetrack_.emit_notify ("launcher_clips");
  }
  void valueTreeParentChanged (juce::ValueTree&) override {}
  void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
};

// == TrackImpl ==
static std::string
trkn_track_type (tracktion::Track &track)
{
  if (dynamic_cast<te::FolderTrack*> (&track))
    return "Folder";
  if (dynamic_cast<te::AudioTrack*> (&track))
    return "Audio/Midi";
  if (track.isTempoTrack())
    return "Tempo";
  if (track.isMarkerTrack())
    return "Marker";
  if (track.isChordTrack())
    return "Chord";
  if (track.isArrangerTrack())
    return "Arranger";
  if (track.isMasterTrack())
    return "Master";
  return "Unknown";
}

TrackImplP
TrackImpl::from_trkn (tracktion::Track &t)
{
  TrackImpl *track = find_ase_obj<TrackImpl> (t);
  if (track)
    return shared_ptr_cast<TrackImpl> (track);
  TrackImplP trackp = TrackImpl::make_shared (t);
  return trackp;
}

TrackImpl::TrackImpl (tracktion::Track &track) :
  project_ (find_ase_obj<ProjectImpl> (track.edit)),
  track_ (&track), te_type_ (trkn_track_type (track))
{
  register_ase_obj (this, track);
  state_listener_ = std::make_unique<TrackStateListener> (*this);
  update_telemetry();
}

TrackImpl::TrackImpl (ProjectImpl &project, bool masterflag) :
  project_ (&project)
{
  gadget_flags (masterflag ? MASTER_TRACK : 0);
}

TrackImpl::~TrackImpl()
{
  unregister_ase_obj (this, track_.get());
  state_listener_ = nullptr;
  assert_return (_parent() == nullptr);
}

String
TrackImpl::name() const
{
  if (auto trackp = track_.get())
    return trackp->getName().toStdString();
  return "";
}

void
TrackImpl::name (const std::string &n)
{
  if (auto trackp = track_.get())
    trackp->setName (juce::String (n));
}

ProjectImpl*
TrackImpl::project () const
{
  return project_;
}

String
TrackImpl::fallback_name() const
{
  if (is_master())
    return "Master";
  if (auto project_ = project()) {
    ssize_t i = project_->track_index (*this);
    return string_format ("Track %u", i >= 0 ? i + 1 : i);
  }
  return DeviceImpl::fallback_name();
}

void
TrackImpl::midi_channel (int32 midichannel)
{
  midichannel = CLAMP (midichannel, 0, 16);
  return_unless (midichannel != midi_channel_);
  midi_channel_ = midichannel;
  emit_notify ("midi_channel");
}

bool
TrackImpl::is_muted() const
{
  if (auto t = track_.get())
    return t->isMuted (false);
  return false;
}

void
TrackImpl::set_muted (bool muted)
{
  if (auto t = track_.get())
    t->setMute (muted);
}

bool
TrackImpl::is_hidden() const
{
  if (auto t = track_.get())
    return t->isHidden ();
  return false;
}

void
TrackImpl::set_hidden (bool hidden)
{
  if (auto t = track_.get())
    t->setHidden (hidden);
}

bool
TrackImpl::is_solo() const
{
  if (auto t = track_.get())
    return t->isSolo (false);
  return false;
}

void
TrackImpl::set_solo (bool solo)
{
  if (auto t = track_.get())
    t->setSolo (solo);
}

bool
TrackImpl::is_master() const
{
  if (auto t = track_.get())
    return t->isMasterTrack();
  return false;
}

double
TrackImpl::volume() const
{
  if (auto t = track_.get())
    if (auto at = dynamic_cast<te::AudioTrack*> (t))
      if (auto vol = at->getVolumePlugin())
        return vol->getVolumeDb();
  return 0.0;
}

void
TrackImpl::volume (double db)
{
  if (auto t = track_.get())
    if (auto at = dynamic_cast<te::AudioTrack*> (t))
      if (auto vol = at->getVolumePlugin())
        vol->setVolumeDb (db);
}

double
TrackImpl::pan() const
{
  if (auto t = track_.get())
    if (auto at = dynamic_cast<te::AudioTrack*> (t))
      if (auto vol = at->getVolumePlugin())
        return vol->getPan();
  return 0.0;
}

void
TrackImpl::pan (double pan)
{
  if (auto t = track_.get())
    if (auto at = dynamic_cast<te::AudioTrack*> (t))
      if (auto vol = at->getVolumePlugin())
        vol->setPan (pan);
}

ClipS
TrackImpl::launcher_clips()
{
  ClipS clips;
  if (auto t = track_.get())
    if (auto ct = dynamic_cast<te::ClipTrack*> (t))
      for (auto *clip : ct->getClips())
        if (dynamic_cast<te::MidiClip*> (clip))
          if (auto clipimpl = ClipImpl::from_trkn (*clip))
            clips.push_back (clipimpl);
  return clips;
}

ssize_t
TrackImpl::clip_index (const ClipImpl &clip) const
{
  if (auto t = track_.get())
    if (auto ct = dynamic_cast<te::ClipTrack *> (t)) {
      auto &clips = ct->getClips();
      for (int i = 0; i < clips.size(); i++)
        if (clips[i] == clip.clip_.get())
          return i;
    }
  return -1;
}

int
TrackImpl::clip_succession (const ClipImpl &clip) const
{
  ssize_t idx = clip_index (clip);
  if (idx < 0)
    return NONE;
  if (auto t = track_.get())
    if (auto ct = dynamic_cast<te::ClipTrack *> (t)) {
      auto &clips = ct->getClips();
      if (idx + 1 < clips.size())
        return idx + 1;
    }
  return NONE;
}

DeviceP
TrackImpl::access_device()
{
  return nullptr;
}

MonitorP
TrackImpl::create_monitor (int32 ochannel)
{
  return nullptr;
}

TelemetryFieldS
TrackImpl::telemetry() const
{
  TelemetryFieldS v;
  return_unless (state_listener_, v);
  auto &t = state_listener_->telemetry;
  v.push_back (telemetry_field ("dbspl0", &t.dbspl0));
  v.push_back (telemetry_field ("dbspl1", &t.dbspl1));
  return v;
}

void
TrackImpl::update_telemetry()
{
  return_unless (state_listener_);
  auto &t = state_listener_->telemetry;
  auto [left, right] = state_listener_->get_levels();
  t.dbspl0 = left;
  t.dbspl1 = right;
}

DeviceInfo
TrackImpl::device_info()
{
  return {};
}

void
TrackImpl::remove_self ()
{
  return_unless (!is_master());
  auto *track = track_.get();
  if (track) {
    // Remove from edit's track list
    track_->edit.deleteTrack (track);
    // Clear references
    track_ = SelectableWeakref<tracktion::Track>{};
    state_listener_ = nullptr;
  }
  GadgetImpl::remove_self();
}

// == TrackImpl::ClipScout ==
TrackImpl::ClipScout::ClipScout() noexcept
{
  // PRNG initialization goes here
}

/// Setup clip succession order.
void
TrackImpl::ClipScout::setup (const std::vector<int> &indices)
{
  indices_ = indices;
}

/// Determine clip succession.
int
TrackImpl::ClipScout::advance (int previous)
{
  if (previous >= 0 && previous < indices_.size()) {
    last_ = previous;
    return indices_[last_];
  }
  return NONE;
}

/// Reset state (history), preserves succession order.
void
TrackImpl::ClipScout::reset()
{
  last_ = -1;
}

/// Assign new succession order, preserves history.
void
TrackImpl::ClipScout::update (const ClipScout &other)
{
  indices_ = other.indices_;
}

ClipP
TrackImpl::create_midi_clip (const String &name, double start, double length)
{
  if (auto t = track_.get()) {
    if (auto at = dynamic_cast<tracktion::AudioTrack *> (t)) {
      const tracktion::TimeRange range (tracktion::TimePosition::fromSeconds (start), tracktion::TimeDuration::fromSeconds (length));
      auto                       clip = at->insertMIDIClip (juce::String (name), range, nullptr);
      if (clip)
        return ClipImpl::from_trkn (*clip);
      else
        warning ("insertMIDIClip returned null");
    }
    else
      warning ("dynamic_cast<AudioTrack*> failed for track type: %s", trkn_track_type (*t).c_str());
  }
  else
    warning ("track_.get() returned null");
  return nullptr;
}

ClipP
TrackImpl::create_audio_clip (const String &name, double start, double length)
{
  if (auto t = track_.get()) {
    if (auto ct = dynamic_cast<tracktion::ClipTrack *> (t)) {
      const tracktion::TimeRange range (tracktion::TimePosition::fromSeconds (start), tracktion::TimeDuration::fromSeconds (length));
      auto                       clip = ct->insertNewClip (tracktion::TrackItem::Type::wave, juce::String (name), range, nullptr);
      if (clip)
        return ClipImpl::from_trkn (*clip);
      else
        warning ("insertNewClip returned null");
    }
    else
      warning ("dynamic_cast<ClipTrack*> failed for track type: %s", trkn_track_type (*t).c_str());
  }
  else
    warning ("track_.get() returned null");
  return nullptr;
}

PluginP
TrackImpl::create_plugin (const String &type)
{
  if (auto t = track_.get()) {
    auto plugin = t->edit.getPluginCache().createNewPlugin (type.c_str(), {});
    if (plugin) {
      t->pluginList.insertPlugin (plugin, 0, nullptr);
      return PluginImpl::from_trkn (*plugin);
    }
    else
      warning ("createNewPlugin returned null");
  }
  else
    warning ("track_.get() returned null");
  return nullptr;
}

PluginS
TrackImpl::list_plugins()
{
  PluginS plugins;
  if (auto t = track_.get()) {
    auto &pluginList = t->pluginList;
    for (int i = 0; i < pluginList.size(); i++) {
      if (auto *plugin = pluginList[i]) {
        if (auto pluginimpl = PluginImpl::from_trkn (*plugin))
          plugins.push_back (pluginimpl);
      }
    }
  }
  return plugins;
}

} // Ase
