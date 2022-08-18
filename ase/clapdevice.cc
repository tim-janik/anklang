// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clapdevice.hh"
#include "clapplugin.hh"
#include "project.hh"
#include "processor.hh"
#include "compress.hh"
#include "properties.hh"
#include "storage.hh"
#include "jsonipc/jsonipc.hh"
#include "path.hh"
#include "main.hh"
#include "serialize.hh"
#include "internal.hh"
#include "gtk2wrap.hh"
#include <dlfcn.h>

#define CDEBUG(...)     Ase::debug ("clap", __VA_ARGS__)
#define CDEBUG_ENABLED() Ase::debug_key_enabled ("clap")

namespace Ase {

// == ClapPropertyImpl ==
struct ClapPropertyImpl : public Property, public virtual EmittableImpl {
  ClapDeviceImplP device_;
  clap_id  param_id = CLAP_INVALID_ID; // uint32_t
  uint32_t flags = 0; // clap_param_info_flags
  String ident_, label_, module_;
  double min_value = NAN, max_value = NAN, default_value = NAN;
public:
  String   identifier     () override   { return ident_; }
  String   label          () override   { return label_; }
  String   nick           () override   { return property_guess_nick (label_); }
  String   unit           () override   { return ""; }
  String   hints          () override   { return ClapParamInfo::hints_from_param_info_flags (flags); }
  String   group          () override   { return module_; }
  String   blurb          () override   { return ""; }
  String   description    () override   { return ""; }
  double   get_min        () override   { return min_value; }
  double   get_max        () override   { return max_value; }
  double   get_step       () override   { return is_stepped() ? 1 : 0; }
  bool     is_numeric     () override   { return true; }
  bool     is_stepped     ()            { return strstr (hints().c_str(), ":stepped:"); }
  void     reset          () override   { set_value (default_value); }
  ClapPropertyImpl (ClapDeviceImplP device, const ClapParamInfo info) :
    device_ (device)
  {
    param_id = info.param_id;
    flags = info.flags;
    ident_ = info.ident;
    label_ = info.name;
    module_ = info.module;
    min_value = info.min_value;
    max_value = info.max_value;
    default_value = info.default_value;
  }
  ChoiceS
  choices () override
  {
    const double mi = get_min(), ma = get_max();
    const bool down = ma < mi;
    return_unless (is_stepped(), {});
    ChoiceS choices;
    if (ma - mi <= 100)
      for (double d = mi; down ? d > ma : d < ma; d += down ? -1 : +1) {
        const String ident = string_format ("%f", d);
        choices.push_back ({ ident, ident });
      }
    return choices;
  }
  double
  get_normalized () override
  {
    const double mi = get_min(), ma = get_max();
    const double value = get_value().as_double();
    return (value - mi) / (ma - mi);
  }
  bool
  set_normalized (double v) override
  {
    const double mi = get_min(), ma = get_max();
    const double value = v * (ma - mi) + mi;
    return set_value (value);
  }
  String
  get_text () override
  {
    String txt;
    device_->handle_->param_get_value (param_id, &txt);
    return txt;
  }
  bool
  set_text (String vstr) override
  {
    return device_->handle_->param_set_value (param_id, vstr);
  }
  Value
  get_value () override
  {
    return Value (device_->handle_->param_get_value (param_id));
  }
  bool
  set_value (const Value &val) override
  {
    return device_->handle_->param_set_value (param_id, val.as_double());
  }
};

// == ClapDeviceImpl ==
JSONIPC_INHERIT (ClapDeviceImpl, Device);

ClapDeviceImpl::ClapDeviceImpl (ClapPluginHandleP claphandle) :
  handle_ (claphandle)
{
  assert_return (claphandle != nullptr);
  paramschange_ = on_event ("params:change", [this] (const Event &event) {
    proc_params_change (event);
  });
  if (handle_)
    handle_->_set_parent (this);
}

ClapDeviceImpl::~ClapDeviceImpl()
{
  if (handle_)
    handle_->_set_parent (nullptr);
  if (handle_)
    handle_->destroy();
  handle_ = nullptr;
}

String
ClapDeviceImpl::get_device_path ()
{
  std::vector<String> nums;
  Device *parent = dynamic_cast<Device*> (this->_parent());
  for (Device *dev = this; parent; dev = parent, parent = dynamic_cast<Device*> (dev->_parent()))
    {
      ssize_t index = Aux::index_of (parent->list_devices(),
                                     [dev] (const DeviceP &e) { return dev == &*e; });
      if (index >= 0)
        nums.insert (nums.begin(), string_from_int (index));
    }
  String s = string_join ("d", nums);
  ProjectImpl *project = _project();
  Track *track = _track();
  if (project && track)
    s = string_format ("t%ud%s", project->track_index (*track), s);
  return s;
}

void
ClapDeviceImpl::serialize (WritNode &xs)
{
  GadgetImpl::serialize (xs);

  if (xs.in_save() && handle_) {
    // save state into blob, collect parameter updates
    String blobname = string_format ("clap-%s.bin", get_device_path());
    String blobfile = _project()->writer_file_name (blobname);
    ClapParamUpdateS param_updates;
    handle_->save_state (blobfile, param_updates); // blobfile is modifyable
    // add blob to project state
    if (Path::check (blobfile, "fr")) {
      blobname = Path::basename (blobfile);
      if (string_endswith (blobname, ".zst")) // strip .zst for automatic decompression
        blobname = blobname.substr (0, blobname.size() - 4);
      xs["state_blob"] & blobname;
      Error err = _project()->writer_add_file (blobfile);
      if (!!err)
        printerr ("%s: %s: %s\n", program_alias(), blobfile, ase_error_blurb (err));
    }
    // save parameter updates as simple value array
    if (!param_updates.empty())
      {
        ValueS values;
        for (const ClapParamUpdate &pu : param_updates) {
          Value val;
          val = pu.param_id;
          values.push_back (val);
          val = pu.value;
          values.push_back (val);
        }
        xs["param_values"] & values;
      }
  }
  if (xs.in_load() && handle_ && !handle_->activated()) {
    StreamReaderP blob;
    String blobname;
    // load blob and value array
    xs["state_blob"] & blobname;
    if (!blobname.empty())
      blob = stream_reader_zip_member (_project()->loader_archive(), blobname);
    ValueS load_values;
    xs["param_values"] & load_values;
    // reconstruct parameter updates
    ClapParamUpdateS param_updates;
    for (size_t i = 0; i + 1 < load_values.size(); i += 2)
      if (load_values[i]->is_numeric() && load_values[i+1]->is_numeric())
        {
          const ClapParamUpdate pu = {
            .steady_time = 0,
            .param_id = uint32_t (load_values[i]->as_int()),
            .flags = 0,
            .value = load_values[i+1]->as_double(),
          };
          param_updates.push_back (pu);
        }
    load_values.clear();
    load_values.reserve (0);
    // load blob and parameter updates state
    handle_->load_state (blob, param_updates);
  }
}

DeviceInfo
ClapDeviceImpl::device_info ()
{
  return clap_device_info (handle_->descriptor);
}

PropertyS
ClapDeviceImpl::access_properties ()
{
  // reuse existing properties
  ClapDeviceImplP selfp = shared_ptr_cast<ClapDeviceImpl> (this);
  PropertyS properties;
  for (const ClapParamInfo &pinfo : handle_->param_infos())
    {
      auto aseprop = handle_->param_get_property (pinfo.param_id);
      if (aseprop)
        properties.push_back (aseprop);
      else
        {
          auto prop = std::make_shared<ClapPropertyImpl> (selfp, pinfo);
          handle_->param_set_property (prop->param_id, prop);
          properties.push_back (prop);
        }
    }
  return properties;
}

void
ClapDeviceImpl::proc_params_change (const Event &event)
{
  if (handle_)
    handle_->params_changed();
}

void
ClapDeviceImpl::_set_parent (Gadget *parent)
{
  GadgetImpl::_set_parent (parent);
  ClapDeviceImplP selfp = shared_ptr_cast<ClapDeviceImpl> (this);
  // deactivate and destroy plugin
  if (!parent && handle_)
    {
      handle_->destroy_gui();
      handle_->deactivate();
      handle_->destroy();
    }
}

void
ClapDeviceImpl::_activate ()
{
  if (_parent() && handle_)
    handle_->activate();
}

void
ClapDeviceImpl::gui_toggle ()
{
  if (handle_->gui_visible())
    handle_->hide_gui();
  else if (handle_->supports_gui())
    handle_->show_gui();
}

bool
ClapDeviceImpl::gui_supported ()
{
  return handle_->supports_gui();
}

bool
ClapDeviceImpl::gui_visible ()
{
  return handle_->gui_visible();
}

DeviceInfoS
ClapDeviceImpl::list_clap_plugins ()
{
  static DeviceInfoS devs;
  if (devs.size())
    return devs;
  for (ClapPluginDescriptor *descriptor : ClapPluginDescriptor::collect_descriptors()) {
    std::string title = descriptor->name; // FIXME
    if (!descriptor->version.empty())
      title = title + " " + descriptor->version;
    if (!descriptor->vendor.empty())
      title = title + " - " + descriptor->vendor;
    devs.push_back (clap_device_info (*descriptor));
  }
  return devs;
}

AudioProcessorP
ClapDeviceImpl::_audio_processor () const
{
  return handle_->audio_processor();
}

void
ClapDeviceImpl::_set_event_source (AudioProcessorP esource)
{
  // FIXME: implement
}

void
ClapDeviceImpl::_disconnect_remove ()
{
  // FIXME: implement
}

String
ClapDeviceImpl::clap_version ()
{
  const String clapversion = string_format ("%u.%u.%u", CLAP_VERSION.major, CLAP_VERSION.minor, CLAP_VERSION.revision);
  return clapversion;
}

ClapPluginHandleP
ClapDeviceImpl::access_clap_handle (DeviceP device)
{
  ClapDeviceImpl *clapdevice = dynamic_cast<ClapDeviceImpl*> (&*device);
  return clapdevice ? clapdevice->handle_ : nullptr;
}

DeviceP
ClapDeviceImpl::create_clap_device (AudioEngine &engine, const String &clapuri)
{
  assert_return (string_startswith (clapuri, "CLAP:"), nullptr);
  const String clapid = clapuri.substr (5);
  ClapPluginDescriptor *descriptor = nullptr;
  for (ClapPluginDescriptor *desc : ClapPluginDescriptor::collect_descriptors())
    if (clapid == desc->id) {
      descriptor = desc;
      break;
    }
  if (descriptor)
    {
      auto make_clap_device = [&descriptor] (const String &aseid, AudioProcessor::StaticInfo static_info, AudioProcessorP aproc) -> DeviceP {
        ClapPluginHandleP handlep = ClapPluginHandle::make_clap_handle (*descriptor, aproc);
        return ClapDeviceImpl::make_shared (handlep);
      };
      DeviceP devicep = AudioProcessor::registry_create (ClapPluginHandle::audio_processor_type(), engine, make_clap_device);
      ClapDeviceImplP clapdevicep = shared_ptr_cast<ClapDeviceImpl> (devicep);
      return clapdevicep;
    }
  return nullptr;
}

} // Ase
