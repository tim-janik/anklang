// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first

#include "track.hh"
#include "combo.hh"
#include "project.hh"
#include "nativedevice.hh"
#include "clip.hh"
#include "midilib.hh"
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
  DeviceImpl::serialize (xs);
  // save clips
  if (xs.in_save())
    for (auto &bclip : clips_)
      {
        ClipImplP clip = shared_ptr_cast<ClipImpl> (bclip);
        if (!clip->needs_serialize())
          continue;
        WritNode xc = xs["clips"].push();
        xc & *clip;
        const int64 index = clip_index (*clip);
        xc.front ("clip-index") << index;
      }
  // load clips
  if (xs.in_load())
    {
      ClipS clips = launcher_clips(); // forces creation
      for (auto &xc : xs["clips"].to_nodes())
        {
          int64 index = xc["clip-index"].as_int();
          if (index < 0 || size_t (index) >= clips.size())
            continue;
          ClipImplP clip = shared_ptr_cast<ClipImpl> (clips[index]);
          xc & *clip;
        }
      emit_notify ("launcher_clips");
    }
  // device chain
  xs["chain"] & *dynamic_cast<Serializable*> (&*chain_); // always exists
}

void
TrackImpl::_set_parent (GadgetImpl *parent)
{
  auto project = dynamic_cast<ProjectImpl*> (parent);
  assert_return (!!parent == !!project);
  DeviceImpl::_set_parent (project);
  if (project)
    {
      AudioEngine *engine = App.engine;
      assert_return (!midi_prod_);
      midi_prod_ = create_processor_device (*engine, "Ase::MidiLib::MidiProducerImpl", true);
      assert_return (midi_prod_);
      midi_prod_->_set_parent (this);
      AudioProcessorP esource = midi_prod_->_audio_processor()->engine().get_event_source();
      midi_prod_->_set_event_source (esource);
      midi_prod_->_audio_processor()->connect_event_input (*esource);
      assert_return (!chain_);
      chain_ = create_processor_device (*engine, "Ase::AudioChain", true);
      assert_return (chain_);
      chain_->_set_parent (this);
      chain_->_set_event_source (midi_prod_->_audio_processor());
    }
  else if (chain_)
    {
      midi_prod_->_disconnect_remove();
      chain_->_disconnect_remove();
      chain_->_set_parent (nullptr);
      chain_ = nullptr;
      midi_prod_->_set_parent (nullptr);
      midi_prod_ = nullptr;
    }
  emit_notify ("project");
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
TrackImpl::queue_cmd (CallbackS &queue, Cmd cmd, double arg)
{
  assert_return (midi_prod_);
  MidiLib::MidiProducerIfaceP midi_iface = std::dynamic_pointer_cast<MidiLib::MidiProducerIface> (midi_prod_->_audio_processor());
  auto func = [midi_iface, cmd, arg] () {
    if (cmd == START)
      midi_iface->start();
    else if (cmd == STOP)
      midi_iface->stop (arg);
  };
  queue.push_back (func);
}

void
TrackImpl::queue_cmd (DCallbackS &queue, Cmd cmd)
{
  assert_return (midi_prod_);
  MidiLib::MidiProducerIfaceP midi_iface = std::dynamic_pointer_cast<MidiLib::MidiProducerIface> (midi_prod_->_audio_processor());
  auto func = [midi_iface, cmd] (const double arg) {
    if (cmd == START)
      midi_iface->start();
    else if (cmd == STOP)
      midi_iface->stop (arg);
  };
  queue.push_back (func);
}

void
TrackImpl::midi_channel (int32 midichannel) // TODO: implement
{
  midichannel = CLAMP (midichannel, 0, 16);
  return_unless (midichannel != midi_channel_);
  midi_channel_ = midichannel;
  emit_notify ("midi_channel");
}

static constexpr const uint MAX_LAUNCHER_CLIPS = 8;

ClipS
TrackImpl::launcher_clips ()
{
  const uint max_clips = MAX_LAUNCHER_CLIPS;
  if (clips_.size() < max_clips)
    {
      clips_.reserve (max_clips);
      while (clips_.size() < max_clips)
        clips_.push_back (ClipImpl::make_shared (*this));
      // update_clips();
    }
  return Aux::container_copy<ClipS> (clips_);
}

ssize_t
TrackImpl::clip_index (const ClipImpl &clip) const
{
  for (size_t i = 0; i < clips_.size(); i++)
    if (clips_[i].get() == &clip)
      return i;
  return -1;
}

int
TrackImpl::clip_succession (const ClipImpl &clip) const
{
  ssize_t index = clip_index (clip);
  return_unless (index >= 0, NONE);
  // advance clip
  index += 1;
  if (index >= clips_.size())
    index = 0;
  return clips_[index] ? index : NONE;
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
  return {}; // TODO: implement telemtry from trkn for tracks
  MidiLib::MidiProducerIfaceP midi_prod = std::dynamic_pointer_cast<MidiLib::MidiProducerIface> (midi_prod_->_audio_processor());
  Ase::AudioChain *audio_chain = dynamic_cast<Ase::AudioChain*> (&*chain_->_audio_processor());
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
}

DeviceInfo
TrackImpl::device_info ()
{
  return {}; // TODO: DeviceInfo
}

AudioProcessorP
TrackImpl::_audio_processor () const
{
  return {}; // TODO: AudioProcessorP
}

void
TrackImpl::_set_event_source (AudioProcessorP esource)
{
  // TODO: _set_event_source
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
