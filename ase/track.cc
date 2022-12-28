// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
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

namespace Ase {

// == TrackImpl ==
JSONIPC_INHERIT (TrackImpl, Track);

TrackImpl::TrackImpl (ProjectImpl &project, bool masterflag)
{
  gadget_flags (masterflag ? MASTER_TRACK : 0);
}

TrackImpl::~TrackImpl()
{
  assert_return (_parent() == nullptr);
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
      ClipS clips = list_launcher_clips(); // forces creation
      for (auto &xc : xs["clips"].to_nodes())
        {
          int64 index = xc["clip-index"].as_int();
          if (index < 0 || size_t (index) >= clips.size())
            continue;
          ClipImplP clip = shared_ptr_cast<ClipImpl> (clips[index]);
          xc & *clip;
        }
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
      AudioEngine *engine = main_config.engine;
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

int32
TrackImpl::midi_channel () const
{
  return 0;
}

void
TrackImpl::midi_channel (int32 midichannel) // TODO: implement
{}

static constexpr const uint MAX_LAUNCHER_CLIPS = 8;

ClipS
TrackImpl::list_launcher_clips ()
{
  const uint max_clips = MAX_LAUNCHER_CLIPS;
  if (clips_.size() < max_clips)
    {
      clips_.reserve (max_clips);
      while (clips_.size() < max_clips)
        clips_.push_back (ClipImpl::make_shared (*this));
      update_clips();
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
  size_t index = clip_index (clip);
  return_unless (index >= 0, NONE);
  // advance clip
  index += 1;
  if (index >= clips_.size())
    index = 0;
  return clips_[index] ? index : NONE;
}

void
TrackImpl::update_clips ()
{
  return_unless (midi_prod_);
  MidiLib::MidiProducerIfaceP midi_iface = std::dynamic_pointer_cast<MidiLib::MidiProducerIface> (midi_prod_->_audio_processor());
  MidiLib::MidiFeedP feedp = std::make_shared<MidiLib::MidiFeed>();
  MidiLib::MidiFeed &feed = *feedp;
  feed.generators.resize (clips_.size());
  for (size_t i = 0; i < clips_.size(); i++)
    feed.generators[i].setup (*clips_[i]);
  std::vector<int> scout_indices (clips_.size());
  for (size_t i = 0; i < clips_.size(); i++)
    scout_indices[i] = clips_[i] ? clip_succession (*clips_[i]) : NONE;
  feed.scout.setup (scout_indices);
  auto job = [midi_iface, feedp] () mutable {
    midi_iface->update_feed (feedp);
    // swap MidiFeedP copy to defer dtor to user thread
  };
  midi_iface->engine().async_jobs += job;
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
  MidiLib::MidiProducerIfaceP midi_prod = std::dynamic_pointer_cast<MidiLib::MidiProducerIface> (midi_prod_->_audio_processor());
  TelemetryFieldS v;
  assert_return (midi_prod, v);
  const MidiLib::MidiProducerIface::Position *const position = midi_prod->position();
  v.push_back (telemetry_field ("current_clip", &position->current));
  v.push_back (telemetry_field ("current_tick", &position->tick));
  v.push_back (telemetry_field ("next_clip", &position->next));
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

} // Ase
