// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "track.hh"
#include "combo.hh"
#include "project.hh"
#include "device.hh"
#include "serialize.hh"
#include "jsonipc/jsonipc.hh"
#include "internal.hh"

namespace Ase {

// == TrackImpl ==
JSONIPC_INHERIT (TrackImpl, Track);

TrackImpl::TrackImpl (bool masterflag) :
  masterflag_ (masterflag)
{}

TrackImpl::~TrackImpl()
{
  set_project (nullptr);
}

String
TrackImpl::fallback_name () const
{
  if (masterflag_)
    return "Master";
  if (project_)
    {
      ssize_t i = project_->track_index (*this);
      return string_format ("Track %u", i >= 0 ? i + 1 : i);
    }
  return GadgetImpl::fallback_name();
}

void
TrackImpl::serialize (WritNode &xs)
{
  GadgetImpl::serialize (xs);
  // device chain
  xs["chain"] & *chain_; // always one present
}

void
TrackImpl::set_project (ProjectImpl *project)
{
  if (project)
    assert_return (!project_);
  ProjectImpl *old = project_;
  (void) old;
  project_ = project;
  if (project_)
    {
      assert_return (!chain_);
      chain_ = DeviceImpl::create_output ("Anklang.Devices.AudioChain");
      assert_return (chain_);
      AudioProcessorP esource = chain_->audio_processor()->engine().get_event_source();
      chain_->set_event_source (esource);
    }
  else if (chain_)
    {
      chain_->disconnect_remove();
      chain_ = nullptr;
    }
  emit_event ("notify", "project");
}

int32
TrackImpl::midi_channel () const
{
  return 0;
}

void
TrackImpl::midi_channel (int32 midichannel) // TODO: implement
{}

ClipS
TrackImpl::list_clips () // TODO: implement
{
  return {};
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

} // Ase
