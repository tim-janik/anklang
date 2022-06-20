// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clapplugin.hh"
#include "jsonipc/jsonipc.hh"
#include "processor.hh"
#include "path.hh"
#include "main.hh"
#include "internal.hh"
#include "gtk2wrap.hh"
#include <dlfcn.h>
#include <glob.h>

#define CDEBUG(...)     Ase::debug ("Clap", __VA_ARGS__)
#define CDEBUG_ENABLED() Ase::debug_key_enabled ("Clap")

namespace Ase {

ASE_CLASS_DECLS (ClapPluginHandleImpl);

// == fwd decls ==
static const char*      anklang_host_name        ();
static String           clapid                   (const clap_host *host);
static ClapPluginHandleImpl*  handle_ptr               (const clap_host *host);
static ClapPluginHandleImplP  handle_sptr              (const clap_host *host);
static const void*      host_get_extension       (const clap_host *host, const char *extension_id);
static void             host_request_callback_mt (const clap_host *host);
static bool             host_unregister_timer    (const clap_host *host, clap_id timer_id);

// == ClapPluginHandle ==
ClapPluginHandle::ClapPluginHandle (const String &clapid) :
  clapid_ (clapid)
{}

ClapPluginHandle::~ClapPluginHandle()
{}

// == ClapPluginHandleImpl ==
class ClapPluginHandleImpl : public ClapPluginHandle {
public:
  static String     clapid (const clap_host *host) { return Ase::clapid (host); }
  String            clapid () const                { return ClapPluginHandle::clapid(); }
  clap_host_t phost = {
    .clap_version = CLAP_VERSION,
    .host_data = (ClapPluginHandleImpl*) this,
    .name = anklang_host_name(), .vendor = "anklang.testbit.eu",
    .url = "https://anklang.testbit.eu/", .version = ase_version(),
    .get_extension = [] (const clap_host *host, const char *extension_id) {
      const void *ext = host_get_extension (host, extension_id);
      CDEBUG ("%s: host_get_extension(\"%s\"): %p", clapid (host), extension_id, ext);
      return ext;
    },
    .request_restart = [] (const clap_host *host) {
      CDEBUG ("%s: host.request_restart", clapid (host));
    },  // deactivate() + activate()
    .request_process = [] (const clap_host *host) {
      CDEBUG ("%s: host.request_process", clapid (host));
    },  // process()
    .request_callback = host_request_callback_mt,
  };
  const clap_plugin_t *plugin_ = nullptr;
  const clap_plugin_gui *plugin_gui = nullptr;
  const clap_plugin_params *plugin_params = nullptr;
  const clap_plugin_timer_support *plugin_timer_support = nullptr;
  const clap_plugin_audio_ports_config *plugin_audio_ports_config = nullptr;
  const clap_plugin_audio_ports *plugin_audio_ports = nullptr;
  const clap_plugin_note_ports *plugin_note_ports = nullptr;
  ClapPluginHandleImpl (const clap_plugin_factory *factory, const String &clapid_) :
    ClapPluginHandle (clapid_)
  {
    if (factory)
      plugin_ = factory->create_plugin (factory, &phost, clapid().c_str());
    if (plugin_ && !plugin_->init (plugin_)) {
      CDEBUG ("%s: initialization failed", clapid());
      destroy(); // destroy per spec and cleanup resources used by init()
    }
    if (plugin_) {
      CDEBUG ("%s: initialized", clapid());
      plugin_gui = (const clap_plugin_gui*) plugin_->get_extension (plugin_, CLAP_EXT_GUI);
      plugin_params = (const clap_plugin_params*) plugin_->get_extension (plugin_, CLAP_EXT_PARAMS);
      plugin_timer_support = (const clap_plugin_timer_support*) plugin_->get_extension (plugin_, CLAP_EXT_TIMER_SUPPORT);
      plugin_audio_ports_config = (const clap_plugin_audio_ports_config*) plugin_->get_extension (plugin_, CLAP_EXT_AUDIO_PORTS_CONFIG);
      plugin_audio_ports = (const clap_plugin_audio_ports*) plugin_->get_extension (plugin_, CLAP_EXT_AUDIO_PORTS);
      plugin_note_ports = (const clap_plugin_note_ports*) plugin_->get_extension (plugin_, CLAP_EXT_NOTE_PORTS);
    }
  }
  ~ClapPluginHandleImpl()
  {
    destroy();
  }
  std::vector<uint> timers;
  void
  destroy () override
  {
    destroy_gui();
    if (plugin_ && activated())
      deactivate();
    if (plugin_)
      CDEBUG ("%s: destroying", clapid());
    while (timers.size())
      host_unregister_timer (&phost, timers.back());
    if (plugin_)
      plugin_->destroy (plugin_);
    plugin_ = nullptr;
    plugin_gui = nullptr;
    plugin_params = nullptr;
    plugin_timer_support = nullptr;
    plugin_audio_ports_config = nullptr;
    plugin_audio_ports = nullptr;
    plugin_note_ports = nullptr;
  }
};

// == helpers ==
static const char*
anklang_host_name()
{
  static String name = "Anklang//" + executable_name();
  return name.c_str();
}

static String
clapid (const clap_host *host)
{
  ClapPluginHandleImpl *handle = handle_ptr (host);
  return handle->clapid();
}

static ClapPluginHandleImpl*
handle_ptr (const clap_host *host)
{
  ClapPluginHandleImpl *handle = (ClapPluginHandleImpl*) host->host_data;
  return handle;
}

static ClapPluginHandleImplP
handle_sptr (const clap_host *host)
{
  ClapPluginHandleImpl *handle = handle_ptr (host);
  return shared_ptr_cast<ClapPluginHandleImpl> (handle);
}

// == clap_host_log ==
static void
host_log (const clap_host_t *host, clap_log_severity severity, const char *msg)
{
  static const char *severtities[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL",
                                       "BADHOST", "BADPLUGIN", };
  const char *cls = severity < sizeof (severtities) / sizeof (severtities[0]) ? severtities[severity] : "MISC";
  if (severity == CLAP_LOG_DEBUG)
    CDEBUG ("%s: %s", clapid (host), msg);
  else // severity != CLAP_LOG_DEBUG
    printerr ("CLAP-%s:%s: %s\n", cls, clapid (host), msg);
}
static const clap_host_log host_ext_log = { .log = host_log };

// == clap_host_timer_support ==
static bool
host_call_on_timer (ClapPluginHandleImplP handlep, clap_id timer_id)
{
  // gui_threads_enter();
  if (handlep->plugin_timer_support) // register_timer() runs too early for this check
    handlep->plugin_timer_support->on_timer (handlep->plugin_, timer_id);
  // gui_threads_leave();
  return true; // keep-alive
}

static bool
host_register_timer (const clap_host *host, uint32_t period_ms, clap_id *timer_id)
{
  // Note, plugins (JUCE) may call this method during init(), when plugin_timer_support==NULL
  ClapPluginHandleImplP handlep = handle_sptr (host);
  period_ms = MAX (30, period_ms);
  auto timeridp = std::make_shared<uint> (0);
  *timeridp = main_loop->exec_timer ([handlep, timeridp] () {
    return host_call_on_timer (handlep, *timeridp);
  }, period_ms, period_ms, EventLoop::PRIORITY_UPDATE);
  *timer_id = *timeridp;
  handlep->timers.push_back (*timeridp);
  CDEBUG ("%s: %s: ms=%u: id=%u", clapid (host), __func__, period_ms, timer_id);
  return true;
}

static bool
host_unregister_timer (const clap_host *host, clap_id timer_id)
{
  // NOTE: plugin_ might be destroying here
  ClapPluginHandleImpl *handle = handle_ptr (host);
  const bool deleted = Aux::erase_first (handle->timers, [timer_id] (uint id) { return id == timer_id; });
  if (deleted)
    main_loop->remove (timer_id);
  CDEBUG ("%s: %s: deleted=%u: id=%u", clapid (host), __func__, deleted, timer_id);
  return deleted;
}
static const clap_host_timer_support host_ext_timer_support = {
  .register_timer = host_register_timer,
  .unregister_timer = host_unregister_timer,
};

// == clap_host_thread_check ==
static bool
host_is_main_thread (const clap_host_t *host)
{
  return this_thread_is_ase();
}

static bool
host_is_audio_thread (const clap_host_t *host)
{
  return AudioEngine::thread_is_engine();
}

static const clap_host_thread_check host_ext_thread_check = {
  .is_main_thread = host_is_main_thread,
  .is_audio_thread = host_is_audio_thread,
};

// == clap_host extensions ==
static const void*
host_get_extension (const clap_host *host, const char *extension_id)
{
  const String ext = extension_id;
  if (ext == CLAP_EXT_LOG)            return &host_ext_log;
  //if (ext == CLAP_EXT_GUI)            return &host_gui;
  if (ext == CLAP_EXT_TIMER_SUPPORT)  return &host_ext_timer_support;
  if (ext == CLAP_EXT_THREAD_CHECK)   return &host_ext_thread_check;
  //if (ext == CLAP_EXT_AUDIO_PORTS)    return &host_audio_ports_;
  //if (ext == CLAP_EXT_PARAMS)         return &host_params;
  else return nullptr;
}

static void
host_request_callback_mt (const clap_host *host) {
  CDEBUG ("%s: %s", clapid (host), __func__);
  ClapPluginHandleImplP handlep = handle_sptr (host);
  main_loop->exec_callback ([handlep] () {
    if (handlep->plugin_) {
      // gui_threads_enter();
      handlep->plugin_->on_main_thread (handlep->plugin_);
      // gui_threads_leave();
    }
  });
}

} // Ase
