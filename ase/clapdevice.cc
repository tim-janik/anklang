// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clapdevice.hh"
#include "jsonipc/jsonipc.hh"
#include <clap/clap.h>
#include <dlfcn.h>
#include <glob.h>
#include "internal.hh"

#define CDEBUG(...)     Ase::debug ("Clap", __VA_ARGS__)

namespace Ase {

static std::vector<std::string>
list_clap_files ()
{
  std::vector<std::string> files;
  glob_t pglob = { 0, };
  int ret = glob ("~/.clap/*.clap", GLOB_TILDE_CHECK | GLOB_MARK | GLOB_NOSORT, nullptr, &pglob);
  if (ret == 0) {
    for (size_t i = 0; i < pglob.gl_pathc; i++) {
      files.push_back (pglob.gl_pathv[i]);
    }
    globfree (&pglob);
  }
  ret = glob ("/usr/lib/clap/*.clap", GLOB_TILDE_CHECK | GLOB_MARK | GLOB_NOSORT, nullptr, &pglob);
  if (ret == 0) {
    for (size_t i = 0; i < pglob.gl_pathc; i++) {
      files.push_back (pglob.gl_pathv[i]);
    }
    globfree (&pglob);
  }
  return files;
}

struct ClapPluginInfo {
  std::string dlclap, id, name, version, vendor, features;
  std::string description, url, manual_url, support_url;
};
using ClapPluginInfoS = std::vector<ClapPluginInfo>;

static void
add_clap_plugin_infos (const std::string &pluginpath, ClapPluginInfoS &infos)
{
  auto get_dlerror = [] () { const char *err = dlerror(); return err ? err : "unknown dlerror"; };
  void *handle = dlopen (pluginpath.c_str(), RTLD_LOCAL | RTLD_NOW);
  if (!handle) {
    CDEBUG ("dlopen failed: %s: %s", pluginpath, get_dlerror());
    return;
  }
  const clap_plugin_entry *pluginentry = (const clap_plugin_entry*) dlsym (handle, "clap_entry");
  if (pluginentry && clap_version_is_compatible (pluginentry->clap_version)) {
    const bool initialized = pluginentry->init (pluginpath.c_str());
    if (initialized) {
      const clap_plugin_factory *pluginfactory = (const clap_plugin_factory *) pluginentry->get_factory (CLAP_PLUGIN_FACTORY_ID);
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
        ClapPluginInfo cinfo;
        cinfo.dlclap = pluginpath;
        cinfo.id = pdesc->id;
        cinfo.name = pdesc->name ? pdesc->name : pdesc->id;
        cinfo.version = pdesc->version ? pdesc->version : "0.0.0-unknown";
        cinfo.vendor = pdesc->vendor ? pdesc->vendor : "";
        cinfo.url = pdesc->url ? pdesc->url : "";
        cinfo.manual_url = pdesc->manual_url ? pdesc->manual_url : "";
        cinfo.support_url = pdesc->support_url ? pdesc->support_url : "";
        cinfo.description = pdesc->description ? pdesc->description : "";
        StringS features;
        if (pdesc->features)
          for (size_t ft = 0; pdesc->features[ft]; ft++)
            if (pdesc->features[ft][0])
              features.push_back (pdesc->features[ft]);
        cinfo.features = ":" + string_join (":", features) + ":";
        infos.push_back (cinfo);
        CDEBUG ("Plugin: %s %s %s (%s, %s)%s", cinfo.name, cinfo.version,
                cinfo.vendor.empty() ? "" : "- " + cinfo.vendor,
                cinfo.id, clapversion,
                cinfo.features.empty() ? "" : ": " + cinfo.features);
      }
    } else
      CDEBUG ("init() failed: %s", pluginpath);
    pluginentry->deinit();
  } else
    CDEBUG ("invalid clap_entry: %s", !pluginentry ? "NULL" :
            string_format ("clap-%u.%u.%u", pluginentry->clap_version.major, pluginentry->clap_version.minor,
                           pluginentry->clap_version.revision));
  if (dlclose (handle) != 0)
    CDEBUG ("dlclose failed: %s: %s", pluginpath, get_dlerror());
}

// == ClapDeviceImpl ==
JSONIPC_INHERIT (ClapDeviceImpl, Device);

ClapDeviceImpl::ClapDeviceImpl ()
{}

ClapDeviceImpl::~ClapDeviceImpl()
{}

DeviceInfoS
ClapDeviceImpl::list_clap_plugins ()
{
  ClapPluginInfoS plugininfos;
  for (const auto &clapfile : list_clap_files())
    add_clap_plugin_infos (clapfile, plugininfos);
  DeviceInfoS devs;
  for (const ClapPluginInfo &cinfo : plugininfos) {
    std::string title = cinfo.name;
    if (!cinfo.version.empty())
      title = title + " " + cinfo.version;
    if (!cinfo.vendor.empty())
      title = title + " - " + cinfo.vendor;
    printout ("%s\n", title);
    DeviceInfo di;
    di.uri = "clapid:" + cinfo.id;
    di.name = cinfo.name;
    di.description = cinfo.description;
    di.website_url = cinfo.url;
    di.creator_name = cinfo.vendor;
    di.creator_url = cinfo.manual_url;
    const char *const cfeatures = cinfo.features.c_str();
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

bool
ClapDeviceImpl::is_combo_device ()
{
  return false;
}

DeviceS
ClapDeviceImpl::list_devices ()
{
  return {};
}

DeviceInfoS
ClapDeviceImpl::list_device_types ()
{
  return {}; // FIXME: empty?
}

void
ClapDeviceImpl::remove_device (Device &sub)
{
}

DeviceP
ClapDeviceImpl::create_device (const String &uuiduri)
{
  return {};
}

DeviceP
ClapDeviceImpl::create_device_before (const String &uuiduri, Device &sibling)
{
  return {};
}

} // Ase
