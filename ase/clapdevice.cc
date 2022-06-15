// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clapdevice.hh"
#include "jsonipc/jsonipc.hh"
#include "processor.hh"
#include "path.hh"
#include <clap/clap.h>
#include <dlfcn.h>
#include <glob.h>
#include "internal.hh"

#define CDEBUG(...)     Ase::debug ("Clap", __VA_ARGS__)

namespace Ase {

// == helpers ==
static String
feature_canonify (const String &str)
{
  return string_canonify (str, string_set_a2z() + string_set_A2Z() + "-0123456789", "-");
}

static std::vector<std::string>
list_clap_files ()
{
  std::vector<std::string> files;
  Path::rglob ("~/.clap", "*.clap", files);
  Path::rglob ("/usr/lib/clap", "*.clap", files);
  const char *clapsearchpath = getenv ("CLAP_PATH");
  if (clapsearchpath)
    for (const String &spath : Path::searchpath_split (clapsearchpath))
      Path::rglob (spath, "*.clap", files);
  Path::unique_realpaths (files);
  return files;
}

class ClapPluginHandle {
  void *dlhandle_ = nullptr;
  uint  opened_ = 0;
public:
  explicit ClapPluginHandle (std::string dlfilename) :
    dlfile (dlfilename)
  {}
  ~ClapPluginHandle() {
    assert_return (opened_ == 0);
  }
  const std::string dlfile;
  std::string id, name, version, vendor, features;
  std::string description, url, manual_url, support_url;
  const clap_plugin_entry *pluginentry = nullptr;
  bool opened() const { return dlhandle_ && pluginentry; }
  void open();
  void close();
  template<typename Ptr> Ptr symbol (const char *symname);
};
using ClapPluginHandleS = std::vector<ClapPluginHandle*>;

static auto get_dlerror = [] () { const char *err = dlerror(); return err ? err : "unknown dlerror"; };

template<typename Ptr> Ptr
ClapPluginHandle::symbol (const char *symname)
{
  void *p = dlhandle_ ? dlsym (dlhandle_, symname) : nullptr;
  return (Ptr) p;
}

void
ClapPluginHandle::open()
{
  if (!dlhandle_) {
    dlhandle_ = dlopen (dlfile.c_str(), RTLD_LOCAL | RTLD_NOW);
    if (dlhandle_) {
      pluginentry = symbol<const clap_plugin_entry*> ("clap_entry");
      bool initialized = false;
      if (pluginentry && clap_version_is_compatible (pluginentry->clap_version)) {
        initialized = pluginentry->init (dlfile.c_str());
        if (!initialized)
          pluginentry->deinit();
      }
      if (!initialized) {
        CDEBUG ("invalid clap_entry: %s", !pluginentry ? "NULL" :
                string_format ("clap-%u.%u.%u", pluginentry->clap_version.major, pluginentry->clap_version.minor,
                               pluginentry->clap_version.revision));
        pluginentry = nullptr;
        dlclose (dlhandle_);
        dlhandle_ = nullptr;
      }
    } else
      CDEBUG ("dlopen failed: %s: %s", dlfile, get_dlerror());
  }
  if (pluginentry)
    opened_++;
}

void
ClapPluginHandle::close()
{
  return_unless (opened_ > 0);
  opened_--;
  return_unless (opened_ == 0);
  if (pluginentry) {
    pluginentry->deinit();
    pluginentry = nullptr;
  }
  if (dlhandle_) {
    if (dlclose (dlhandle_) != 0)
      CDEBUG ("dlclose failed: %s: %s", dlfile, get_dlerror());
    dlhandle_ = nullptr;
  }
}

static void
add_clap_plugin_handle (const std::string &pluginpath, ClapPluginHandleS &infos)
{
  ClapPluginHandle scanhandle = ClapPluginHandle (pluginpath);
  scanhandle.open();
  if (scanhandle.opened()) {
    const clap_plugin_factory *pluginfactory = (const clap_plugin_factory *) scanhandle.pluginentry->get_factory (CLAP_PLUGIN_FACTORY_ID);
    const uint32_t plugincount = !pluginfactory ? 0 : pluginfactory->get_plugin_count (pluginfactory);
    for (size_t i = 0; i < plugincount; i++) {
      const clap_plugin_descriptor_t *pdesc = pluginfactory->get_plugin_descriptor (pluginfactory, i);
      if (!pdesc || !pdesc->id || !pdesc->id[0])
        continue;
      const std::string clapversion = string_format ("clap-%u.%u.%u", pdesc->clap_version.major, pdesc->clap_version.minor, pdesc->clap_version.revision);
      if (!clap_version_is_compatible (pdesc->clap_version)) {
        CDEBUG ("invalid plugin: %s (%s)", pdesc->id, clapversion);
        continue;
      }
      ClapPluginHandle *handle = new ClapPluginHandle (pluginpath);
      handle->id = pdesc->id;
      handle->name = pdesc->name ? pdesc->name : pdesc->id;
      handle->version = pdesc->version ? pdesc->version : "0.0.0-unknown";
      handle->vendor = pdesc->vendor ? pdesc->vendor : "";
      handle->url = pdesc->url ? pdesc->url : "";
      handle->manual_url = pdesc->manual_url ? pdesc->manual_url : "";
      handle->support_url = pdesc->support_url ? pdesc->support_url : "";
      handle->description = pdesc->description ? pdesc->description : "";
      StringS features;
      if (pdesc->features)
        for (size_t ft = 0; pdesc->features[ft]; ft++)
          if (pdesc->features[ft][0])
            features.push_back (feature_canonify (pdesc->features[ft]));
      handle->features = ":" + string_join (":", features) + ":";
      infos.push_back (handle);
      CDEBUG ("Plugin: %s %s %s (%s, %s)%s", handle->name, handle->version,
              handle->vendor.empty() ? "" : "- " + handle->vendor,
              handle->id, clapversion,
              handle->features.empty() ? "" : ": " + handle->features);
    }
  }
  scanhandle.close();
}

static const ClapPluginHandleS&
collect_clap_plugin_handles ()
{
  static ClapPluginHandleS handle_list;
  if (handle_list.empty()) {
    for (const auto &clapfile : list_clap_files())
      add_clap_plugin_handle (clapfile, handle_list);
  }
  return handle_list;
}

// == ClapDeviceImpl ==
JSONIPC_INHERIT (ClapDeviceImpl, Device);

ClapDeviceImpl::ClapDeviceImpl (const String &clapid, AudioProcessorP aproc) :
  proc_ (aproc)
{
  for (ClapPluginHandle *handle : collect_clap_plugin_handles())
    if (clapid == handle->id) {
      handle_ = handle;
      break;
    }
  if (handle_)
    handle_->open();
  if (handle_ && handle_->opened()) {
    const clap_plugin_factory *pluginfactory = (const clap_plugin_factory *) handle_->pluginentry->get_factory (CLAP_PLUGIN_FACTORY_ID);
    clap_host_t phost = { .clap_version = CLAP_VERSION,
                          .name = "Anklang/ase/" __FILE__,
                          .vendor = "Anklang", .url = "https://anklang.testbit.eu/",
                          .version = __DATE__,
    };
    const clap_plugin_t *plugin = pluginfactory->create_plugin (pluginfactory, &phost, clapid.c_str());
    if (plugin && !plugin->init (plugin)) {
      plugin->destroy (plugin);
      plugin = nullptr;
    }
    if (plugin)
      plugin->destroy (plugin);
  } else
    handle_ = nullptr;
}

ClapDeviceImpl::~ClapDeviceImpl()
{
  if (handle_)
    handle_->close();
}

DeviceInfoS
ClapDeviceImpl::list_clap_plugins ()
{
  static DeviceInfoS devs;
  if (devs.size())
    return devs;
  for (const ClapPluginHandle *handle : collect_clap_plugin_handles()) {
    std::string title = handle->name;
    if (!handle->version.empty())
      title = title + " " + handle->version;
    if (!handle->vendor.empty())
      title = title + " - " + handle->vendor;
    DeviceInfo di;
    di.uri = "CLAP:" + handle->id;
    di.name = handle->name;
    di.description = handle->description;
    di.website_url = handle->url;
    di.creator_name = handle->vendor;
    di.creator_url = handle->manual_url;
    const char *const cfeatures = handle->features.c_str();
    if (strstr (cfeatures, ":instrument:"))        // CLAP_PLUGIN_FEATURE_INSTRUMENT
      di.category = "Instrument";
    else if (strstr (cfeatures, ":analyzer:"))     // CLAP_PLUGIN_FEATURE_ANALYZER
      di.category = "Analyzer";
    else if (strstr (cfeatures, ":note-effect:"))  // CLAP_PLUGIN_FEATURE_NOTE_EFFECT
      di.category = "Note FX";
    else if (strstr (cfeatures, ":audio-effect:")) // CLAP_PLUGIN_FEATURE_AUDIO_EFFECT
      di.category = "Audio FX";
    else if (strstr (cfeatures, ":effect:"))       // CLAP_PLUGIN_FEATURE_AUDIO_EFFECT
      di.category = "Audio FX";
    else
      di.category = "Clap Device";
    devs.push_back (di);
  }
  return devs;
}

DeviceInfo
ClapDeviceImpl::device_info ()
{
  return {};
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

// == ClapWrapper ==
class ClapWrapper : public AudioProcessor {
public:
  ClapWrapper (AudioEngine &engine) :
    AudioProcessor (engine)
  {}
  static void
  static_info (AudioProcessorInfo &info)
  {
    info.label = "Anklang.Devices.ClapWrapper";
  }
  void
  reset (uint64 target_stamp) override
  {}
  void
  initialize (SpeakerArrangement busses) override
  {
    remove_all_buses();
    auto input = add_input_bus ("Input", busses);
    auto output = add_output_bus ("Output", busses);
  }
  void
  render (uint n_frames) override
  {
    const IBusId i1 = IBusId (1);
    const OBusId o1 = OBusId (1);
    const uint ni = this->n_ichannels (i1);
    const uint no = this->n_ochannels (o1);
    assert_return (ni == no);
    for (size_t i = 0; i < ni; i++)
      redirect_oblock (o1, i, ifloats (i1, i));
  }
};
static auto clap_wrapper_id = register_audio_processor<ClapWrapper>();

DeviceP
ClapDeviceImpl::create_clap_device (AudioEngine &engine, const String &clapid)
{
  assert_return (string_startswith (clapid, "CLAP:"), nullptr);
  auto make_device = [&clapid] (const String &aseid, AudioProcessor::StaticInfo static_info, AudioProcessorP aproc) -> DeviceP {
    return ClapDeviceImpl::make_shared (clapid.substr (5), aproc);
  };
  DeviceP devicep = AudioProcessor::registry_create (clap_wrapper_id, engine, make_device);
  assert_return (devicep && devicep->_audio_processor(), nullptr);
  return devicep;
}

} // Ase
