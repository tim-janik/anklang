// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clapdevice.hh"
#include "jsonipc/jsonipc.hh"
#include "processor.hh"
#include "path.hh"
#include "main.hh"
#include "internal.hh"
#include "gtk2wrap.hh"
#include <clap/clap.h>
#include <dlfcn.h>
#include <glob.h>

#define CDEBUG(...)     Ase::debug ("Clap", __VA_ARGS__)
#define CDEBUG_ENABLED() Ase::debug_key_enabled ("Clap")

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

static float scratch_float_buffer[AUDIO_BLOCK_FLOAT_ZEROS_SIZE];

static const char*
clap_event_type_string (int etype)
{
  switch (etype)
    {
    case CLAP_EVENT_NOTE_ON:                    return "NOTE_ON";
    case CLAP_EVENT_NOTE_OFF:                   return "NOTE_OFF";
    case CLAP_EVENT_NOTE_CHOKE:                 return "NOTE_CHOKE";
    case CLAP_EVENT_NOTE_END:                   return "NOTE_END";
    case CLAP_EVENT_NOTE_EXPRESSION:            return "NOTE_EXPRESSION";
    case CLAP_EVENT_PARAM_VALUE:                return "PARAM_VALUE";
    case CLAP_EVENT_PARAM_MOD:                  return "PARAM_MOD";
    case CLAP_EVENT_PARAM_GESTURE_BEGIN:        return "PARAM_GESTURE_BEGIN";
    case CLAP_EVENT_PARAM_GESTURE_END:          return "PARAM_GESTURE_END";
    case CLAP_EVENT_TRANSPORT:                  return "TRANSPORT";
    case CLAP_EVENT_MIDI:                       return "MIDI";
    case CLAP_EVENT_MIDI_SYSEX:                 return "MIDI_SYSEX";
    case CLAP_EVENT_MIDI2:                      return "MIDI2";
    default:                                    return "<UNKNOWN>";
    }
}

static ASE_UNUSED String
clap_event_to_string (const clap_event_note_t *enote)
{
  const char *et = clap_event_type_string (enote->header.type);
  switch (enote->header.type)
    {
    case CLAP_EVENT_NOTE_ON:
    case CLAP_EVENT_NOTE_OFF:
    case CLAP_EVENT_NOTE_CHOKE:
    case CLAP_EVENT_NOTE_END:
      return string_format ("%+4d ch=%-2u %-14s pitch=%d vel=%f id=%x sz=%d spc=%d flags=%x port=%d",
                            enote->header.time, enote->channel, et, enote->key, enote->velocity, enote->note_id,
                            enote->header.size, enote->header.space_id, enote->header.flags, enote->port_index);
    default:
      return string_format ("%+4d %-20s sz=%d spc=%d flags=%x port=%d",
                            enote->header.time, et, enote->header.size, enote->header.space_id, enote->header.flags);
    }
}

