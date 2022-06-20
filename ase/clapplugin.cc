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
static void             try_load_x11wrapper      ();
static Gtk2DlWrapEntry *x11wrapper = nullptr;
static float scratch_float_buffer[AUDIO_BLOCK_FLOAT_ZEROS_SIZE];

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
      get_port_infos();
    }
  }
  ~ClapPluginHandleImpl()
  {
    destroy();
  }
  bool plugin_activated = false;
  bool plugin_processing = false;
  bool gui_visible = false;
  bool gui_canresize = false;
  ulong gui_windowid = 0;
  std::vector<uint> timers;
  void get_port_infos ();
  bool
  activate (AudioEngine &engine) override
  {
    return_unless (plugin_, false);
    assert_return (!activated(), true);
    plugin_activated = plugin_->activate (plugin_, engine.sample_rate(), 32, 4096);
    CDEBUG ("%s: %s", clapid(), plugin_activated ? "activate" : "failed to activate");
    return activated();
  }
  bool activated() const override { return plugin_activated; }
  void
  deactivate() override
  {
    return_unless (plugin_);
    assert_return (activated());
    plugin_activated = false;
    plugin_->deactivate (plugin_);
    CDEBUG ("%s: deactivated", clapid());
  }
  bool
  start_processing() override
  {
    return_unless (plugin_, false);
    assert_return (!plugin_processing, false);
    plugin_processing = plugin_->start_processing && plugin_->start_processing (plugin_);
    return plugin_processing;
  }
  clap_process_status
  process (const clap_process_t *clapprocess) override
  {
    assert_return (plugin_processing, CLAP_PROCESS_ERROR);
    return plugin_ && plugin_->process (plugin_, clapprocess);
  }
  void
  stop_processing() override
  {
    return_unless (plugin_processing);
    if (plugin_->stop_processing)
      plugin_->stop_processing (plugin_);
    plugin_processing = false;
  }
  void show_gui    () override;
  void hide_gui    () override;
  void destroy_gui () override;
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

