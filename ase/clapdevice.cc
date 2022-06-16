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

static const char*
anklang_host_name()
{
  static String name = "Anklang//" + executable_name();
  return name.c_str();
}

// == CLAP file locations ==
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

// == CLAP file handle ==
class ClapFileHandle {
  void *dlhandle_ = nullptr;
  uint  open_count_ = 0;
public:
  const std::string dlfile;
  const clap_plugin_entry *pluginentry = nullptr;
  ~ClapFileHandle() { assert_return (!opened()); }
  explicit ClapFileHandle (const String &pathname) :
    dlfile (pathname)
  {}
  static String
  get_dlerror ()
  {
    const char *err = dlerror();
    return err ? err : "unknown dlerror";
  };
  template<typename Ptr> Ptr
  symbol (const char *symname) const
  {
    void *p = dlhandle_ ? dlsym (dlhandle_, symname) : nullptr;
    return (Ptr) p;
  }
  void
  close()
  {
    assert_return (open_count_ > 0);
    open_count_--;
    return_unless (open_count_ == 0);
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
  void
  open()
  {
    if (open_count_++ == 0 && !dlhandle_) {
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
          CDEBUG ("unusable clap_entry: %s", !pluginentry ? "NULL" :
                  string_format ("clap-%u.%u.%u", pluginentry->clap_version.major, pluginentry->clap_version.minor,
                                 pluginentry->clap_version.revision));
          pluginentry = nullptr;
          dlclose (dlhandle_);
          dlhandle_ = nullptr;
        }
      } else
        CDEBUG ("dlopen failed: %s: %s", dlfile, get_dlerror());
    }
  }
  bool opened() const { return dlhandle_ && pluginentry; }
};

// == CLAP plugin descriptor ==
class ClapDeviceImpl::PluginDescriptor {
  ClapFileHandle &clapfile_;
public:
  explicit PluginDescriptor (ClapFileHandle &clapfile) :
    clapfile_ (clapfile)
  {}
  std::string id, name, version, vendor, features;
  std::string description, url, manual_url, support_url;
  void open()   { clapfile_.open(); }
  void close()  { clapfile_.close(); }
  const clap_plugin_entry *
  entry() {
    return clapfile_.opened() ? clapfile_.pluginentry : nullptr;
  }
  using Collection = std::vector<PluginDescriptor*>;
  static void              add_descriptor      (const std::string &pluginpath, Collection &infos);
  static const Collection& collect_descriptors ();
};

void
ClapDeviceImpl::PluginDescriptor::add_descriptor (const std::string &pluginpath, Collection &infos)
{
  ClapFileHandle *filehandle = new ClapFileHandle (pluginpath);
  filehandle->open();
  if (!filehandle->opened()) {
    filehandle->close();
    delete filehandle;
    return;
  }
  const clap_plugin_factory *pluginfactory = (const clap_plugin_factory *) filehandle->pluginentry->get_factory (CLAP_PLUGIN_FACTORY_ID);
  const uint32_t plugincount = !pluginfactory ? 0 : pluginfactory->get_plugin_count (pluginfactory);
  for (size_t i = 0; i < plugincount; i++)
    {
      const clap_plugin_descriptor_t *pdesc = pluginfactory->get_plugin_descriptor (pluginfactory, i);
      if (!pdesc || !pdesc->id || !pdesc->id[0])
        continue;
      const std::string clapversion = string_format ("clap-%u.%u.%u", pdesc->clap_version.major, pdesc->clap_version.minor, pdesc->clap_version.revision);
      if (!clap_version_is_compatible (pdesc->clap_version)) {
        CDEBUG ("invalid plugin: %s (%s)", pdesc->id, clapversion);
        continue;
      }
      PluginDescriptor *descriptor = new PluginDescriptor (*filehandle);
      descriptor->id = pdesc->id;
      descriptor->name = pdesc->name ? pdesc->name : pdesc->id;
      descriptor->version = pdesc->version ? pdesc->version : "0.0.0-unknown";
      descriptor->vendor = pdesc->vendor ? pdesc->vendor : "";
      descriptor->url = pdesc->url ? pdesc->url : "";
      descriptor->manual_url = pdesc->manual_url ? pdesc->manual_url : "";
      descriptor->support_url = pdesc->support_url ? pdesc->support_url : "";
      descriptor->description = pdesc->description ? pdesc->description : "";
      StringS features;
      if (pdesc->features)
        for (size_t ft = 0; pdesc->features[ft]; ft++)
          if (pdesc->features[ft][0])
            features.push_back (feature_canonify (pdesc->features[ft]));
      descriptor->features = ":" + string_join (":", features) + ":";
      infos.push_back (descriptor);
      CDEBUG ("Plugin: %s %s %s (%s, %s)%s", descriptor->name, descriptor->version,
              descriptor->vendor.empty() ? "" : "- " + descriptor->vendor,
              descriptor->id, clapversion,
              descriptor->features.empty() ? "" : ": " + descriptor->features);
    }
  filehandle->close();
}