// == Gtk2DlWrapEntry ==
static Gtk2DlWrapEntry*
x11wrapper()
{
  static Gtk2DlWrapEntry *gtk2wrapentry = [] () {
    String gtk2wrapso = anklang_runpath (RPath::LIBDIR, "gtk2wrap.so");
    void *gtklhandle_ = dlopen (gtk2wrapso.c_str(), RTLD_LOCAL | RTLD_NOW);
    const char *symname = "Ase__Gtk2__wrapentry";
    void *ptr = gtklhandle_ ? dlsym (gtklhandle_, symname) : nullptr;
    return (Gtk2DlWrapEntry*) ptr;
  } ();
  assert_return (gtk2wrapentry != nullptr, nullptr);
  return gtk2wrapentry;
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
class ClapDeviceImpl::PluginHandle : public std::enable_shared_from_this<PluginHandle> {
  const clap_plugin_t *plugin_ = nullptr;
  String clapid_;
  bool activated_ = false;
  clap_host_t phost = {
    .clap_version = CLAP_VERSION,
    .host_data = (PluginHandle*) this,
    .name = anklang_host_name(), .vendor = "anklang.testbit.eu",
    .url = "https://anklang.testbit.eu/", .version = ase_version(),
    .get_extension = [] (const struct clap_host *host, const char *extension_id) {
      const void *ext = ((PluginHandle*) host->host_data)->get_host_extension (extension_id);
      CDEBUG ("%s: host.get_host_extension(\"%s\"): %p", ((PluginHandle*) host->host_data)->debug_name(), extension_id, ext);
      return ext;
    },
    .request_restart = [] (const struct clap_host *host) {
      CDEBUG ("%s: host.request_restart", ((PluginHandle*) host->host_data)->debug_name());
    },  // deactivate() + activate()
    .request_process = [] (const struct clap_host *host) {
      CDEBUG ("%s: host.request_process", ((PluginHandle*) host->host_data)->debug_name());
    },  // process()
    .request_callback = [] (const struct clap_host *host) {
      CDEBUG ("%s: host.request_callback", ((PluginHandle*) host->host_data)->debug_name());
      ((PluginHandle*) host->host_data)->plugin_request_callback_mt();
    },  // on_main_thread()
  };
  clap_host_log host_log_ = {
    .log = [] (const clap_host_t *host, clap_log_severity severity, const char *msg) {
      ((PluginHandle*) host->host_data)->log (severity, msg);
    },
  };
  clap_host_audio_ports host_audio_ports_ = {
    .is_rescan_flag_supported = [] (const clap_host_t *host, uint32_t flag) {
      CDEBUG ("%s: host.audio_ports.is_rescan_flag_supported: false", ((PluginHandle*) host->host_data)->debug_name());
      return false;
    },
    .rescan = [] (const clap_host_t *host, uint32_t flags) {
      CDEBUG ("%s: host.audio_ports.rescan(0x%x)", ((PluginHandle*) host->host_data)->debug_name(), flags);
    },
  };
  clap_host_timer_support host_timer_support = {
    .register_timer = [] (const clap_host_t *host, uint32_t period_ms, clap_id *timer_id) {
      return ((PluginHandle*) host->host_data)->register_timer (period_ms, timer_id);
    },
    .unregister_timer = [] (const clap_host_t *host, clap_id timer_id) {
      return ((PluginHandle*) host->host_data)->unregister_timer (timer_id);
    }
  };
  clap_host_gui host_gui = {
    .resize_hints_changed = [] (const clap_host_t *host) {
      CDEBUG ("%s: host.resize_hints_changed", ((PluginHandle*) host->host_data)->debug_name());
    },
    .request_resize = [] (const clap_host_t *host, uint32_t width, uint32_t height) {
      CDEBUG ("%s: host.request_resize (%d, %d)", ((PluginHandle*) host->host_data)->debug_name(), width, height);
      return ((PluginHandle*) host->host_data)->resize_window (width, height);
    },
    .request_show = [] (const clap_host_t *host) {
      CDEBUG ("%s: host.request_show", ((PluginHandle*) host->host_data)->debug_name());
      return false;
    },
    .request_hide = [] (const clap_host_t *host) {
      CDEBUG ("%s: host.request_hide", ((PluginHandle*) host->host_data)->debug_name());
      return false;
    },
    .closed = [] (const clap_host_t *host, bool was_destroyed) {
      CDEBUG ("%s: host.closed(destroyed=%d)", ((PluginHandle*) host->host_data)->debug_name(), was_destroyed);
      ((PluginHandle*) host->host_data)->plugin_gui_closed (was_destroyed);
    },
  };
  clap_host_thread_check host_thread_check = {
    .is_main_thread = [] (const clap_host_t *host) {
      const bool b = this_thread_is_ase();
      //CDEBUG ("%s: host.is_main_thread: %d", ((PluginHandle*) host->host_data)->debug_name(), b);
      return b;
    },
    .is_audio_thread = [] (const clap_host_t *host) {
      const bool b = AudioEngine::thread_is_engine();
      //CDEBUG ("%s: host.is_audio_thread: %d", ((PluginHandle*) host->host_data)->debug_name(), b);
      return b;
    },
  };
  clap_host_params host_params = {
    .rescan = [] (const clap_host_t *host, clap_param_rescan_flags flags) {
      CDEBUG ("%s: host.params.rescan(0x%x)", ((PluginHandle*) host->host_data)->debug_name(), flags);
    },
    .clear = [] (const clap_host_t *host, clap_id param_id, clap_param_clear_flags flags) {
      CDEBUG ("%s: host.params.clear(0x%x)", ((PluginHandle*) host->host_data)->debug_name(), flags);
    },
    .request_flush = [] (const clap_host_t *host) {
      CDEBUG ("%s: host.params.request_flush", ((PluginHandle*) host->host_data)->debug_name());
      ((PluginHandle*) host->host_data)->plugin_request_flush_mt();
    },
  };
  const void*
  get_host_extension (const char *extension_id)
  {
    const String ext = extension_id;
    if (ext == CLAP_EXT_LOG)            return &host_log_;
    if (ext == CLAP_EXT_AUDIO_PORTS)    return &host_audio_ports_;
    if (ext == CLAP_EXT_GUI)            return &host_gui;
    if (ext == CLAP_EXT_TIMER_SUPPORT)  return &host_timer_support;
    if (ext == CLAP_EXT_THREAD_CHECK)   return &host_thread_check;
    if (ext == CLAP_EXT_PARAMS)         return &host_params;
    else return nullptr;
  }
  void log (clap_log_severity severity, const char *msg);
  void
  plugin_request_callback_mt()
  {
    std::shared_ptr<PluginHandle> selfp = shared_ptr_cast<PluginHandle> (this);
    main_loop->exec_callback ([selfp] () {
      if (selfp->plugin_) {
        // gui_threads_enter();
        selfp->plugin_->on_main_thread (selfp->plugin_);
        // gui_threads_leave();
      }
    });
  }
  void
  plugin_request_flush_mt()
  {
    // FIXME: flush plugin_params
  }
  std::vector<uint> timers;
  bool
  plugin_on_timer (clap_id timer_id)
  {
    // gui_threads_enter();
    if (plugin_timer_support) // register_timer() cannot catch this
      plugin_timer_support->on_timer (plugin_, timer_id);
    // gui_threads_leave();
    return true; // keep-alive
  }
  bool
  register_timer (uint32_t period_ms, clap_id *timer_id)
  {
    // Avoid premature check for plugin_timer_support, JUCE requests timers in ->init()
    // *before* plugin_->get_extension (plugin_, CLAP_EXT_TIMER_SUPPORT); can be queried
    // if (!plugin_timer_support) return false;
    period_ms = MAX (30, period_ms);
    auto timeridp = std::make_shared<uint> (0);
    *timeridp = main_loop->exec_timer ([this, timeridp] () {
      return plugin_on_timer (*timeridp);
    }, period_ms, period_ms, EventLoop::PRIORITY_UPDATE);
    *timer_id = *timeridp;
    timers.push_back (*timeridp);
    CDEBUG ("%s: %s: ms=%u: id=%u", debug_name(), __func__, period_ms, timer_id);
    return true;
  }
  bool
  unregister_timer (clap_id timer_id)
  {
    // NOTE: plugin_ might be destroying here
    const bool deleted = Aux::erase_first (timers, [timer_id] (uint id) { return id == timer_id; });
    if (deleted)
      main_loop->remove (timer_id);
    CDEBUG ("%s: %s: deleted=%u: id=%u", debug_name(), __func__, deleted, timer_id);
    return deleted;
  }
  const clap_plugin_gui *plugin_gui = nullptr;
  const clap_plugin_audio_ports_config *plugin_audio_ports_config = nullptr;
  const clap_plugin_audio_ports *plugin_audio_ports = nullptr;
  const clap_plugin_note_ports *plugin_note_ports = nullptr;
  const clap_plugin_timer_support *plugin_timer_support = nullptr;
  const clap_plugin_params *plugin_params = nullptr;
public:
  const clap_input_events_t plugin_input_events = {
    .ctx = (PluginHandle*) this,
    .size = [] (const struct clap_input_events *evlist) -> uint32_t {
      return ((PluginHandle*) evlist->ctx)->input_events.size();
    },
    .get = [] (const struct clap_input_events *evlist, uint32_t index) -> const clap_event_header_t* {
      return &((PluginHandle*) evlist->ctx)->input_events.at (index).header;
    },
  };
  const clap_output_events_t plugin_output_events = {
    .ctx = (PluginHandle*) this,
    .try_push = [] (const struct clap_output_events *evlist, const clap_event_header_t *event) -> bool {
      PluginHandle *self = (PluginHandle*) evlist->ctx;
      if (0)
        CDEBUG ("%s: host.try_push(type=%x): false", self->debug_name(), event->type);
      return false;
    },
  };
  union EventUnion {
    clap_event_header_t          header;        // size, time, space_id, type, flags
    clap_event_note_t            note;          // CLAP_NOTE_DIALECT_CLAP
    clap_event_note_expression_t expression;    // CLAP_NOTE_DIALECT_CLAP
    clap_event_param_value_t     value;
    clap_event_param_mod_t       mod;
    clap_event_param_gesture_t   gesture;
    clap_event_midi_t            midi1;         // CLAP_NOTE_DIALECT_MIDI
    clap_event_midi_sysex_t      sysex;         // CLAP_NOTE_DIALECT_MIDI
    clap_event_midi2_t           midi2;         // CLAP_NOTE_DIALECT_MIDI2
  };
  std::vector<EventUnion> input_events;
  std::vector<clap_event_header_t> output_events;
  std::vector<clap_audio_ports_config_t> audio_ports_configs;
  std::vector<clap_audio_port_info_t> audio_iport_infos, audio_oport_infos;
  std::vector<clap_note_port_info_t> note_iport_infos, note_oport_infos;
  std::vector<clap_audio_buffer_t> audio_inputs, audio_outputs;
  std::vector<float*> data32ptrs;
  void
  get_port_infos()
  {
    assert_return (!activated());
    uint total_channels = 0;
    // audio_ports_configs
    audio_ports_configs.resize (!plugin_audio_ports_config ? 0 : plugin_audio_ports_config->count (plugin_));
    for (size_t i = 0; i < audio_ports_configs.size(); i++)
      if (!plugin_audio_ports_config->get (plugin_, i, &audio_ports_configs[i]))
        audio_ports_configs[i] = { CLAP_INVALID_ID, { 0, }, 0, 0, 0, 0, "", 0, 0, "" };
    if (audio_ports_configs.size()) { // not encountered yet
      String s = string_format ("audio_configs:%u:", audio_ports_configs.size());
      for (size_t i = 0; i < audio_ports_configs.size(); i++)
        if (audio_ports_configs[i].id != CLAP_INVALID_ID)
          s += string_format (" %u:%s:iports=%u:oports=%u:imain=%u,%s:omain=%u,%s",
                              audio_ports_configs[i].id,
                              audio_ports_configs[i].name,
                              audio_ports_configs[i].input_port_count,
                              audio_ports_configs[i].output_port_count,
                              audio_ports_configs[i].has_main_input * audio_ports_configs[i].main_input_channel_count,
                              audio_ports_configs[i].main_input_port_type,
                              audio_ports_configs[i].has_main_output * audio_ports_configs[i].main_output_channel_count,
                              audio_ports_configs[i].main_output_port_type);
      CDEBUG ("%s: %s", debug_name(), s);
    }
    // note_iport_infos
    note_iport_infos.resize (!plugin_note_ports ? 0 : plugin_note_ports->count (plugin_, true));
    for (size_t i = 0; i < note_iport_infos.size(); i++)
      if (!plugin_note_ports->get (plugin_, i, true, &note_iport_infos[i]))
        note_iport_infos[i] = { CLAP_INVALID_ID, 0, 0, { 0, }, };
    if (note_iport_infos.size()) {
      String s = string_format ("note_iports=%u:", note_iport_infos.size());
      for (size_t i = 0; i < note_iport_infos.size(); i++)
        if (note_iport_infos[i].id != CLAP_INVALID_ID)
          s += string_format (" %u:%s:can=%x:want=%x",
                              note_iport_infos[i].id,
                              note_iport_infos[i].name,
                              note_iport_infos[i].supported_dialects,
                              note_iport_infos[i].preferred_dialect);
      CDEBUG ("%s: %s", debug_name(), s);
    }
    // note_oport_infos
    note_oport_infos.resize (!plugin_note_ports ? 0 : plugin_note_ports->count (plugin_, false));
    for (size_t i = 0; i < note_oport_infos.size(); i++)
      if (!plugin_note_ports->get (plugin_, i, false, &note_oport_infos[i]))
        note_oport_infos[i] = { CLAP_INVALID_ID, 0, 0, { 0, }, };
    if (note_oport_infos.size()) {
      String s = string_format ("note_oports=%u:", note_oport_infos.size());
      for (size_t i = 0; i < note_oport_infos.size(); i++)
        if (note_oport_infos[i].id != CLAP_INVALID_ID)
          s += string_format (" %u:%s:can=%x:want=%x",
                              note_oport_infos[i].id,
                              note_oport_infos[i].name,
                              note_oport_infos[i].supported_dialects,
                              note_oport_infos[i].preferred_dialect);
      CDEBUG ("%s: %s", debug_name(), s);
    }
    // audio_iport_infos
    audio_iport_infos.resize (!plugin_audio_ports ? 0 : plugin_audio_ports->count (plugin_, true));
    for (size_t i = 0; i < audio_iport_infos.size(); i++)
      if (!plugin_audio_ports->get (plugin_, i, true, &audio_iport_infos[i]))
        audio_iport_infos[i] = { CLAP_INVALID_ID, { 0 }, 0, 0, "", CLAP_INVALID_ID };
      else
        total_channels += audio_iport_infos[i].channel_count;
    if (audio_iport_infos.size()) {
      String s = string_format ("audio_iports=%u:", audio_iport_infos.size());
      for (size_t i = 0; i < audio_iport_infos.size(); i++)
        if (audio_iport_infos[i].id != CLAP_INVALID_ID && audio_iport_infos[i].port_type)
          s += string_format (" %u:ch=%u:%s:m=%u:%s:",
                              audio_iport_infos[i].id,
                              audio_iport_infos[i].channel_count,
                              audio_iport_infos[i].name,
                              audio_iport_infos[i].flags & CLAP_AUDIO_PORT_IS_MAIN,
                              audio_iport_infos[i].port_type);
      CDEBUG ("%s: %s", debug_name(), s);
    }
    // audio_oport_infos
    audio_oport_infos.resize (!plugin_audio_ports ? 0 : plugin_audio_ports->count (plugin_, false));
    for (size_t i = 0; i < audio_oport_infos.size(); i++)
      if (!plugin_audio_ports->get (plugin_, i, false, &audio_oport_infos[i]))
        audio_oport_infos[i] = { CLAP_INVALID_ID, { 0 }, 0, 0, "", CLAP_INVALID_ID };
      else
        total_channels += audio_oport_infos[i].channel_count;
    if (audio_oport_infos.size()) {
      String s = string_format ("audio_oports=%u:", audio_oport_infos.size());
      for (size_t i = 0; i < audio_oport_infos.size(); i++)
        if (audio_oport_infos[i].id != CLAP_INVALID_ID && audio_oport_infos[i].port_type)
          s += string_format (" %u:ch=%u:%s:m=%u:%s:",
                              audio_oport_infos[i].id,
                              audio_oport_infos[i].channel_count,
                              audio_oport_infos[i].name,
                              audio_oport_infos[i].flags & CLAP_AUDIO_PORT_IS_MAIN,
                              audio_oport_infos[i].port_type);
      CDEBUG ("%s: %s", debug_name(), s);
    }
    // allocate .data32 pointer arrays for all input/output port channels
    data32ptrs.resize (total_channels);
    // audio_inputs
    audio_inputs.resize (audio_iport_infos.size());
    for (size_t i = 0; i < audio_inputs.size(); i++) {
      audio_inputs[i] = { nullptr, nullptr, 0, 0, 0 };
      if (audio_iport_infos[i].id == CLAP_INVALID_ID) continue;
      audio_inputs[i].channel_count = audio_iport_infos[i].channel_count;
      total_channels -= audio_inputs[i].channel_count;
      audio_inputs[i].data32 = &data32ptrs[total_channels];
      for (size_t j = 0; j < audio_inputs[i].channel_count; j++)
        audio_inputs[i].data32[j] = const_cast<float*> (const_float_zeros);
    }
    // audio_outputs
    audio_outputs.resize (audio_oport_infos.size());
    for (size_t i = 0; i < audio_outputs.size(); i++) {
      audio_outputs[i] = { nullptr, nullptr, 0, 0, 0 };
      if (audio_oport_infos[i].id == CLAP_INVALID_ID) continue;
      audio_outputs[i].channel_count = audio_oport_infos[i].channel_count;
      total_channels -= audio_outputs[i].channel_count;
      audio_outputs[i].data32 = &data32ptrs[total_channels];
      for (size_t j = 0; j < audio_outputs[i].channel_count; j++)
        audio_outputs[i].data32[j] = scratch_float_buffer;
    }
    assert_return (total_channels == 0);
    // input_events
    input_events.resize (0);
    // output_events
    output_events.resize (0);
  }
  PluginHandle (const clap_plugin_factory *factory, const String &clapid) :
    clapid_ (clapid)
  {
    if (factory)
      plugin_ = factory->create_plugin (factory, &phost, clapid_.c_str());
    if (plugin_ && !plugin_->init (plugin_)) {
      CDEBUG ("%s: initialization failed", debug_name());
      destroy(); // destroy per spec and cleanup resources used by init()
    }
    if (plugin_) {
      CDEBUG ("%s: initialized", debug_name());
      plugin_audio_ports_config = (const clap_plugin_audio_ports_config*) plugin_->get_extension (plugin_, CLAP_EXT_AUDIO_PORTS_CONFIG);
      plugin_audio_ports = (const clap_plugin_audio_ports*) plugin_->get_extension (plugin_, CLAP_EXT_AUDIO_PORTS);
      plugin_note_ports = (const clap_plugin_note_ports*) plugin_->get_extension (plugin_, CLAP_EXT_NOTE_PORTS);
      plugin_gui = (const clap_plugin_gui*) plugin_->get_extension (plugin_, CLAP_EXT_GUI);
      plugin_timer_support = (const clap_plugin_timer_support*) plugin_->get_extension (plugin_, CLAP_EXT_TIMER_SUPPORT);
      plugin_params = (const clap_plugin_params*) plugin_->get_extension (plugin_, CLAP_EXT_PARAMS);
    }
  }
  ~PluginHandle()
  {
    destroy();
  }
  String debug_name() const { return clapid_; }
  void
  destroy()
  {
    destroy_gui();
    if (plugin_ && activated())
      deactivate();
    if (plugin_)
      CDEBUG ("%s: destroying", debug_name());
    while (timers.size())
      host_timer_support.unregister_timer (&phost, timers.back());
    if (plugin_)
      plugin_->destroy (plugin_);
    plugin_ = nullptr;
  }
  bool activated() const { return activated_; }
  bool
  activate (AudioEngine &engine)
  {
    return_unless (plugin_, false);
    assert_return (!activated(), true);
    activated_ = plugin_->activate (plugin_, engine.sample_rate(), 32, 4096);
    CDEBUG ("%s: %s", debug_name(), activated_ ? "activated" : "failed to activate");
    return activated();
  }
  void
  deactivate()
  {
    return_unless (plugin_);
    assert_return (activated());
    activated_ = false;
    plugin_->deactivate (plugin_);
    CDEBUG ("%s: deactivated", debug_name());
  }
  bool
  start_processing()
  {
    return plugin_ && plugin_->process && plugin_->start_processing && plugin_->start_processing (plugin_);
  }
  clap_process_status
  process (const clap_process_t *clapprocess)
  {
    return plugin_->process (plugin_, clapprocess);
  }
  void
  stop_processing()
  {
    return_unless (plugin_);
    if (plugin_->stop_processing)
      plugin_->stop_processing (plugin_);
  }
  ulong gui_windowid = 0;
  bool gui_visible = false;
  void
  plugin_gui_closed (bool was_destroyed)
  {
    CDEBUG ("%s: %s: was_destroyed=%d", debug_name(), __func__);
    gui_visible = false;
    if (was_destroyed && plugin_gui) {
      plugin_gui->destroy (plugin_);
      gui_windowid = 0;
    }
  }
  void
  gui_delete_request()
  {
    CDEBUG ("%s: %s", debug_name(), __func__);
    destroy_gui();
  }
  ulong
  create_x11_window (int width, int height)
  {
    std::shared_ptr<PluginHandle> selfp = shared_ptr_cast<PluginHandle> (this);
    Gtk2WindowSetup wsetup {
      .title = debug_name(), .width = width, .height = height,
      .deleterequest_mt = [selfp] () { main_loop->exec_callback ([selfp]() { selfp->gui_delete_request(); }); },
    };
    const ulong windowid = x11wrapper()->create_window (wsetup);
    return windowid;
  }
  bool
  resize_window (int width, int height)
  {
    if (gui_windowid) {
      if (x11wrapper()->resize_window (gui_windowid, width, height)) {
        if (plugin_gui->can_resize (plugin_))
          plugin_gui->set_size (plugin_, width, height);
        return true;
      }
    }
    return false;
  }
  void
  show_gui()
  {
    if (!gui_windowid && plugin_gui) {
      const bool floating = false;
      clap_window_t cwindow = { .api = CLAP_WINDOW_API_X11, .x11 = 0 };
      if (plugin_gui->is_api_supported (plugin_, cwindow.api, floating)) {
        bool created = plugin_gui->create (plugin_, cwindow.api, floating);
        CDEBUG ("GUI: created: %d\n", created);
        //bool scaled = plugin_gui->set_scale (plugin_, scale);
        //CDEBUG ("GUI: scaled: %f\n", scaled * scale);
        uint32_t width = 0, height = 0;
        bool sized = plugin_gui->get_size (plugin_, &width, &height);
        CDEBUG ("GUI: size=%d: %dx%d\n", sized, width, height);
        cwindow.x11 = create_x11_window (width, height);
        bool parentset = plugin_gui->set_parent (plugin_, &cwindow);
        CDEBUG ("GUI: parentset: %d\n", parentset);
        bool canresize = plugin_gui->can_resize (plugin_);
        CDEBUG ("GUI: canresize: %d\n", canresize);
        gui_windowid = cwindow.x11;
      }
    }
    if (gui_windowid) {
      gui_visible = plugin_gui->show (plugin_);
      x11wrapper()->show_window (gui_windowid);
      CDEBUG ("GUI: gui_visible: %d\n", gui_visible);
    }
  }
  void
  hide_gui()
  {
    if (gui_windowid) {
      plugin_gui->hide (plugin_);
      x11wrapper()->show_window (gui_windowid);
      gui_visible = false;
    }
  }
  void
  destroy_gui()
  {
    hide_gui();
    if (gui_windowid) {
      plugin_gui->destroy (plugin_);
      x11wrapper()->destroy_window (gui_windowid);
      gui_windowid = 0;
    }
  }
};

void
ClapDeviceImpl::PluginHandle::log (clap_log_severity severity, const char *msg)
{
  static const char *severtities[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL",
                                       "BADHOST", "BADPLUGIN", };
  const char *cls = severity < sizeof (severtities) / sizeof (severtities[0]) ? severtities[severity] : "MISC";
  if (severity != CLAP_LOG_DEBUG || CDEBUG_ENABLED())
    printerr ("CLAP-%s:%s: %s\n", cls, clapid_, msg);
}

// == CLAP AudioWrapper ==
class ClapDeviceImpl::AudioWrapper : public AudioProcessor {
  PluginHandle *handle_ = nullptr;
  bool can_process_ = false;
  clap_note_dialect eventinput_ = clap_note_dialect (0), eventoutput_ = clap_note_dialect (0);
  uint ibus_clapidx_ = -1, obus_clapidx_ = -1;
  String ibus_name_, obus_name_;
  IBusId ibusid = {};
  OBusId obusid = {};
public:
  AudioWrapper (AudioEngine &engine) :
    AudioProcessor (engine)
  {}
  void
  event_ports (clap_note_dialect eventinput, clap_note_dialect eventoutput)
  {
    eventinput_ = eventinput;
    eventoutput_ = eventoutput;
  }
  void
  stereo_ibus (uint ibus_clapidx, const char *ibus_name)
  {
    ibus_clapidx_ = ibus_clapidx;
    ibus_name_ = ibus_name;
  }
  void
  stereo_obus (uint obus_clapidx, const char *obus_name)
  {
    obus_clapidx_ = obus_clapidx;
    obus_name_ = obus_name;
  }
  static void
  static_info (AudioProcessorInfo &info)
  {
    info.label = "Anklang.Devices.ClapAudioWrapper";
  }
  void
  reset (uint64 target_stamp) override
  {}
  void
  convert_clap_events (int64_t steady_time)
  {
    auto &input_events = handle_->input_events;
    MidiEventRange erange = get_event_input();
    if (input_events.capacity() < erange.events_pending())
      input_events.reserve (erange.events_pending() + 128);
    input_events.resize (erange.events_pending());
    uint j = 0;
    for (const auto &ev : erange)
      switch (ev.message())
        {
          clap_event_note_t *enote;
        case MidiMessage::NOTE_ON:
        case MidiMessage::NOTE_OFF:
          enote = &input_events[j++].note;
          enote->header.size = sizeof (*enote);
          enote->header.type = ev.message() == MidiMessage::NOTE_ON ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF;
          enote->header.time = MAX (ev.frame, 0);
          enote->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
          enote->header.flags = 0;
          enote->note_id = ev.noteid;
          enote->port_index = 0;
          enote->channel = ev.channel;
          enote->key = ev.key;
          enote->velocity = ev.velocity;
          break;
        case MidiMessage::ALL_NOTES_OFF:
          enote = &input_events[j++].note;
          enote->header.size = sizeof (*enote);
          enote->header.type = CLAP_EVENT_NOTE_CHOKE;
          enote->header.time = MAX (ev.frame, 0);
          enote->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
          enote->header.flags = 0;
          enote->note_id = -1;
          enote->port_index = 0;
          enote->channel = -1;
          enote->key = -1;
          enote->velocity = 0;
          break;
        default: ;
        }
    input_events.resize (j);
    // for (const auto &ev : input_events) CDEBUG ("input: %s", clap_event_to_string (&ev.note));
  }
  void
  convert_midi1_events (int64_t steady_time)
  {
    auto &input_events = handle_->input_events;
    MidiEventRange erange = get_event_input();
    if (input_events.capacity() < erange.events_pending())
      input_events.reserve (erange.events_pending() + 128);
    input_events.resize (erange.events_pending());
    uint j = 0;
    for (const auto &ev : erange)
      switch (ev.message())
        {
          clap_event_midi_t *midi1;
        case MidiMessage::NOTE_ON:
        case MidiMessage::NOTE_OFF:
          midi1 = &input_events[j++].midi1;
          midi1->header.size = sizeof (*midi1);
          midi1->header.type = CLAP_EVENT_MIDI;
          midi1->header.time = MAX (ev.frame, 0);
          midi1->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
          midi1->header.flags = 0;
          midi1->port_index = 0;
          if (ev.message() == MidiMessage::NOTE_ON)
            midi1->data[0] = 0x90 + ev.channel;
          else
            midi1->data[0] = 0x80 + ev.channel;
          midi1->data[1] = ev.key;
          midi1->data[2] = uint8_t (ev.velocity * 127);
          break;
        case MidiMessage::ALL_NOTES_OFF:
          midi1 = &input_events[j++].midi1;
          midi1->header.size = sizeof (*midi1);
          midi1->header.type = CLAP_EVENT_MIDI;
          midi1->header.time = MAX (ev.frame, 0);
          midi1->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
          midi1->header.flags = 0;
          midi1->port_index = 0;
          midi1->data[0] = 0xB0 + ev.channel;
          midi1->data[1] = 123;
          midi1->data[2] = 0;
          break;
        default: ;
        }
    input_events.resize (j);
  }
  void (AudioWrapper::*convert_input_events) (int64_t) = nullptr;
  void
  initialize (SpeakerArrangement busses) override
  {
    remove_all_buses();
    if (ibus_clapidx_ != -1)
      ibusid = add_input_bus (ibus_name_, SpeakerArrangement::STEREO);
    if (obus_clapidx_ != -1)
      obusid = add_output_bus (obus_name_, SpeakerArrangement::STEREO);
    if (eventoutput_)
      prepare_event_output();
    if (eventinput_ & CLAP_NOTE_DIALECT_CLAP) {
      prepare_event_input();
      convert_input_events = &AudioWrapper::convert_clap_events;
    } else if (eventinput_ & CLAP_NOTE_DIALECT_MIDI) {
      prepare_event_input();
      convert_input_events = &AudioWrapper::convert_midi1_events;
    } else
      convert_input_events = nullptr;
    // workaround AudioProcessor asserting that a Processor should have *some* IO bus
    if (!convert_input_events && !eventoutput_ && ibusid == IBusId (0) && obusid == OBusId (0))
      prepare_event_input();
  }
  clap_process_t processinfo = { 0, };
  void
  set_plugin (ClapDeviceImpl::PluginHandle *handle)
  {
    if (handle_)
      handle_->stop_processing();
    can_process_ = false;
    handle_ = handle;
    if (handle_) {
      can_process_ = handle_->start_processing();
      processinfo = clap_process_t {
        .steady_time = 0, .frames_count = 0, .transport = nullptr,
        .audio_inputs = &handle_->audio_inputs[0], .audio_outputs = &handle_->audio_outputs[0],
        .audio_inputs_count = uint32_t (handle_->audio_inputs.size()),
        .audio_outputs_count = uint32_t (handle_->audio_outputs.size()),
        .in_events = &handle_->plugin_input_events, .out_events = &handle_->plugin_output_events,
      };
    }
  }
  void
  render (uint n_frames) override
  {
    const uint ni = ibusid != IBusId (0) ? this->n_ichannels (ibusid) : 0;
    for (size_t i = 0; i < ni; i++) {
      assert_return (processinfo.audio_inputs[ibus_clapidx_].channel_count == ni);
      processinfo.audio_inputs[ibus_clapidx_].data32[i] = const_cast<float*> (ifloats (ibusid, i));
    }
    const uint no = obusid != OBusId (0) ? this->n_ochannels (obusid) : 0;
    for (size_t i = 0; i < no; i++) {
      assert_return (processinfo.audio_outputs[obus_clapidx_].channel_count == no);
      processinfo.audio_outputs[obus_clapidx_].data32[i] = oblock (obusid, i);
    }
    // FIXME: pass through if !can_process_ or CLAP_PROCESS_ERROR
    if (can_process_) {
      processinfo.frames_count = n_frames;
      if (convert_input_events)
        (this->*convert_input_events) (processinfo.steady_time);
      processinfo.steady_time += processinfo.frames_count;
      clap_process_status status = handle_->process (&processinfo);
      (void) status;
    }
  }
};
static auto clap_audio_wrapper_id = register_audio_processor<ClapDeviceImpl::AudioWrapper>();

// == ClapDeviceImpl ==
JSONIPC_INHERIT (ClapDeviceImpl, Device);

ClapDeviceImpl::ClapDeviceImpl (const String &clapid, AudioProcessorP aproc) :
  proc_ (shared_ptr_cast<AudioWrapper> (aproc))
{
  assert_return (proc_ != nullptr);
  for (PluginDescriptor *descriptor : PluginDescriptor::collect_descriptors())
    if (clapid == descriptor->id) {
      descriptor_ = descriptor;
      break;
    }
  if (descriptor_)
    descriptor_->open();
  const clap_plugin_entry *pluginentry = !descriptor_ || !proc_ ? nullptr : descriptor_->entry();
  const clap_plugin_factory *pluginfactory = !pluginentry ? nullptr :
                                             (const clap_plugin_factory *) pluginentry->get_factory (CLAP_PLUGIN_FACTORY_ID);
  handle_ = std::make_shared<PluginHandle> (pluginfactory, clapid);
  if (handle_) {
    handle_->get_port_infos();
    uint ibus_clapidx = CLAP_INVALID_ID, ibus_channels = 0;
    uint obus_clapidx = CLAP_INVALID_ID, obus_channels = 0;
    const char *ibus_name = nullptr, *obus_name = nullptr;
    const auto &audio_iport_infos = handle_->audio_iport_infos;
    for (size_t i = 0; i < audio_iport_infos.size(); i++)
      if (audio_iport_infos[i].port_type && strcmp (audio_iport_infos[i].port_type, CLAP_PORT_STEREO) == 0 &&
          audio_iport_infos[i].channel_count == 2 && audio_iport_infos[i].flags & CLAP_AUDIO_PORT_IS_MAIN) {
        // ibus_clapid = audio_iport_infos[i].id;
        ibus_clapidx = i;
        ibus_name = audio_iport_infos[i].name;
        ibus_channels = audio_iport_infos[i].channel_count;
      }
    const auto &audio_oport_infos = handle_->audio_oport_infos;
    for (size_t i = 0; i < audio_oport_infos.size(); i++)
      if (audio_oport_infos[i].port_type && strcmp (audio_oport_infos[i].port_type, CLAP_PORT_STEREO) == 0 &&
          audio_oport_infos[i].channel_count == 2 && audio_oport_infos[i].flags & CLAP_AUDIO_PORT_IS_MAIN) {
        // obus_clapid = audio_oport_infos[i].id;
        obus_clapidx = i;
        obus_name = audio_oport_infos[i].name;
        obus_channels = audio_oport_infos[i].channel_count;
      }
    if (ibus_channels == 2)
      proc_->stereo_ibus (ibus_clapidx, ibus_name);
    if (obus_channels == 2)
      proc_->stereo_obus (obus_clapidx, obus_name);
    proc_->event_ports (clap_note_dialect (handle_->note_iport_infos.size() ? handle_->note_iport_infos[0].supported_dialects : 0),
                        clap_note_dialect (handle_->note_oport_infos.size() ? handle_->note_oport_infos[0].supported_dialects : 0));
    if (handle_->note_iport_infos.size())
      handle_->input_events.reserve (1024);
  }
}

ClapDeviceImpl::~ClapDeviceImpl()
{
  if (handle_)
    handle_->destroy();
  handle_ = nullptr;
  if (descriptor_)
    descriptor_->close();
}

void
ClapDeviceImpl::_set_parent (Gadget *parent)
{
  ClapDeviceImplP selfp = shared_ptr_cast<ClapDeviceImpl> (this);
  GadgetImpl::_set_parent (parent);
  // activate and use plugin from proc_
  if (parent && handle_) {
    handle_->show_gui();
    if (handle_->activate (proc_->engine())) {
      handle_->show_gui();
      auto j = [selfp] () {
        selfp->proc_->set_plugin (&*selfp->handle_);
      };
      proc_->engine().async_jobs += j;
    }
  }
  // deactivate and destroy plugin, but first stop proc_ using it
  if (!parent && handle_)
    {
      handle_->destroy_gui();
      auto deferred_destroy = [selfp] (void*) {
        selfp->handle_->destroy();
      };
      std::shared_ptr<void> atjobdtor = { nullptr, deferred_destroy };
      auto j = [selfp, atjobdtor] () {
        selfp->proc_->set_plugin (nullptr);
      };
      proc_->engine().async_jobs += j;
      // once job is processed, dtor runs in mainthread
    }
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

AudioProcessorP
ClapDeviceImpl::_audio_processor () const
{
  return proc_;
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

DeviceP
ClapDeviceImpl::create_clap_device (AudioEngine &engine, const String &clapid)
{
  assert_return (string_startswith (clapid, "CLAP:"), nullptr);
  auto make_device = [&clapid] (const String &aseid, AudioProcessor::StaticInfo static_info, AudioProcessorP aproc) -> DeviceP {
    return ClapDeviceImpl::make_shared (clapid.substr (5), aproc);
  };
  DeviceP devicep = AudioProcessor::registry_create (clap_audio_wrapper_id, engine, make_device);
  assert_return (devicep && devicep->_audio_processor(), nullptr);
  return devicep;
}

} // Ase
