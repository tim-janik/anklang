// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first

#include "track.hh"
#include "project.hh"
#include "clip.hh"
#include "plugin.hh"
#include "server.hh"
#include "main.hh"
#include "path.hh"
#include "unicode.hh"
#include "jsonipc/jsonipc.hh"
#include "internal.hh"
#include <unistd.h>

namespace te = tracktion::engine;

namespace Ase {

// == TrackStateListener ==
class TrackImpl::TrackStateListener : public juce::ValueTree::Listener {
  TrackImpl &asetrack_;
  juce::ValueTree track_state_;
  juce::ValueTree volume_plugin_state_;
  te::LevelMeasurer::Client meter_client_;
  juce::WeakReference<te::LevelMeasurer> measurer_;
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
      }
    }
    if (auto vol = asetrack_.volume_plugin()) {
      volume_plugin_state_ = vol->state;
      volume_plugin_state_.addListener (this);
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
      if (property == tracktion::engine::IDs::volume) {
        asetrack_.emit_notify ("volume_db");
        asetrack_.emit_notify ("slider_pos");
      }
      if (property == tracktion::engine::IDs::pan)
        asetrack_.emit_notify ("pan");
    }
    asetrack_.update_telemetry();
  }
  void
  notify_clips ()
  {
    asetrack_.emit_notify ("launcher_clips");
    asetrack_.emit_notify ("all_clips");
  }
  void
  valueTreeChildAdded (juce::ValueTree &parent, juce::ValueTree &child) override
  {
    if (parent == track_state_ && te::Clip::isClipState (child))
      notify_clips();
  }
  void
  valueTreeChildRemoved (juce::ValueTree &parent, juce::ValueTree &child, int) override
  {
    if (parent == track_state_ && te::Clip::isClipState (child))
      notify_clips();
  }
  void valueTreeParentChanged (juce::ValueTree&) override {}
  void
  valueTreeChildOrderChanged (juce::ValueTree &parent, int, int newIndex) override
  {
    // Reordering clips changes the order of the clip lists, so emit like add/remove
    if (parent == track_state_ && te::Clip::isClipState (parent.getChild (newIndex)))
      notify_clips();
  }
};

// == TrackImpl ==
// Tracktion tracks whose getName() ignores setName()
static bool
trkn_fixed_name (const te::Track &track)
{
  return track.isMasterTrack() || track.isTempoTrack() || track.isMarkerTrack() || track.isArrangerTrack();
}

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
    if (!trkn_fixed_name (*trackp))
      {
        trackp->edit.getUndoManager().beginNewTransaction ("Set Track Name");
        trackp->setName (juce::String (n));
      }
}

ProjectImpl*
TrackImpl::project () const
{
  return project_;
}

bool
TrackImpl::muted() const
{
  if (auto t = track_.get())
    return t->isMuted (false);
  return false;
}

void
TrackImpl::muted (bool muted)
{
  if (auto t = track_.get())
    t->setMute (muted);
}

bool
TrackImpl::hidden() const
{
  if (auto t = track_.get())
    return t->isHidden ();
  return false;
}

void
TrackImpl::hidden (bool hidden)
{
  if (auto t = track_.get())
    t->setHidden (hidden);
}

bool
TrackImpl::solo() const
{
  if (auto t = track_.get())
    return t->isSolo (false);
  return false;
}

void
TrackImpl::solo (bool solo)
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

bool
TrackImpl::is_control_track() const
{
  if (auto t = track_.get())
    return t->isTempoTrack() || t->isMarkerTrack() || t->isChordTrack() ||
           t->isArrangerTrack() || t->isMasterTrack();
  return is_master();
}

te::VolumeAndPanPlugin*
TrackImpl::volume_plugin () const
{
  if (auto t = track_.get()) {
    if (t->isMasterTrack())
      return t->edit.getMasterVolumePlugin().get();
    if (auto at = dynamic_cast<te::AudioTrack*> (t))
      return at->getVolumePlugin();
  }
  return nullptr;
}

float
TrackImpl::volume_db() const
{
  if (auto vol = volume_plugin())
    return trkn_volume_db (*vol);
  return 0;
}

void
TrackImpl::volume_db (float db)
{
  if (auto vol = volume_plugin())
    trkn_volume_db (*vol, db);
}

float
TrackImpl::slider_pos() const
{
  if (auto vol = volume_plugin())
    return trkn_slider_pos (*vol);
  return 0;
}

void
TrackImpl::slider_pos (float pos)
{
  if (auto vol = volume_plugin())
    trkn_slider_pos (*vol, pos);
}

float
TrackImpl::pan() const
{
  if (auto vol = volume_plugin())
    return trkn_pan (*vol);
  return 0;
}

void
TrackImpl::pan (float pan)
{
  if (auto vol = volume_plugin())
    trkn_pan (*vol, pan);
}

StringS
TrackImpl::read_only_properties() const
{
  StringS props = { "all_clips", "launcher_clips" };
  auto t = track_.get();
  if (t && trkn_fixed_name (*t))
    props.push_back ("name");
  if (!dynamic_cast<te::AudioTrack*> (t) && !dynamic_cast<te::FolderTrack*> (t))
    props.insert (props.end(), { "muted", "solo" });
  if (!volume_plugin())
    props.insert (props.end(), { "pan", "slider_pos", "volume_db" });
  return props;
}

ClipS
TrackImpl::launcher_clips() const
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

void
TrackImpl::launcher_clips (const ClipS &)
{
  warning ("Track.launcher_clips is read-only");
}

ClipS
TrackImpl::all_clips() const
{
  ClipS clips;
  if (auto ct = dynamic_cast<te::ClipTrack*> (track_.get()))
    for (auto *clip : ct->getClips())
      clips.push_back (ClipImpl::from_trkn (*clip));
  return clips;
}

void
TrackImpl::all_clips (const ClipS &)
{
  warning ("Track.all_clips is read-only");
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

ClipP
TrackImpl::insert_wave_clip (const String &name, const String &utf8filename, double start)
{
  auto at = dynamic_cast<te::AudioTrack*> (track_.get());
  if (!at)
    {
      warning ("Track.insert_wave_clip: track holds no audio clips: %s", te_type_);
      return nullptr;
    }
  const String file = Path::realpath (Path::abspath (decodefs (utf8filename)));
  if (utf8filename.empty() || !Path::check (file, "fr"))
    {
      warning ("Track.insert_wave_clip: not an audio file: %s", file);
      return nullptr;
    }
  bool linked = false;
  const String reference = project_->admit_asset (file, linked);
  return_unless (!reference.empty(), nullptr);
  const String link = project_->resolve_asset (reference);
  te::AudioFile audiofile (at->edit.engine, juce::File (link));
  if (linked)
    at->edit.engine.getAudioFileManager().forceFileUpdate (audiofile);
  if (!audiofile.isValid())
    {
      if (linked)
        unlink (link.c_str());
      warning ("Track.insert_wave_clip: not an audio file: %s", file);
      return nullptr;
    }
  const te::ClipPosition position = { { tracktion::TimePosition::fromSeconds (start),
                                        tracktion::TimeDuration::fromSeconds (audiofile.getLength()) }, {} };
  auto undo_scope = project_->undo_scope (_("Insert Audio Clip"));
  auto clip = at->insertNewClip (te::TrackItem::Type::wave, juce::String (name), position, nullptr);
  if (clip)
    clip->getSourceFileReference().source = juce::String (reference);
  if (!clip && linked)
    unlink (link.c_str());
  return clip ? ClipImpl::from_trkn (*clip) : nullptr;
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