// == get_port_infos ==
void
ClapPluginHandleImpl::get_port_infos()
{
  assert_return (!activated());
  uint total_channels = 0;
  // audio_ports_configs_
  audio_ports_configs_.resize (!plugin_audio_ports_config ? 0 : plugin_audio_ports_config->count (plugin_));
  for (size_t i = 0; i < audio_ports_configs_.size(); i++)
    if (!plugin_audio_ports_config->get (plugin_, i, &audio_ports_configs_[i]))
      audio_ports_configs_[i] = { CLAP_INVALID_ID, { 0, }, 0, 0, 0, 0, "", 0, 0, "" };
  if (audio_ports_configs_.size()) { // not encountered yet
    String s = string_format ("audio_configs:%u:", audio_ports_configs_.size());
    for (size_t i = 0; i < audio_ports_configs_.size(); i++)
      if (audio_ports_configs_[i].id != CLAP_INVALID_ID)
        s += string_format (" %u:%s:iports=%u:oports=%u:imain=%u,%s:omain=%u,%s",
                            audio_ports_configs_[i].id,
                            audio_ports_configs_[i].name,
                            audio_ports_configs_[i].input_port_count,
                            audio_ports_configs_[i].output_port_count,
                            audio_ports_configs_[i].has_main_input * audio_ports_configs_[i].main_input_channel_count,
                            audio_ports_configs_[i].main_input_port_type,
                            audio_ports_configs_[i].has_main_output * audio_ports_configs_[i].main_output_channel_count,
                            audio_ports_configs_[i].main_output_port_type);
    CDEBUG ("%s: %s", clapid(), s);
  }
  // note_iport_infos_
  note_iport_infos_.resize (!plugin_note_ports ? 0 : plugin_note_ports->count (plugin_, true));
  for (size_t i = 0; i < note_iport_infos_.size(); i++)
    if (!plugin_note_ports->get (plugin_, i, true, &note_iport_infos_[i]))
      note_iport_infos_[i] = { CLAP_INVALID_ID, 0, 0, { 0, }, };
  if (note_iport_infos_.size()) {
    String s = string_format ("note_iports=%u:", note_iport_infos_.size());
    for (size_t i = 0; i < note_iport_infos_.size(); i++)
      if (note_iport_infos_[i].id != CLAP_INVALID_ID)
        s += string_format (" %u:%s:can=%x:want=%x",
                            note_iport_infos_[i].id,
                            note_iport_infos_[i].name,
                            note_iport_infos_[i].supported_dialects,
                            note_iport_infos_[i].preferred_dialect);
    CDEBUG ("%s: %s", clapid(), s);
  }
  // note_oport_infos_
  note_oport_infos_.resize (!plugin_note_ports ? 0 : plugin_note_ports->count (plugin_, false));
  for (size_t i = 0; i < note_oport_infos_.size(); i++)
    if (!plugin_note_ports->get (plugin_, i, false, &note_oport_infos_[i]))
      note_oport_infos_[i] = { CLAP_INVALID_ID, 0, 0, { 0, }, };
  if (note_oport_infos_.size()) {
    String s = string_format ("note_oports=%u:", note_oport_infos_.size());
    for (size_t i = 0; i < note_oport_infos_.size(); i++)
      if (note_oport_infos_[i].id != CLAP_INVALID_ID)
        s += string_format (" %u:%s:can=%x:want=%x",
                            note_oport_infos_[i].id,
                            note_oport_infos_[i].name,
                            note_oport_infos_[i].supported_dialects,
                            note_oport_infos_[i].preferred_dialect);
    CDEBUG ("%s: %s", clapid(), s);
  }
  // audio_iport_infos_
  audio_iport_infos_.resize (!plugin_audio_ports ? 0 : plugin_audio_ports->count (plugin_, true));
  for (size_t i = 0; i < audio_iport_infos_.size(); i++)
    if (!plugin_audio_ports->get (plugin_, i, true, &audio_iport_infos_[i]))
      audio_iport_infos_[i] = { CLAP_INVALID_ID, { 0 }, 0, 0, "", CLAP_INVALID_ID };
    else
      total_channels += audio_iport_infos_[i].channel_count;
  if (audio_iport_infos_.size()) {
    String s = string_format ("audio_iports=%u:", audio_iport_infos_.size());
    for (size_t i = 0; i < audio_iport_infos_.size(); i++)
      if (audio_iport_infos_[i].id != CLAP_INVALID_ID && audio_iport_infos_[i].port_type)
        s += string_format (" %u:ch=%u:%s:m=%u:%s:",
                            audio_iport_infos_[i].id,
                            audio_iport_infos_[i].channel_count,
                            audio_iport_infos_[i].name,
                            audio_iport_infos_[i].flags & CLAP_AUDIO_PORT_IS_MAIN,
                            audio_iport_infos_[i].port_type);
    CDEBUG ("%s: %s", clapid(), s);
  }
  // audio_oport_infos_
  audio_oport_infos_.resize (!plugin_audio_ports ? 0 : plugin_audio_ports->count (plugin_, false));
  for (size_t i = 0; i < audio_oport_infos_.size(); i++)
    if (!plugin_audio_ports->get (plugin_, i, false, &audio_oport_infos_[i]))
      audio_oport_infos_[i] = { CLAP_INVALID_ID, { 0 }, 0, 0, "", CLAP_INVALID_ID };
    else
      total_channels += audio_oport_infos_[i].channel_count;
  if (audio_oport_infos_.size()) {
    String s = string_format ("audio_oports=%u:", audio_oport_infos_.size());
    for (size_t i = 0; i < audio_oport_infos_.size(); i++)
      if (audio_oport_infos_[i].id != CLAP_INVALID_ID && audio_oport_infos_[i].port_type)
        s += string_format (" %u:ch=%u:%s:m=%u:%s:",
                            audio_oport_infos_[i].id,
                            audio_oport_infos_[i].channel_count,
                            audio_oport_infos_[i].name,
                            audio_oport_infos_[i].flags & CLAP_AUDIO_PORT_IS_MAIN,
                            audio_oport_infos_[i].port_type);
    CDEBUG ("%s: %s", clapid(), s);
  }
  // allocate .data32 pointer arrays for all input/output port channels
  data32ptrs_.resize (total_channels);
  // audio_inputs_
  audio_inputs_.resize (audio_iport_infos_.size());
  for (size_t i = 0; i < audio_inputs_.size(); i++) {
    audio_inputs_[i] = { nullptr, nullptr, 0, 0, 0 };
    if (audio_iport_infos_[i].id == CLAP_INVALID_ID) continue;
    audio_inputs_[i].channel_count = audio_iport_infos_[i].channel_count;
    total_channels -= audio_inputs_[i].channel_count;
    audio_inputs_[i].data32 = &data32ptrs_[total_channels];
    for (size_t j = 0; j < audio_inputs_[i].channel_count; j++)
      audio_inputs_[i].data32[j] = const_cast<float*> (const_float_zeros);
  }
  // audio_outputs_
  audio_outputs_.resize (audio_oport_infos_.size());
  for (size_t i = 0; i < audio_outputs_.size(); i++) {
    audio_outputs_[i] = { nullptr, nullptr, 0, 0, 0 };
    if (audio_oport_infos_[i].id == CLAP_INVALID_ID) continue;
    audio_outputs_[i].channel_count = audio_oport_infos_[i].channel_count;
    total_channels -= audio_outputs_[i].channel_count;
    audio_outputs_[i].data32 = &data32ptrs_[total_channels];
    for (size_t j = 0; j < audio_outputs_[i].channel_count; j++)
      audio_outputs_[i].data32[j] = scratch_float_buffer;
  }
  assert_return (total_channels == 0);
  // input_events_
  input_events_.resize (0);
  // output_events_
  output_events_.resize (0);
}

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

