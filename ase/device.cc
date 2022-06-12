// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "device.hh"
#include "combo.hh"
#include "jsonipc/jsonipc.hh"
#include "serialize.hh"
#include "internal.hh"

namespace Ase {

// == DeviceImpl ==
JSONIPC_INHERIT (DeviceImpl, Device);

DeviceImpl::DeviceImpl (const String &aseid, AudioProcessor::StaticInfo static_info, AudioProcessorP aproc) :
  proc_ (aproc), combo_ (std::dynamic_pointer_cast<AudioCombo> (proc_))
{
  assert_return (aproc != nullptr);
  AudioProcessorInfo pinfo;
  static_info (pinfo);
  info.uri          = aseid;
  info.name         = pinfo.label;
  info.category     = pinfo.category;
  info.description  = pinfo.description;
  info.website_url  = pinfo.website_url;
  info.creator_name = pinfo.creator_name;
  info.creator_url  = pinfo.creator_url;
}

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
        DeviceImplP subdevicep = shared_ptr_cast<DeviceImpl> (append_device (uuiduri));
        xc & *subdevicep;
      }
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
      devs.push_back (proc->get_device());
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
  AudioProcessor::registry_foreach ([&iseq] (const String &aseid, AudioProcessor::StaticInfo static_info) {
    AudioProcessorInfo pinfo;
    static_info (pinfo);
    DeviceInfo info;
    info.uri          = aseid;
    info.name         = pinfo.label;
    info.category     = pinfo.category;
    info.description  = pinfo.description;
    info.website_url  = pinfo.website_url;
    info.creator_name = pinfo.creator_name;
    info.creator_url  = pinfo.creator_url;
    if (!info.name.empty() && !info.category.empty())
      iseq.push_back (info);
  });
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
DeviceImpl::insert_device (const String &uri, Device *sibling)
{
  DeviceP devicep;
  DeviceImpl *siblingi = dynamic_cast<DeviceImpl*> (sibling);
  AudioProcessorP siblingp = siblingi ? siblingi->proc_ : nullptr;
  if (combo_)
    {
      devicep = create_processor_device (proc_->engine(), uri, false);
      return_unless (devicep, nullptr);
      AudioProcessorP subp = devicep->_audio_processor();
      return_unless (subp, nullptr);
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
DeviceImpl::append_device (const String &uri)
{
  return insert_device (uri, nullptr);
}

DeviceP
DeviceImpl::insert_device (const String &uri, Device &sibling)
{
  return insert_device (uri, &sibling);
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
DeviceImpl::create_ase_device (AudioEngine &engine, const String &aseid)
{
  auto make_device = [] (const String &aseid, AudioProcessor::StaticInfo static_info, AudioProcessorP aproc) -> DeviceP {
    return DeviceImpl::make_shared (aseid, static_info, aproc);
  };
  DeviceP devicep = AudioProcessor::registry_create (aseid, engine, make_device);
  return_unless (devicep && devicep->_audio_processor(), nullptr);
  return devicep;
}

DeviceP
create_processor_device (AudioEngine &engine, const String &uri, bool engineproducer)
{
  DeviceP devicep;
  // assume string_startswith (uri, "Ase:")
  devicep = DeviceImpl::create_ase_device (engine, uri);
  return_unless (devicep, nullptr);
  AudioProcessorP procp = devicep->_audio_processor();
  if (procp) {
    auto j = [procp,engineproducer] () {
      procp->enable_engine_output (engineproducer);
    };
    engine.async_jobs += j;
  }
  return devicep;
}

} // Ase
