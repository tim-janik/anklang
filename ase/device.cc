// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "device.hh"
#include "combo.hh"
#include "main.hh"
#include "jsonipc/jsonipc.hh"
#include "internal.hh"

namespace Ase {

// == DeviceImpl ==
JSONIPC_INHERIT (DeviceImpl, Device);

DeviceImpl::DeviceImpl (AudioProcessor &proc) :
  proc_ (proc.shared_from_this()), combo_ (std::dynamic_pointer_cast<AudioCombo> (proc_))
{}

DeviceImpl::~DeviceImpl()
{}

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

StringS
DeviceImpl::list_properties ()
{
  std::vector<const AudioProcessor::PParam*> pparams;
  pparams.reserve (proc_->params_.size());
  for (const AudioProcessor::PParam &p : proc_->params_)
    pparams.push_back (&p);
  std::sort (pparams.begin(), pparams.end(), [] (auto a, auto b) { return a->info->order < b->info->order; });
  StringS names;
  names.reserve (pparams.size());
  for (const AudioProcessor::PParam *p : pparams)
    names.push_back (p->info->ident);
  return names;
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
  auto job = [&devs, combo] () {
    for (auto &proc : combo->list_processors())
      devs.push_back (proc->device_impl());
  };
  proc_->engine_.const_jobs += job;
  return devs;
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
      AudioProcessorP subp = make_audio_processor (proc_->engine(), uuiduri);
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
DeviceImpl::disconnect_remove ()
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
  AudioProcessorP procp = make_audio_processor (*engine, uuiduri);
  return_unless (procp, nullptr);
  DeviceP devicep = procp->get_device();
  auto j = [procp] () {
    procp->enable_engine_output (true);
  };
  engine->async_jobs += j;
  return devicep;
}

} // Ase