// == clap_host_audio_ports ==
static bool
host_is_rescan_flag_supported (const clap_host_t *host, uint32_t flag)
{
  const bool supported = false;
  CDEBUG ("%s: %s: %s", clapid (host), __func__, supported);
  return supported;
}

static void
host_rescan (const clap_host_t *host, uint32_t flag)
{
  CDEBUG ("%s: %s", clapid (host), __func__);
}

static const clap_host_audio_ports host_ext_audio_ports = {
  .is_rescan_flag_supported = host_is_rescan_flag_supported,
  .rescan = host_rescan,
};

// == clap_host_params ==
static void
host_params_rescan (const clap_host_t *host, clap_param_rescan_flags flags)
{
  CDEBUG ("%s: %s(0x%x)", clapid (host), __func__, flags);
}

static void
host_params_clear (const clap_host_t *host, clap_id param_id, clap_param_clear_flags flags)
{
  CDEBUG ("%s: %s(%u,0x%x)", clapid (host), __func__, param_id, flags);
}

static void
host_request_flush (const clap_host_t *host)
{
  CDEBUG ("%s: %s", clapid (host), __func__);
}

static const clap_host_params host_ext_params = {
  .rescan = host_params_rescan,
  .clear = host_params_clear,
  .request_flush = host_request_flush,
};

// == clap_host_gui ==
static void
host_gui_delete_request (ClapPluginHandleImplP handlep)
{
  CDEBUG ("%s: %s", handlep->clapid(), __func__);
  handlep->destroy_gui();
}

static ulong
host_gui_create_x11_window (ClapPluginHandleImplP handlep, int width, int height)
{
  Gtk2WindowSetup wsetup {
    .title = handlep->clapid(), .width = width, .height = height,
    .deleterequest_mt = [handlep] () {
      main_loop->exec_callback ([handlep]() {
        host_gui_delete_request (handlep);
      });
    },
  };
  const ulong windowid = x11wrapper->create_window (wsetup);
  return windowid;
}

void
ClapPluginHandleImpl::show_gui()
{
  if (plugin_gui)
    try_load_x11wrapper();
  if (!gui_windowid && plugin_gui && x11wrapper)
    {
      ClapPluginHandleImplP handlep = handle_sptr (&phost);
      const bool floating = false;
      clap_window_t cwindow = {
        .api = CLAP_WINDOW_API_X11,
        .x11 = 0
      };
      if (plugin_gui->is_api_supported (plugin_, cwindow.api, floating))
        {
          bool created = plugin_gui->create (plugin_, cwindow.api, floating);
          CDEBUG ("%s: gui_create: %d\n", clapid(), created);
          gui_canresize = plugin_gui->can_resize (plugin_);
          CDEBUG ("%s: gui_can_resize: %d\n", clapid(), gui_canresize);
          const double scale = 1.0;
          const bool scaled = plugin_gui->set_scale (plugin_, scale);
          CDEBUG ("%s: gui_set_scale(%f): %f\n", clapid(), scale, scaled);
          uint32_t width = 0, height = 0;
          const bool sized = plugin_gui->get_size (plugin_, &width, &height);
          CDEBUG ("%s: gui_get_size: %ux%u: %d\n", clapid(), width, height, sized);
          cwindow.x11 = host_gui_create_x11_window (handlep, width, height);
          const bool parentset = plugin_gui->set_parent (plugin_, &cwindow);
          CDEBUG ("%s: gui_set_parent: %d\n", clapid(), parentset);
          gui_windowid = cwindow.x11;
        }
    }
  if (gui_windowid) {
    gui_visible = plugin_gui->show (plugin_);
    CDEBUG ("%s: gui_show: %d\n", clapid(), gui_visible);
    if (!gui_visible)
      ; // do nothing, early JUCE versions have a bug returning false here
    x11wrapper->show_window (gui_windowid);
  }
}

