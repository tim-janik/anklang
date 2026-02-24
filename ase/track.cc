// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first

#include "track.hh"
#include "project.hh"
#include "clip.hh"
#include "server.hh"
#include "main.hh"
#include "serialize.hh"
#include "jsonipc/jsonipc.hh"
#include "internal.hh"

namespace te = tracktion::engine;

namespace Ase {

// == TrackStateListener ==
class TrackImpl::TrackStateListener : public juce::ValueTree::Listener {
  TrackImpl &asetrack_;
  juce::ValueTree track_state_; // similar to a *shared_ptr
public:
  TrackStateListener (TrackImpl &asetrack) :
    asetrack_ (asetrack), track_state_ (asetrack_.track_->state)
  {
    track_state_.addListener (this);
  }
  ~TrackStateListener() override
  {
    track_state_.removeListener (this);
  }
  void
  valueTreePropertyChanged (juce::ValueTree &tree, const juce::Identifier &property) override
  {
    return_unless (tree == track_state_);
    if (property == tracktion::engine::IDs::name)
      asetrack_.emit_notify ("name");
  }
  void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
  void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
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
  TrackImpl *track = SelectableHandle::find_selectable_handle<TrackImpl> (t);
  if (track)
    return shared_ptr_cast<TrackImpl> (track);
  TrackImplP trackp = TrackImpl::make_shared (t);
  return trackp;
}

#if 0 // TODO: cleanup
// Helper struct to hold UI data for your rendering engine
struct TrackUIData
{
  juce::String name;
  juce::String type;
  juce::Colour colour;
  int depth;
  bool isFolder;
  bool isMuted;
  bool isSoloed;

