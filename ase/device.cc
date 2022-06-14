// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "device.hh"
#include "combo.hh"
#include "main.hh"
#include "jsonipc/jsonipc.hh"
#include "serialize.hh"
#include "internal.hh"

namespace Ase {

// == DeviceImpl ==
JSONIPC_INHERIT (DeviceImpl, Device);

DeviceImpl::DeviceImpl (AudioProcessor &proc) :
  proc_ (proc.shared_from_this()), combo_ (std::dynamic_pointer_cast<AudioCombo> (proc_))
{}

DeviceImpl::~DeviceImpl()
{}

void
DeviceImpl::serialize (WritNode &xs)
{
  GadgetImpl::serialize (xs);
  // save subdevices
  if (combo_ && xs.in_save())
    for (auto &subdev : list_devices())
      {
        DeviceImplP subdevicep = shared_ptr_cast<DeviceImpl> (subdev);
        DeviceInfo info = subdevicep->device_info();
        WritNode xc = xs["devices"].push();
        xc & *subdevicep;
        xc.front ("Device.URI") & info.uri;
      }
  // load subdevices
  if (combo_ && xs.in_load())
    for (auto &xc : xs["devices"].to_nodes())
      {
        String uuiduri = xc["Device.URI"].as_string();
        if (uuiduri.empty())
          continue;
        DeviceImplP subdevicep = shared_ptr_cast<DeviceImpl> (create_device (uuiduri));
        xc & *subdevicep;
      }
}

DeviceInfo
DeviceImpl::device_info ()
{
  AudioProcessorInfo pinf;
  proc_->query_info (pinf);
  DeviceInfo info;
  info.uri          = pinf.uri;
  info.name         = pinf.label;
  info.category     = pinf.category;
  info.description  = pinf.description;
  info.website_url  = pinf.website_url;
  info.creator_name = pinf.creator_name;
  info.creator_url  = pinf.creator_url;
  return info;
}

PropertyS
DeviceImpl::access_properties ()
{
  std::vector<const AudioProcessor::PParam*> pparams;
  pparams.reserve (proc_->params_.size());
  for (const AudioProcessor::PParam &p : proc_->params_)
    pparams.push_back (&p);
  std::sort (pparams.begin(), pparams.end(), [] (auto a, auto b) { return a->info->order < b->info->order; });
  PropertyS pseq;
  pseq.reserve (pparams.size());
  for (const AudioProcessor::PParam *p : pparams)
    pseq.push_back (proc_->access_property (p->id));
  return pseq;
}

PropertyP
DeviceImpl::access_property (String ident)
{
  for (const AudioProcessor::PParam &p : proc_->params_)
    if (p.info->ident == ident)
      return proc_->access_property (p.id);
  return {};
}

DeviceS
DeviceImpl::list_devices ()
{
  DeviceS devs;
  AudioComboP combo = combo_;
  auto j = [&devs, combo] () {
    for (auto &proc : combo->list_processors())
      devs.push_back (proc->device_impl());
  };
  proc_->engine().const_jobs += j;
  return devs;
}

void
DeviceImpl::_set_event_source (AudioProcessorP esource)
{
  if (esource)
    assert_return (esource->has_event_output());
  AudioComboP combo = combo_;
  return_unless (combo);
  auto j = [combo, esource] () {
    combo->set_event_source (esource);
  };
  proc_->engine().async_jobs += j;
}

DeviceInfoS
DeviceImpl::list_device_types ()
{
  DeviceInfoS iseq;
  const auto rlist = AudioProcessor::registry_list();
  iseq.reserve (rlist.size());
  for (const AudioProcessorInfo &entry : rlist)
    {
      DeviceInfo info;
      info.uri          = entry.uri;
      info.name         = entry.label;
      info.category     = entry.category;
      info.description  = entry.description;
      info.website_url  = entry.website_url;
      info.creator_name = entry.creator_name;
      info.creator_url  = entry.creator_url;
      iseq.push_back (info);
    }
  return iseq;
}

void
DeviceImpl::remove_device (Device &sub)
{
  DeviceImpl *subi = dynamic_cast<DeviceImpl*> (&sub);
  AudioProcessorP subp = subi ? subi->proc_ : nullptr;
  if (subp && combo_)
    {
      AudioComboP combo = combo_;
      auto j = [combo, subp] () {
        combo->remove (*subp);
      };
      proc_->engine().async_jobs += j;
    }
  // blocking on an async_job for returning `true` would take fairly long
}

DeviceP
DeviceImpl::create_device_before (const String &uuiduri, Device *sibling)
{
  DeviceP devicep;
  DeviceImpl *siblingi = dynamic_cast<DeviceImpl*> (sibling);
  AudioProcessorP siblingp = siblingi ? siblingi->proc_ : nullptr;
  if (combo_)
    {
      AudioProcessorP subp = proc_->engine().create_audio_processor (uuiduri);
      return_unless (subp, nullptr);
      devicep = subp->get_device();
      AudioComboP combo = combo_;
      auto j = [combo, subp, siblingp] () {
        const size_t pos = siblingp ? combo->find_pos (*siblingp) : ~size_t (0);
        combo->insert (subp, pos);
      };
      proc_->engine().async_jobs += j;
    }
  return devicep;
}

DeviceP
DeviceImpl::create_device (const String &uuiduri)
{
  return create_device_before (uuiduri, nullptr);
}

DeviceP
DeviceImpl::create_device_before (const String &uuiduri, Device &sibling)
{
  return create_device_before (uuiduri, &sibling);
}

void
DeviceImpl::_disconnect_remove ()
{
  AudioProcessorP proc = proc_;
  AudioEngine *engine = &proc->engine();
  auto j = [proc] () {
    proc->enable_engine_output (false);
    proc->disconnect_ibuses();
    proc->disconnect_obuses();
    proc->disconnect_event_input();
    // FIXME: remove from combo container if child
  };
  engine->async_jobs += j;
}

DeviceP
DeviceImpl::create_output (const String &uuiduri)
{
  AudioEngine *engine = main_config.engine;
  AudioProcessorP procp = engine->create_audio_processor (uuiduri);
  return_unless (procp, nullptr);
  DeviceP devicep = procp->get_device();
  auto j = [procp] () {
    procp->enable_engine_output (true);
  };
  engine->async_jobs += j;
  return devicep;
}

} // Ase