void
ClapPluginHandleImpl::hide_gui()
{
  if (gui_windowid)
    {
      plugin_gui->hide (plugin_);
      x11wrapper->hide_window (gui_windowid);
      gui_visible = false;
  }
}

void
ClapPluginHandleImpl::destroy_gui()
{
  hide_gui();
  if (gui_windowid) {
    plugin_gui->destroy (plugin_);
    x11wrapper->destroy_window (gui_windowid);
    gui_windowid = 0;
  }
}

static void
host_resize_hints_changed (const clap_host_t *host)
{
  CDEBUG ("%s: %s", clapid (host), __func__);
}

static bool
host_request_resize (const clap_host_t *host, uint32_t width, uint32_t height)
{
  ClapPluginHandleImpl *handle = handle_ptr (host);
  CDEBUG ("%s: %s(%u,%u)", clapid (host), __func__, width, height);
  if (handle->gui_windowid) {
    if (x11wrapper->resize_window (handle->gui_windowid, width, height)) {
      if (handle->plugin_gui->can_resize (handle->plugin_))
        handle->plugin_gui->set_size (handle->plugin_, width, height);
      return true;
    }
  }
  return false;
}

static bool
host_request_show (const clap_host_t *host)
{
  const bool supported = false;
  CDEBUG ("%s: %s: %d", clapid (host), __func__, supported);
  return supported;
}

static bool
host_request_hide (const clap_host_t *host)
{
  const bool supported = false;
  CDEBUG ("%s: %s: %d", clapid (host), __func__, supported);
  return supported;
}

static void
host_gui_closed (const clap_host_t *host, bool was_destroyed)
{
  ClapPluginHandleImpl *handle = handle_ptr (host);
  CDEBUG ("%s: %s(was_destroyed=%u)", clapid (host), __func__, was_destroyed);
  handle->gui_visible = false;
  if (was_destroyed && handle->plugin_gui) {
    handle->gui_windowid = 0;
    handle->plugin_gui->destroy (handle->plugin_);
  }
}

static const clap_host_gui host_ext_gui = {
  .resize_hints_changed = host_resize_hints_changed,
  .request_resize = host_request_resize,
  .request_show = host_request_show,
  .request_hide = host_request_hide,
  .closed = host_gui_closed,
};

// == clap_host extensions ==
static const void*
host_get_extension (const clap_host *host, const char *extension_id)
{
  const String ext = extension_id;
  if (ext == CLAP_EXT_LOG)            return &host_ext_log;
  if (ext == CLAP_EXT_GUI)            return &host_ext_gui;
  if (ext == CLAP_EXT_TIMER_SUPPORT)  return &host_ext_timer_support;
  if (ext == CLAP_EXT_THREAD_CHECK)   return &host_ext_thread_check;
  if (ext == CLAP_EXT_AUDIO_PORTS)    return &host_ext_audio_ports;
  if (ext == CLAP_EXT_PARAMS)         return &host_ext_params;
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

// == Gtk2DlWrapEntry ==
static void
try_load_x11wrapper()
{
  return_unless (x11wrapper == nullptr);
  static Gtk2DlWrapEntry *gtk2wrapentry = [] () {
    String gtk2wrapso = anklang_runpath (RPath::LIBDIR, "gtk2wrap.so");
    void *gtkdlhandle_ = dlopen (gtk2wrapso.c_str(), RTLD_LOCAL | RTLD_NOW);
    const char *symname = "Ase__Gtk2__wrapentry";
    void *ptr = gtkdlhandle_ ? dlsym (gtkdlhandle_, symname) : nullptr;
    return (Gtk2DlWrapEntry*) ptr;
  } ();
  x11wrapper = gtk2wrapentry;
}

} // Ase
