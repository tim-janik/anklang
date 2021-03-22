// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "device.hh"
#include "combo.hh"
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

} // Ase
