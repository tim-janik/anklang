// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clapplugin.hh"
#include "clapdevice.hh"
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

ASE_CLASS_DECLS (ClapAudioWrapper);
ASE_CLASS_DECLS (ClapPluginHandleImpl);

// == fwd decls ==
static const char*           anklang_host_name        ();
static String                clapid                   (const clap_host *host);
static ClapPluginHandleImpl* handle_ptr               (const clap_host *host);
static ClapPluginHandleImplP handle_sptr              (const clap_host *host);
static const clap_plugin*    access_clap_plugin       (ClapPluginHandle *handle);
static const void*           host_get_extension       (const clap_host *host, const char *extension_id);
static void                  host_request_callback_mt (const clap_host *host);
static bool                  host_unregister_timer    (const clap_host *host, clap_id timer_id);
static void                  try_load_x11wrapper      ();
static Gtk2DlWrapEntry *x11wrapper = nullptr;
static float scratch_float_buffer[AUDIO_BLOCK_FLOAT_ZEROS_SIZE];

// == ClapEventUnion ==
union ClapEventUnion {
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

// == ClapAudioWrapper ==
class ClapAudioWrapper : public AudioProcessor {
  ClapPluginHandle *handle_ = nullptr;
  const clap_plugin *clapplugin_ = nullptr;
  IBusId ibusid = {};
  OBusId obusid = {};
  uint imain_clapidx = ~0, omain_clapidx = ~0, iside_clapidx = ~0, oside_clapidx = ~0;
  clap_note_dialect input_event_dialect = clap_note_dialect (0);
  clap_note_dialect output_event_dialect = clap_note_dialect (0);
  bool can_process_ = false;
public:
  static void
  static_info (AudioProcessorInfo &info)
  {
    info.label = "Anklang.Devices.ClapAudioWrapper";
  }
  ClapAudioWrapper (AudioEngine &engine) :
    AudioProcessor (engine)
  {}
  void
  initialize (SpeakerArrangement busses) override
  {
    remove_all_buses();
    handle_ = &*ClapDeviceImpl::access_clap_handle (get_device());
    assert_return (handle_ != nullptr);
    clapplugin_ = access_clap_plugin (handle_);
    assert_return (clapplugin_ != nullptr);

    // find iports
    const auto &audio_iport_infos = handle_->audio_iport_infos;
    for (size_t i = 0; i < audio_iport_infos.size(); i++)
      if (audio_iport_infos[i].port_type && strcmp (audio_iport_infos[i].port_type, CLAP_PORT_STEREO) == 0 && audio_iport_infos[i].channel_count == 2) {
        if (audio_iport_infos[i].flags & CLAP_AUDIO_PORT_IS_MAIN && imain_clapidx == ~0)
          imain_clapidx = i;
        else if (!(audio_iport_infos[i].flags & CLAP_AUDIO_PORT_IS_MAIN) && iside_clapidx == ~0)
          iside_clapidx = i;
      }
    // find oports
    const clap_audio_port_info_t *main_oport = nullptr, *side_oport = nullptr;
    const auto &audio_oport_infos = handle_->audio_oport_infos;
    for (size_t i = 0; i < audio_oport_infos.size(); i++)
      if (audio_oport_infos[i].port_type && strcmp (audio_oport_infos[i].port_type, CLAP_PORT_STEREO) == 0 && audio_oport_infos[i].channel_count == 2) {
        if (audio_oport_infos[i].flags & CLAP_AUDIO_PORT_IS_MAIN && !main_oport)
          omain_clapidx = i;
        else if (!(audio_oport_infos[i].flags & CLAP_AUDIO_PORT_IS_MAIN) && !side_oport)
          oside_clapidx = i;
      }
    // find event ports
    input_event_dialect  = clap_note_dialect (handle_->note_iport_infos.size() ? handle_->note_iport_infos[0].supported_dialects : 0);
    output_event_dialect = clap_note_dialect (handle_->note_oport_infos.size() ? handle_->note_oport_infos[0].supported_dialects : 0);

    // create busses
    if (imain_clapidx < audio_iport_infos.size())
      ibusid = add_input_bus (audio_iport_infos[imain_clapidx].name, SpeakerArrangement::STEREO);
    if (omain_clapidx < audio_oport_infos.size())
      obusid = add_output_bus (audio_oport_infos[omain_clapidx].name, SpeakerArrangement::STEREO);
    // prepare event IO
    if (input_event_dialect & (CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI)) {
      prepare_event_input();
      input_events_.reserve (256); // avoid audio-thread allocations
    }
    if (output_event_dialect & (CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI))
      prepare_event_output();

    // workaround AudioProcessor asserting that a Processor should have *some* IO facilities
    if (!has_event_output() && !has_event_input() && ibusid == 0 && obusid == 0)
      prepare_event_input();
  }
  void
  reset (uint64 target_stamp) override
  {}
  void convert_clap_events (const clap_process_t &process);
  void convert_midi1_events (const clap_process_t &process);
  std::vector<ClapEventUnion> input_events_;
  std::vector<clap_event_header_t> output_events_;
  static uint32_t
  input_events_size (const clap_input_events *evlist)
  {
    ClapAudioWrapper *self = (ClapAudioWrapper*) evlist->ctx;
    return self->input_events_.size();
  }
  static const clap_event_header_t*
  input_events_get (const clap_input_events *evlist, uint32_t index)
  {
    ClapAudioWrapper *self = (ClapAudioWrapper*) evlist->ctx;
    return &self->input_events_.at (index).header;
  }
  static bool
  output_events_try_push (const clap_output_events *evlist, const clap_event_header_t *event)
  {
    ClapAudioWrapper *self = (ClapAudioWrapper*) evlist->ctx;
    if (0)
      CDEBUG ("%s: %s(type=%x): false", self->handle_->clapid(), __func__, event->type);
    return false;
  }
  const clap_input_events_t plugin_input_events = {
    .ctx = (ClapAudioWrapper*) this,
    .size = input_events_size,
    .get = input_events_get,
  };
  const clap_output_events_t plugin_output_events = {
    .ctx = (ClapAudioWrapper*) this,
    .try_push = output_events_try_push,
  };
  clap_process_t processinfo = { 0, };
  bool
  start_processing()
  {
    return_unless (!can_process_, true);
    assert_return (clapplugin_, false);
    can_process_ = clapplugin_->start_processing (clapplugin_);
    CDEBUG ("%s: %s: %d", handle_->clapid(), __func__, can_process_);
    if (can_process_) {
      processinfo = clap_process_t {
        .steady_time = int64_t (engine().frame_counter()),
        .frames_count = 0, .transport = nullptr,
        .audio_inputs = &handle_->audio_inputs_[0], .audio_outputs = &handle_->audio_outputs_[0],
        .audio_inputs_count = uint32_t (handle_->audio_inputs_.size()),
        .audio_outputs_count = uint32_t (handle_->audio_outputs_.size()),
        .in_events = &plugin_input_events, .out_events = &plugin_output_events,
      };
      input_events_.resize (0);
      output_events_.resize (0);
    }
    return can_process_;
  }
  void
  stop_processing()
  {
    return_unless (can_process_);
    can_process_ = false;
    clapplugin_->stop_processing (clapplugin_);
    CDEBUG ("%s: %s", handle_->clapid(), __func__);
    input_events_.resize (0);
    output_events_.resize (0);
  }
  void
  render (uint n_frames) override
  {
    const uint icount = ibusid != 0 ? this->n_ichannels (ibusid) : 0;
    if (can_process_) {
      for (size_t i = 0; i < icount; i++) {
        assert_return (processinfo.audio_inputs[imain_clapidx].channel_count == icount);
        processinfo.audio_inputs[imain_clapidx].data32[i] = const_cast<float*> (ifloats (ibusid, i));
      }
      const uint ocount = obusid != 0 ? this->n_ochannels (obusid) : 0;
      for (size_t i = 0; i < ocount; i++) {
        assert_return (processinfo.audio_outputs[omain_clapidx].channel_count == ocount);
        processinfo.audio_outputs[omain_clapidx].data32[i] = oblock (obusid, i);
      }
      processinfo.frames_count = n_frames;
      if (input_event_dialect & CLAP_NOTE_DIALECT_CLAP)
        convert_clap_events (processinfo);
      else if (input_event_dialect & CLAP_NOTE_DIALECT_MIDI)
        convert_midi1_events (processinfo);
      processinfo.steady_time += processinfo.frames_count;
      clap_process_status status = clapplugin_->process (clapplugin_, &processinfo);
      (void) status; // CLAP_PROCESS_ERROR ?
      if (0)
        CDEBUG ("render: status=%d", status);
    }
  }
};
static CString clap_audio_wrapper_aseid = register_audio_processor<ClapAudioWrapper>();

void
ClapAudioWrapper::convert_clap_events (const clap_process_t &process)
{
  MidiEventRange erange = get_event_input();
  if (input_events_.capacity() < erange.events_pending())
    input_events_.reserve (erange.events_pending() + 128);
  input_events_.resize (erange.events_pending());
  uint j = 0;
  for (const auto &ev : erange)
    switch (ev.message())
      {
        clap_event_note_t *enote;
      case MidiMessage::NOTE_ON:
      case MidiMessage::NOTE_OFF:
        enote = &input_events_[j++].note;
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
        enote = &input_events_[j++].note;
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
  input_events_.resize (j);
  // for (const auto &ev : input_events_) CDEBUG ("input: %s", clap_event_to_string (&ev.note));
}

void
ClapAudioWrapper::convert_midi1_events (const clap_process_t &process)
{
  MidiEventRange erange = get_event_input();
  if (input_events_.capacity() < erange.events_pending())
    input_events_.reserve (erange.events_pending() + 128);
  input_events_.resize (erange.events_pending());
  uint j = 0;
  for (const auto &ev : erange)
    switch (ev.message())
      {
        clap_event_midi_t *midi1;
      case MidiMessage::NOTE_ON:
      case MidiMessage::NOTE_OFF:
        midi1 = &input_events_[j++].midi1;
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
        midi1 = &input_events_[j++].midi1;
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
  input_events_.resize (j);
}

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
    .request_restart = [] (const clap_host *host) { // FIXME: simplify
      CDEBUG ("%s: host.request_restart", clapid (host));
    },  // deactivate() + activate()
    .request_process = [] (const clap_host *host) {
      CDEBUG ("%s: host.request_process", clapid (host));
    },  // process()
    .request_callback = host_request_callback_mt,
  };
  ClapAudioWrapperP proc_;
  const clap_plugin_t *plugin_ = nullptr;
  const clap_plugin_gui *plugin_gui = nullptr;
  const clap_plugin_params *plugin_params = nullptr;
  const clap_plugin_timer_support *plugin_timer_support = nullptr;
  const clap_plugin_audio_ports_config *plugin_audio_ports_config = nullptr;
  const clap_plugin_audio_ports *plugin_audio_ports = nullptr;
  const clap_plugin_note_ports *plugin_note_ports = nullptr;
  ClapPluginHandleImpl (const ClapPluginDescriptor &descriptor_, AudioProcessorP aproc) :
    ClapPluginHandle (descriptor_), proc_ (shared_ptr_cast<ClapAudioWrapper> (aproc))
  {
    assert_return (proc_ != nullptr);
    const clap_plugin_entry *pluginentry = descriptor.entry();
    if (pluginentry)
      {
        const auto *factory = (const clap_plugin_factory*) pluginentry->get_factory (CLAP_PLUGIN_FACTORY_ID);
        if (factory)
          plugin_ = factory->create_plugin (factory, &phost, clapid().c_str());
      }
  }
  bool
  init_plugin ()
  {
    return_unless (plugin_, false);
    if (!plugin_->init (plugin_)) {
      CDEBUG ("%s: initialization failed", clapid());
      destroy(); // destroy per spec and cleanup resources used by init()
      return false;
    }
    CDEBUG ("%s: initialized", clapid());
    plugin_gui = (const clap_plugin_gui*) plugin_->get_extension (plugin_, CLAP_EXT_GUI);
    plugin_params = (const clap_plugin_params*) plugin_->get_extension (plugin_, CLAP_EXT_PARAMS);
    plugin_timer_support = (const clap_plugin_timer_support*) plugin_->get_extension (plugin_, CLAP_EXT_TIMER_SUPPORT);
    plugin_audio_ports_config = (const clap_plugin_audio_ports_config*) plugin_->get_extension (plugin_, CLAP_EXT_AUDIO_PORTS_CONFIG);
    plugin_audio_ports = (const clap_plugin_audio_ports*) plugin_->get_extension (plugin_, CLAP_EXT_AUDIO_PORTS);
    plugin_note_ports = (const clap_plugin_note_ports*) plugin_->get_extension (plugin_, CLAP_EXT_NOTE_PORTS);
    get_port_infos();
    return true;
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
  activated() const override
  {
    return plugin_activated;
  }
  bool
  activate() override
  {
    return_unless (plugin_ && !activated(), activated());
    plugin_activated = plugin_->activate (plugin_, proc_->engine().sample_rate(), 32, 4096);
    CDEBUG ("%s: %s: %d", clapid(), __func__, plugin_activated);
    if (plugin_activated) {
      ClapPluginHandleImplP selfp = shared_ptr_cast<ClapPluginHandleImpl> (this);
      ScopedSemaphore sem;
      proc_->engine().async_jobs += [&sem, selfp] () {
        selfp->proc_->start_processing();
        sem.post();
      };
      sem.wait();
      // active && processing
    }
    return activated();
  }
  void
  deactivate() override
  {
    return_unless (plugin_ && activated());
    if (true) {
      ClapPluginHandleImplP selfp = shared_ptr_cast<ClapPluginHandleImpl> (this);
      ScopedSemaphore sem;
      proc_->engine().async_jobs += [&sem, selfp] () {
        selfp->proc_->stop_processing();
        sem.post();
      };
      sem.wait();
      // !processing && !active
    }
    plugin_activated = false;
    plugin_->deactivate (plugin_);
    CDEBUG ("%s: deactivated", clapid());
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
  AudioProcessorP
  audio_processor () override
  {
    return proc_;
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
}

// == helpers ==
static const char*
anklang_host_name()
{
  static String name = "Anklang//" + executable_name();
  return name.c_str();
}

static String
feature_canonify (const String &str)
{
  return string_canonify (str, string_set_a2z() + string_set_A2Z() + "-0123456789", "-");
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

static const clap_plugin*
access_clap_plugin (ClapPluginHandle *handle)
{
  ClapPluginHandleImpl *handle_ = dynamic_cast<ClapPluginHandleImpl*> (handle);
  return handle_ ? handle_->plugin_ : nullptr;
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
          const bool scaled = scale > 0 ? plugin_gui->set_scale (plugin_, scale) : false;
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

// == ClapPluginDescriptor ==
ClapPluginDescriptor::ClapPluginDescriptor (ClapFileHandle &clapfile) :
  clapfile_ (clapfile)
{}

void
ClapPluginDescriptor::open() const
{
  clapfile_.open();
}

void
ClapPluginDescriptor::close() const
{
  clapfile_.close();
}

const clap_plugin_entry*
ClapPluginDescriptor::entry() const
{
  return clapfile_.opened() ? clapfile_.pluginentry : nullptr;
}

void
ClapPluginDescriptor::add_descriptor (const String &pluginpath, Collection &infos)
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
      ClapPluginDescriptor *descriptor = new ClapPluginDescriptor (*filehandle);
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

const ClapPluginDescriptor::Collection&
ClapPluginDescriptor::collect_descriptors ()
{
  static Collection collection;
  if (collection.empty()) {
    for (const auto &clapfile : list_clap_files())
      add_descriptor (clapfile, collection);
  }
  return collection;
}

// == ClapFileHandle ==
ClapFileHandle::ClapFileHandle (const String &pathname) :
  dlfile (pathname)
{}

ClapFileHandle::~ClapFileHandle()
{
  assert_return (!opened());
}

String
ClapFileHandle::get_dlerror ()
{
  const char *err = dlerror();
  return err ? err : "unknown dlerror";
}

template<typename Ptr> Ptr
ClapFileHandle::symbol (const char *symname) const
{
  void *p = dlhandle_ ? dlsym (dlhandle_, symname) : nullptr;
  return (Ptr) p;
}

void
ClapFileHandle::close()
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
ClapFileHandle::open()
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

bool
ClapFileHandle::opened() const
{
  return dlhandle_ && pluginentry;
}

// == CLAP utilities ==
StringS
list_clap_files()
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

const char*
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

String
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

DeviceInfo
clap_device_info (const ClapPluginDescriptor &descriptor)
{
  DeviceInfo di;
  di.uri = "CLAP:" + descriptor.id;
  di.name = descriptor.name;
  di.description = descriptor.description;
  di.website_url = descriptor.url;
  di.creator_name = descriptor.vendor;
  di.creator_url = descriptor.manual_url;
  const char *const cfeatures = descriptor.features.c_str();
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
  return di;
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

// == ClapPluginHandle ==
ClapPluginHandle::ClapPluginHandle (const ClapPluginDescriptor &descriptor_) :
  descriptor (descriptor_)
{
  descriptor.open();
}

ClapPluginHandle::~ClapPluginHandle()
{
  descriptor.close();
}

CString
ClapPluginHandle::audio_processor_type()
{
  return clap_audio_wrapper_aseid;
}

ClapPluginHandleP
ClapPluginHandle::make_clap_handle (const ClapPluginDescriptor &descriptor, AudioProcessorP audio_processor)
{
  ClapPluginHandleImplP handlep = std::make_shared<ClapPluginHandleImpl> (descriptor, audio_processor);
  handlep->init_plugin();
  return handlep;
}

} // Ase