  // Audio specific (optional)
  float volumeDb = 0.0f;
  float pan = 0.0f;
  juce::String inputName;
  juce::String outputName;
  void dummy()
  {
    TrackUIData data;
    data.depth = depth;
    data.name = t.getName();
    data.colour = t.getColour();
    data.isMuted = t.isMuted (true); // includeMutingByDestination
    data.isSoloed = t.isSolo (true); // includeIndirectSolo
    // Folder tracks might have a VCA plugin or Volume plugin
    if (auto vol = ft->getVolumePlugin())
      {
        data.volumeDb = vol->getVolumeDb();
        data.pan = vol->getPan();
      }
    // Access Volume/Pan via the VolumeAndPanPlugin
    if (auto vol = at->getVolumePlugin())
      {
        data.volumeDb = vol->getVolumeDb();
        data.pan = vol->getPan();
      }
    // Input Device Name
    auto& waveIn = at->getWaveInputDevice();
    if (waveIn.isEnabled())
      data.inputName = waveIn.getName();
    else
      data.inputName = "No Input";
    // Output Name
    data.outputName = at->getOutput().getOutputName();
  }
};
#endif

TrackImpl::TrackImpl (tracktion::Track &track) :
  project_ (SelectableHandle::find_selectable_handle<ProjectImpl> (track.edit)),
  track_ (&track), te_type_ (trkn_track_type (track))
{
  state_listener_ = std::make_unique<TrackStateListener> (*this);
}

TrackImpl::TrackImpl (ProjectImpl &project, bool masterflag) :
  project_ (&project)
{
  gadget_flags (masterflag ? MASTER_TRACK : 0);
}

TrackImpl::~TrackImpl()
{
  state_listener_ = nullptr;
  assert_return (_parent() == nullptr);
}

String
TrackImpl::get_name () const
{
  if (auto trackp = track_.get())
    return trackp->getName().toStdString();
  return "";
}

void
TrackImpl::set_name (const std::string &n)
{
  if (auto trackp = track_.get())
    trackp->setName (juce::String (n));
}

ProjectImpl*
TrackImpl::project () const
{
  return static_cast<ProjectImpl*> (_parent());
}

String
TrackImpl::fallback_name () const
{
  if (is_master())
    return "Master";
  if (auto project_ = project())
    {
      ssize_t i = project_->track_index (*this);
      return string_format ("Track %u", i >= 0 ? i + 1 : i);
    }
  return DeviceImpl::fallback_name();
}

void
TrackImpl::serialize (WritNode &xs)
{
  // TODO: use trkn
}

void
TrackImpl::_activate ()
{
  assert_return (!is_active() && _parent());
  DeviceImpl::_activate();
  midi_prod_->_activate();
  chain_->_activate();
}

void
TrackImpl::_deactivate ()
{
  assert_return (is_active());
  chain_->_deactivate();
  midi_prod_->_deactivate();
  DeviceImpl::_deactivate();
}

void
TrackImpl::midi_channel (int32 midichannel) // TODO: implement
{
  midichannel = CLAMP (midichannel, 0, 16);
  return_unless (midichannel != midi_channel_);
  midi_channel_ = midichannel;
  emit_notify ("midi_channel");
}

bool
TrackImpl::is_muted () const
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
TrackImpl::is_solo () const
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

double
TrackImpl::get_volume () const
{
  if (auto t = track_.get())
    if (auto at = dynamic_cast<te::AudioTrack*> (t))
      if (auto vol = at->getVolumePlugin())
        return vol->getVolumeDb();
  return 0.0;
}

void
TrackImpl::set_volume (double db)
{
  if (auto t = track_.get())
    if (auto at = dynamic_cast<te::AudioTrack*> (t))
      if (auto vol = at->getVolumePlugin())
        vol->setVolumeDb (db);
}

double
TrackImpl::get_pan () const
{
  if (auto t = track_.get())
    if (auto at = dynamic_cast<te::AudioTrack*> (t))
      if (auto vol = at->getVolumePlugin())
        return vol->getPan();
  return 0.0;
}

void
TrackImpl::set_pan (double pan)
{
  if (auto t = track_.get())
    if (auto at = dynamic_cast<te::AudioTrack*> (t))
      if (auto vol = at->getVolumePlugin())
        vol->setPan (pan);
}

ClipS
TrackImpl::launcher_clips ()
{
  return {}; // TODO: implement via trkn clips
}

ssize_t
TrackImpl::clip_index (const ClipImpl &clip) const
{
  return {}; // TODO: implement via trkn clips
}

int
TrackImpl::clip_succession (const ClipImpl &clip) const
{
  return {}; // TODO: implement via trkn clips
}

DeviceP
TrackImpl::access_device ()
{
  return chain_;
}

MonitorP
TrackImpl::create_monitor (int32 ochannel) // TODO: implement
{
  return nullptr;
}

TelemetryFieldS
TrackImpl::telemetry () const
{
#if 0  // TODO: implement telemtry from trkn for tracks
  MidiLib::MidiProducerIfaceP midi_prod = std::dynamic_pointer_cast<MidiLib::MidiProducerIface> (midi_prod_->_audio_processor());
  AudioChain::ProbeArray *probes = audio_chain->run_probes (true);
  TelemetryFieldS v;
  assert_return (midi_prod, v);
  const MidiLib::MidiProducerIface::Position *const position = midi_prod->position();
  v.push_back (telemetry_field ("current_clip", &position->current));
  v.push_back (telemetry_field ("current_tick", &position->tick));
  v.push_back (telemetry_field ("next_clip", &position->next));
  v.push_back (telemetry_field ("dbspl0", &(*probes)[0].dbspl));
  v.push_back (telemetry_field ("dbspl1", &(*probes)[1].dbspl));
  return v;
#endif
  return {};
}

DeviceInfo
TrackImpl::device_info ()
{
  return {}; // TODO: DeviceInfo
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
  if (previous >= 0 && previous < indices_.size())
    {
      last_ = previous;
      return indices_[last_];
    }
  return NONE;
}

/// Reset state (history), preserves succession order.
void
TrackImpl::ClipScout::reset ()
{
  last_ = -1;
}

/// Assign new succession order, preserves history.
void
TrackImpl::ClipScout::update (const ClipScout &other)
{
  indices_ = other.indices_;
}

ClipImplP
TrackImpl::create_midi_clip (const String &name, double start, double length)
{
  if (auto t = track_.get())
    {
       if (auto at = dynamic_cast<tracktion::AudioTrack*> (t))
       {
           const tracktion::TimeRange range (tracktion::TimePosition::fromSeconds(start), tracktion::TimeDuration::fromSeconds(length));
           auto clip = at->insertMIDIClip (juce::String(name), range, nullptr);
           if (clip)
             return ClipImpl::from_trkn (*clip);
           else
             warning ("insertMIDIClip returned null");
       }
       else
         warning ("dynamic_cast<AudioTrack*> failed for track type: %s", trkn_track_type(*t).c_str());
    }
  else
    warning ("track_.get() returned null");
  return nullptr;
}

} // Ase