const ClapDeviceImpl::PluginDescriptor::Collection&
ClapDeviceImpl::PluginDescriptor::collect_descriptors ()
{
  static Collection collection;
  if (collection.empty()) {
    for (const auto &clapfile : list_clap_files())
      add_descriptor (clapfile, collection);
  }
  return collection;
}

// == CLAP plugin handle ==
class ClapDeviceImpl::PluginHandle {
  const clap_plugin_t *plugin_ = nullptr;
  String clapid_;
  std::atomic<uint> request_restart_, request_process_, request_callback_;
  clap_host_t phost = {
    .clap_version = CLAP_VERSION,
    .name = anklang_host_name(), .vendor = "anklang.testbit.eu",
    .url = "https://anklang.testbit.eu/", .version = ase_version(),
    .get_extension = [] (const struct clap_host *host, const char *extension_id) {
      return ((PluginHandle*) host->host_data)->get_extension (extension_id);
    },
    .request_restart = [] (const struct clap_host *host) {
      ((PluginHandle*) host->host_data)->plugin_request (1);
    },  // deactivate() + activate()
    .request_process = [] (const struct clap_host *host) {
      ((PluginHandle*) host->host_data)->plugin_request (2);
    },  // process()
    .request_callback = [] (const struct clap_host *host) {
      ((PluginHandle*) host->host_data)->plugin_request (3);
    },  // on_main_thread()
  };
  const void*
  get_extension (const char *extension_id)
  {
    const String ext = extension_id;
    else return nullptr;
  }
  void
  plugin_request (int what)
  {
    if (what == 1)      // deactivate() + activate()
      request_restart_++;
    else if (what == 2) // process()
      request_process_++;
    else if (what == 3) // on_main_thread()
      request_callback_++;
  }
public:
  PluginHandle (const clap_plugin_factory *factory, const String &clapid) :
    clapid_ (clapid)
  {
    phost.host_data = (PluginHandle*) this;
    if (factory)
      plugin_ = factory->create_plugin (factory, &phost, clapid_.c_str());
    if (plugin_ && !plugin_->init (plugin_)) {
      plugin_->destroy (plugin_);
      plugin_ = nullptr;
    }
  }
  ~PluginHandle()
  {
    destroy();
  }
  void
  destroy()
  {
    if (plugin_)
      plugin_->destroy (plugin_);
    plugin_ = nullptr;
  }
};

// == ClapDeviceImpl ==
JSONIPC_INHERIT (ClapDeviceImpl, Device);

ClapDeviceImpl::ClapDeviceImpl (const String &clapid, AudioProcessorP aproc) :
  proc_ (aproc)
{
  for (PluginDescriptor *descriptor : PluginDescriptor::collect_descriptors())
    if (clapid == descriptor->id) {
      descriptor_ = descriptor;
      break;
    }
  if (descriptor_)
    descriptor_->open();
  const clap_plugin_entry *pluginentry = !descriptor_ ? nullptr : descriptor_->entry();
  const clap_plugin_factory *pluginfactory = !pluginentry ? nullptr :
                                             (const clap_plugin_factory *) pluginentry->get_factory (CLAP_PLUGIN_FACTORY_ID);
  handle_ = new PluginHandle (pluginfactory, clapid);
}

ClapDeviceImpl::~ClapDeviceImpl()
{
  if (handle_) {
    delete handle_;
    handle_ = nullptr;
  }
  if (descriptor_)
    descriptor_->close();
}

DeviceInfoS
ClapDeviceImpl::list_clap_plugins ()
{
  static DeviceInfoS devs;
  if (devs.size())
    return devs;
  for (PluginDescriptor *descriptor : PluginDescriptor::collect_descriptors()) {
    std::string title = descriptor->name;
    if (!descriptor->version.empty())
      title = title + " " + descriptor->version;
    if (!descriptor->vendor.empty())
      title = title + " - " + descriptor->vendor;
    DeviceInfo di;
    di.uri = "CLAP:" + descriptor->id;
    di.name = descriptor->name;
    di.description = descriptor->description;
    di.website_url = descriptor->url;
    di.creator_name = descriptor->vendor;
    di.creator_url = descriptor->manual_url;
    const char *const cfeatures = descriptor->features.c_str();
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
