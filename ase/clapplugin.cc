// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clapplugin.hh"
#include "clapdevice.hh"
#include "jsonipc/jsonipc.hh"
#include "storage.hh"
#include "project.hh"
#include "processor.hh"
#include "path.hh"
#include "main.hh"
#include "compress.hh"
#include "serialize.hh"
#include "internal.hh"
#include "gtk2wrap.hh"
#include <clap/ext/draft/file-reference.h>
#include <dlfcn.h>
#include <glob.h>
#include <math.h>

#define CDEBUG(...)          Ase::debug ("clap", __VA_ARGS__)
#define CDEBUG_ENABLED()     Ase::debug_key_enabled ("clap")
#define PDEBUG(...)          Ase::debug ("clapparam", __VA_ARGS__)
#define PDEBUG_ENABLED()     Ase::debug_key_enabled ("clapparam")
#define CLAPEVENT_ENABLED()  Ase::debug_key_enabled ("clapevent")

namespace Ase {

ASE_CLASS_DECLS (ClapAudioProcessor);
ASE_CLASS_DECLS (ClapPluginHandleImpl);
using ClapEventParamS = std::vector<clap_event_param_value>;
union ClapEventUnion;
using ClapEventUnionS = std::vector<ClapEventUnion>;
using ClapResourceHash = std::tuple<clap_id,String>; // resource_id, hex_hash
using ClapResourceHashS = std::vector<ClapResourceHash>;
using ClapParamIdValue = std::tuple<clap_id,double>; // param_id, value
using ClapParamIdValueS = std::vector<ClapParamIdValue>;


// == fwd decls ==
static const char*           anklang_host_name        ();
static String                clapid                   (const clap_host *host);
static ClapPluginHandleImpl* handle_ptr               (const clap_host *host);
static ClapPluginHandleImplP handle_sptr              (const clap_host *host);
static const clap_plugin*    access_clap_plugin       (ClapPluginHandle *handle);
static const void*           host_get_extension_mt    (const clap_host *host, const char *extension_id);
static void                  host_request_restart_mt  (const clap_host *host);
static void                  host_request_process_mt  (const clap_host *host);
static void                  host_request_callback_mt (const clap_host *host);
static bool                  host_unregister_fd       (const clap_host_t *host, int fd);
static bool                  host_unregister_timer    (const clap_host *host, clap_id timer_id);
static bool                  event_unions_try_push    (ClapEventUnionS &events, const clap_event_header_t *event);
static void                  try_load_x11wrapper      ();
static Gtk2DlWrapEntry *x11wrapper = nullptr;
static float scratch_float_buffer[AUDIO_BLOCK_FLOAT_ZEROS_SIZE];

// == ClapFileHandle ==
class ClapFileHandle {
  void *dlhandle_ = nullptr;
  uint  open_count_ = 0;
public:
  const std::string dlfile;
  const clap_plugin_entry *pluginentry = nullptr;
  explicit
  ClapFileHandle (const String &pathname) :
    dlfile (pathname)
  {}
  virtual
  ~ClapFileHandle ()
  {
    assert_return (!opened());
  }
  bool
  opened () const
  {
    return dlhandle_ && pluginentry;
  }
  void
  open ()
  {
    if (open_count_++ == 0 && !dlhandle_) {
      const String dlfile_mem = Path::stringread (dlfile);
      dlhandle_ = dlopen (dlfile.c_str(), RTLD_LOCAL | RTLD_NOW);
      CDEBUG ("%s: dlopen: %s", dlfile, dlhandle_ ? "OK" : get_dlerror());
      if (dlhandle_) {
        pluginentry = symbol<const clap_plugin_entry*> ("clap_entry");
        bool initialized = false;
        if (pluginentry && clap_version_is_compatible (pluginentry->clap_version))
          initialized = pluginentry->init (dlfile.c_str());
        if (!initialized) {
          CDEBUG ("unusable clap_entry: %s", !pluginentry ? "NULL" :
                  string_format ("clap-%u.%u.%u", pluginentry->clap_version.major, pluginentry->clap_version.minor,
                                 pluginentry->clap_version.revision));
          pluginentry = nullptr;
          dlclose (dlhandle_);
          dlhandle_ = nullptr;
        }
      }
    }
  }
  void
  close ()
  {
    assert_return (open_count_ > 0);
    open_count_--;
    return_unless (open_count_ == 0);
    if (pluginentry) {
      pluginentry->deinit();
      pluginentry = nullptr;
    }
    if (dlhandle_) {
      const bool closingok = dlclose (dlhandle_) == 0;
      CDEBUG ("%s: dlclose: %s", dlfile, closingok ? "OK" : get_dlerror());
      dlhandle_ = nullptr;
    }
  }
  template<typename Ptr> Ptr
  symbol (const char *symname) const
  {
    void *p = dlhandle_ ? dlsym (dlhandle_, symname) : nullptr;
    return (Ptr) p;
  }
  static String
  get_dlerror ()
  {
    const char *err = dlerror();
    return err ? err : "unknown dlerror";
  }
};

// == ClapParamInfoImpl ==
struct ClapParamInfoImpl : ClapParamInfo {
  void *cookie_ = nullptr;
  double next_value_ = NAN;
  std::weak_ptr<Property> aseprop_;
  void
  operator= (const clap_param_info &cinfo)
  {
    unset();
    param_id = cinfo.id;
    ident = string_format ("%08x", param_id);
    name = cinfo.name;
    module = cinfo.module;
    min_value = cinfo.min_value;
    max_value = cinfo.max_value;
    default_value = cinfo.default_value;
    cookie_ = cinfo.cookie;
    next_value_ = NAN;
  }
};
using ClapParamInfoMap = std::unordered_map<clap_id,ClapParamInfoImpl*>;

// == ClapParamInfo ==
ClapParamInfo::ClapParamInfo()
{
  unset();
}

void
ClapParamInfo::unset()
{
  param_id = CLAP_INVALID_ID;
  ident = "";
  name = "";
  module = "";
  min_value = NAN;
  max_value = NAN;
  default_value = NAN;
  current_value = NAN;
  min_value_text = "";
  max_value_text = "";
  default_value_text = "";
  current_value_text = "";
}

String
ClapParamInfo::hints_from_param_info_flags (clap_param_info_flags flags)
{
  static constexpr const struct { uint32_t bit; const char *const hint; } bits[] = {
    { CLAP_PARAM_IS_STEPPED,                    "stepped" },
    { CLAP_PARAM_IS_PERIODIC,                   "periodic" },
    { CLAP_PARAM_IS_HIDDEN,                     "hidden" },
    { CLAP_PARAM_IS_READONLY,                   "readonly" },
    { CLAP_PARAM_IS_BYPASS,                     "bypass" },
    { CLAP_PARAM_IS_AUTOMATABLE,                "automatable" },
    { CLAP_PARAM_IS_AUTOMATABLE_PER_NOTE_ID,    "automatable-note-id" },
    { CLAP_PARAM_IS_AUTOMATABLE_PER_KEY,        "automatable-key" },
    { CLAP_PARAM_IS_AUTOMATABLE_PER_CHANNEL,    "automatable-channel" },
    { CLAP_PARAM_IS_AUTOMATABLE_PER_PORT,       "automatable-port" },
    { CLAP_PARAM_IS_MODULATABLE,                "modulatable" },
    { CLAP_PARAM_IS_MODULATABLE_PER_NOTE_ID,    "modulatable-note-id" },
    { CLAP_PARAM_IS_MODULATABLE_PER_KEY,        "modulatable-key" },
    { CLAP_PARAM_IS_MODULATABLE_PER_CHANNEL,    "modulatable-channel" },
    { CLAP_PARAM_IS_MODULATABLE_PER_PORT,       "modulatable-port" },
    { CLAP_PARAM_REQUIRES_PROCESS,              "requires-process" },
  };
  String hints = ":";
  for (size_t i = 0; i < ASE_ARRAY_SIZE (bits); i++)
    if (flags & bits[i].bit)
      hints += bits[i].hint + String (":");
  if (!(flags & CLAP_PARAM_IS_HIDDEN))
    hints += "G:";
  if (flags & CLAP_PARAM_IS_READONLY)
    hints += "r:";
  else
    hints += "r:w:";
  return hints;
}

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

// == ClapAudioProcessor ==
class ClapAudioProcessor : public AudioProcessor {
  ClapPluginHandle *handle_ = nullptr;
  const clap_plugin *clapplugin_ = nullptr;
  IBusId ibusid = {};
  OBusId obusid = {};
  uint imain_clapidx = ~0, omain_clapidx = ~0, iside_clapidx = ~0, oside_clapidx = ~0;
  clap_note_dialect input_event_dialect = clap_note_dialect (0);
  clap_note_dialect input_preferred_dialect = clap_note_dialect (0);
  clap_note_dialect output_event_dialect = clap_note_dialect (0);
  clap_note_dialect output_preferred_dialect = clap_note_dialect (0);
  bool can_process_ = false;
public:
  static void
  static_info (AudioProcessorInfo &info)
  {
    info.label = "Anklang.Devices.ClapAudioProcessor";
  }
  ClapAudioProcessor (const ProcessorSetup &psetup) :
    AudioProcessor (psetup)
  {}
  ~ClapAudioProcessor()
  {
    while (enqueued_events_.size()) {
      ClapEventParamS *pevents = enqueued_events_.back();
      enqueued_events_.pop_back();
      main_rt_jobs += RtCall (call_delete<ClapEventParamS>, pevents); // delete in main_thread
    }
  }
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
    input_preferred_dialect = clap_note_dialect (handle_->note_iport_infos.size() ? handle_->note_iport_infos[0].preferred_dialect : 0);
    output_event_dialect = clap_note_dialect (handle_->note_oport_infos.size() ? handle_->note_oport_infos[0].supported_dialects : 0);
    output_preferred_dialect = clap_note_dialect (handle_->note_oport_infos.size() ? handle_->note_oport_infos[0].preferred_dialect : 0);

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
    if (output_event_dialect & (CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI)) {
      prepare_event_output();
      output_events_.reserve (256); // avoid audio-thread allocations
    }
    enqueued_events_.reserve (8);

    // workaround AudioProcessor asserting that a Processor should have *some* IO facilities
    if (!has_event_output() && !has_event_input() && ibusid == 0 && obusid == 0)
      prepare_event_input();
  }
  void
  reset (uint64 target_stamp) override
  {}
  void convert_clap_events (const clap_process_t &process, bool as_clapnotes);
  std::vector<ClapEventUnion> input_events_;
  std::vector<ClapEventUnion> output_events_;
  static uint32_t
  input_events_size (const clap_input_events *evlist)
  {
    ClapAudioProcessor *self = (ClapAudioProcessor*) evlist->ctx;
    size_t param_events_size = 0;
    for (const auto &pevents_b : self->enqueued_events_)
      param_events_size += pevents_b->size();
    return param_events_size + self->input_events_.size();
  }
  static const clap_event_header_t*
  input_events_get (const clap_input_events *evlist, uint32_t index)
  {
    ClapAudioProcessor *self = (ClapAudioProcessor*) evlist->ctx;
    for (const auto &pevents_b : self->enqueued_events_) {
      if (index < pevents_b->size())
        return &(*pevents_b)[index].header;
      index -= pevents_b->size();
    }
    return index < self->input_events_.size() ? &self->input_events_[index].header : nullptr;
  }
  static bool
  output_events_try_push (const clap_output_events *evlist, const clap_event_header_t *event)
  {
    ClapAudioProcessor *self = (ClapAudioProcessor*) evlist->ctx;
    return event_unions_try_push (self->output_events_, event);
  }
  const clap_input_events_t plugin_input_events = {
    .ctx = (ClapAudioProcessor*) this,
    .size = input_events_size,
    .get = input_events_get,
  };
  const clap_output_events_t plugin_output_events = {
    .ctx = (ClapAudioProcessor*) this,
    .try_push = output_events_try_push,
  };
  const ClapParamInfoMap *param_info_map_ = nullptr;
  const ClapParamInfoImpl *param_info_map_start_ = nullptr;
  clap_process_t processinfo = { 0, };
  clap_event_transport_t transportinfo = { { 0, }, };
  std::vector<ClapEventParamS*> enqueued_events_;
  void
  enqueue_events (ClapEventParamS *pevents)
  {
    // insert 0-time event list *before* other events
    if (pevents && pevents->size() && pevents->back().header.time == 0)
      for (size_t i = 0; i < enqueued_events_.size(); i++)
        if (enqueued_events_[i]->size() && enqueued_events_[i]->back().header.time > 0) {
          enqueued_events_.insert (enqueued_events_.begin() + i, pevents);
          return;
        }
    // or add to existing queue
    enqueued_events_.push_back (pevents);
  }
  bool
  start_processing (const ClapParamInfoMap *param_info_map, const ClapParamInfoImpl *map_start, size_t map_size)
  {
    return_unless (!can_process_, true);
    assert_return (clapplugin_, false);
    param_info_map_ = param_info_map;
    param_info_map_start_ = map_start;
    atomic_bits_resize (map_size);
    can_process_ = clapplugin_->start_processing (clapplugin_);
    CDEBUG ("%s: %s: %d", handle_->clapid(), __func__, can_process_);
    if (can_process_) {
      processinfo = clap_process_t {
        .steady_time = int64_t (engine().frame_counter()),
        .frames_count = 0, .transport = &transportinfo,
        .audio_inputs = &handle_->audio_inputs_[0], .audio_outputs = &handle_->audio_outputs_[0],
        .audio_inputs_count = uint32_t (handle_->audio_inputs_.size()),
        .audio_outputs_count = uint32_t (handle_->audio_outputs_.size()),
        .in_events = &plugin_input_events, .out_events = &plugin_output_events,
      };
      transportinfo = clap_event_transport_t {
        .header = clap_event_header_t {
          .size     = sizeof (clap_event_transport_t),
          .space_id = CLAP_CORE_EVENT_SPACE_ID,
          .type     = CLAP_EVENT_TRANSPORT
        }
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
    param_info_map_ = nullptr;
    param_info_map_start_ = nullptr;
    CDEBUG ("%s: %s", handle_->clapid(), __func__);
    input_events_.resize (0);
    output_events_.resize (0);
  }
  void
  update_transportinfo()
  {
    const auto &trans = transport();
    const auto &tick_sig = trans.tick_sig;
    transportinfo.flags = CLAP_TRANSPORT_HAS_TEMPO |
                          CLAP_TRANSPORT_HAS_BEATS_TIMELINE |
                          CLAP_TRANSPORT_HAS_SECONDS_TIMELINE |
                          CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
    if (trans.running())
      transportinfo.flags |= CLAP_TRANSPORT_IS_PLAYING;
    double sec_pos  = trans.current_seconds + trans.current_minutes * 60;
    double beat_pos = trans.current_tick_d * (1.0 / TRANSPORT_PPQN);
    transportinfo.song_pos_beats      = llrint (beat_pos * CLAP_BEATTIME_FACTOR);
    transportinfo.song_pos_seconds    = llrint (sec_pos * CLAP_SECTIME_FACTOR);
    transportinfo.tempo               = tick_sig.bpm();
    transportinfo.tempo_inc           = 0;
    transportinfo.loop_start_beats    = 0;
    transportinfo.loop_end_beats      = 0;
    transportinfo.loop_start_seconds  = 0;
    transportinfo.loop_end_seconds    = 0;
    double bar_start = trans.current_bar_tick * (1.0 / TRANSPORT_PPQN);
    transportinfo.bar_start           = llrint (bar_start * CLAP_BEATTIME_FACTOR);
    transportinfo.bar_number          = trans.current_bar;
    transportinfo.tsig_num            = tick_sig.beats_per_bar();
    transportinfo.tsig_denom          = tick_sig.beat_unit();
  }
  void
  render (uint n_frames) override
  {
    const uint icount = ibusid != 0 ? this->n_ichannels (ibusid) : 0;
    if (can_process_) {
      update_transportinfo();
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
      convert_clap_events (processinfo, input_preferred_dialect & CLAP_NOTE_DIALECT_CLAP);
      processinfo.steady_time += processinfo.frames_count;
      const clap_process_status status = clapplugin_->process (clapplugin_, &processinfo);
      bool need_wakeup = dequeue_events (n_frames);
      for (const auto &e : output_events_)
        need_wakeup |= apply_param_value_event (e.value);
      output_events_.resize (0);
      if (need_wakeup)
        enotify_enqueue_mt (PARAMCHANGE);
      if (0)
        CDEBUG ("render: status=%d", status);
    }
  }
  bool
  apply_param_value_event (const clap_event_param_value &e)
  {
    bool need_wakeup = false;
    if (e.header.type == CLAP_EVENT_PARAM_VALUE && e.header.size >= sizeof (clap_event_param_value))
      {
        const clap_event_param_value &event = e;
        const auto it = param_info_map_->find (event.param_id);
        if (it != param_info_map_->end())
          {
            ClapParamInfoImpl *pinfo = it->second;
            pinfo->next_value_ = event.value;
            atomic_bit_notify (pinfo - param_info_map_start_);
            need_wakeup = true;
            PDEBUG ("%s: PROCESS: %08x=%f: (%s)\n", clapplugin_->desc->name, pinfo->param_id, event.value, pinfo->name);
          }
      }
    return need_wakeup;
  }
  bool
  dequeue_events (size_t nframes)
  {
    return_unless (enqueued_events_.size(), false);
    // TODO: need proper time stamp handling
    bool need_wakeup = false;
    while (enqueued_events_.size() && (enqueued_events_[0]->empty() || enqueued_events_[0]->back().header.time < nframes)) {
      ClapEventParamS *const pevents = enqueued_events_[0];
      enqueued_events_.erase (enqueued_events_.begin());
      for (const auto &e : *pevents)
        need_wakeup |= apply_param_value_event (e);
      main_rt_jobs += RtCall (call_delete<ClapEventParamS>, pevents); // delete in main_thread
    }
    return need_wakeup;
  }
};
static CString clap_audio_wrapper_aseid = register_audio_processor<ClapAudioProcessor>();

static inline clap_event_midi*
setup_midi1 (ClapEventUnion *evunion, uint32_t time, uint16_t port_index)
{
  clap_event_midi *midi1 = &evunion->midi1;
  midi1->header.size = sizeof (*midi1);
  midi1->header.time = time;
  midi1->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  midi1->header.type = CLAP_EVENT_MIDI;
  midi1->header.flags = 0;
  midi1->port_index = port_index;
  return midi1;
}

static inline clap_event_note*
setup_evnote (ClapEventUnion *evunion, uint32_t time, uint16_t port_index)
{
  clap_event_note *evnote = &evunion->note;
  evnote->header.size = sizeof (*evnote);
  evnote->header.type = 0;
  evnote->header.time = time;
  evnote->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  evnote->header.flags = 0;
  evnote->port_index = port_index;
  return evnote;
}

static inline clap_event_note_expression*
setup_expression (ClapEventUnion *evunion, uint32_t time, uint16_t port_index)
{
  clap_event_note_expression *expr = &evunion->expression;
  expr->header.size = sizeof (*expr);
  expr->header.type = CLAP_EVENT_NOTE_EXPRESSION;
  expr->header.time = time;
  expr->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  expr->header.flags = 0;
  expr->port_index = port_index;
  return expr;
}

void
ClapAudioProcessor::convert_clap_events (const clap_process_t &process, const bool as_clapnotes)
{
  MidiEventInput evinput = midi_event_input();
  if (input_events_.capacity() < evinput.events_pending())
    input_events_.reserve (evinput.events_pending() + 128);
  input_events_.resize (evinput.events_pending());
  uint j = 0;
  for (const auto &ev : evinput)
    switch (ev.message())
      {
        clap_event_note_expression *expr;
        clap_event_note_t *evnote;
        clap_event_midi_t *midi1;
        int16_t i16;
      case MidiMessage::NOTE_ON:
      case MidiMessage::NOTE_OFF:
      case MidiMessage::AFTERTOUCH:
        if (as_clapnotes && ev.type == MidiEvent::AFTERTOUCH) {
          expr = setup_expression (&input_events_[j++], MAX (ev.frame, 0), 0);
          expr->expression_id = CLAP_NOTE_EXPRESSION_PRESSURE;
          expr->note_id = ev.noteid;
          expr->channel = ev.channel;
          expr->key = ev.key;
          expr->value = ev.velocity;
        } else if (as_clapnotes) {
          evnote = setup_evnote (&input_events_[j++], MAX (ev.frame, 0), 0);
          evnote->header.type = ev.type == MidiEvent::NOTE_ON ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF;
          evnote->note_id = ev.noteid;
          evnote->channel = ev.channel;
          evnote->key = ev.key;
          evnote->velocity = ev.velocity;
        } else {
          midi1 = setup_midi1 (&input_events_[j++], MAX (ev.frame, 0), 0);
          midi1->data[0] = uint8_t (ev.type) | (ev.channel & 0xf);
          midi1->data[1] = ev.key;
          midi1->data[2] = std::min (uint8_t (ev.velocity * 127), uint8_t (127));
        }
        break;
      case MidiMessage::ALL_NOTES_OFF:
        if (as_clapnotes) {
          evnote = setup_evnote (&input_events_[j++], MAX (ev.frame, 0), 0);
          evnote->header.type = CLAP_EVENT_NOTE_CHOKE;
          evnote->note_id = -1;
          evnote->channel = -1;
          evnote->key = -1;
          evnote->velocity = 0;
        } else {
          midi1 = setup_midi1 (&input_events_[j++], MAX (ev.frame, 0), 0);
          midi1->data[0] = 0xB0 | (ev.channel & 0xf);
          midi1->data[1] = 123;
          midi1->data[2] = 0;
        }
        break;
      case MidiMessage::CONTROL_CHANGE:
        midi1 = setup_midi1 (&input_events_[j++], MAX (ev.frame, 0), 0);
        midi1->data[0] = 0xB0 | (ev.channel & 0xf);
        midi1->data[1] = ev.param;
        midi1->data[2] = ev.cval;
        break;
      case MidiMessage::CHANNEL_PRESSURE:
        midi1 = setup_midi1 (&input_events_[j++], MAX (ev.frame, 0), 0);
        midi1->data[0] = 0xD0 | (ev.channel & 0xf);
        midi1->data[1] = std::min (uint8_t (ev.velocity * 127), uint8_t (127));
        midi1->data[2] = 0;
        break;
      case MidiMessage::PITCH_BEND:
        midi1 = setup_midi1 (&input_events_[j++], MAX (ev.frame, 0), 0);
        midi1->data[0] = 0xE0 | (ev.channel & 0xf);
        midi1->data[1] = std::min (uint8_t (ev.velocity * 127), uint8_t (127));
        midi1->data[2] = 0;
        i16 = ev.value < 0 ? ev.value * 8192.0 : ev.value * 8191.0;
        i16 += 8192;
        midi1->data[1] = i16 & 127;
        midi1->data[2] = (i16 >> 7) & 127;
        break;
      default: ;
      }
  input_events_.resize (j);
  if (debug_enabled()) // lock-free check
    {
      static bool evdebug = CLAPEVENT_ENABLED();
      if (ASE_UNLIKELY (evdebug))
        for (const auto &ev : input_events_) {
          if (ev.header.type == CLAP_EVENT_MIDI)
            printerr ("%+4d ch=%-2u %-14s %02X %02X %02X sz=%d spc=%d flags=%x port=%d\n",
                      ev.midi1.header.time, ev.midi1.data[0] & 0xf, "MIDI1",
                      ev.midi1.data[0], ev.midi1.data[1], ev.midi1.data[2],
                      ev.midi1.header.size, ev.midi1.header.space_id, ev.midi1.header.flags, ev.midi1.port_index);
          else
            printerr ("%s\n", clap_event_to_string (&ev.note));
        }
    }
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
      const void *ext = host_get_extension_mt (host, extension_id);
      CDEBUG ("%s: host_get_extension_mt(\"%s\"): %p", clapid (host), extension_id, ext);
      return ext;
    },
    .request_restart = host_request_restart_mt,
    .request_process = host_request_process_mt,
    .request_callback = host_request_callback_mt,
  };
  ClapAudioProcessorP proc_;
  const clap_plugin_t *plugin_ = nullptr;
  const clap_plugin_gui *plugin_gui = nullptr;
  const clap_plugin_state *plugin_state = nullptr;
  const clap_plugin_file_reference *plugin_file_reference = nullptr;
  const clap_plugin_params *plugin_params = nullptr;
  const clap_plugin_timer_support *plugin_timer_support = nullptr;
  const clap_plugin_audio_ports_config *plugin_audio_ports_config = nullptr;
  const clap_plugin_audio_ports *plugin_audio_ports = nullptr;
  const clap_plugin_note_ports *plugin_note_ports = nullptr;
  const clap_plugin_posix_fd_support *plugin_posix_fd_support = nullptr;
  ClapPluginHandleImpl (const ClapPluginDescriptor &descriptor_, AudioProcessorP aproc) :
    ClapPluginHandle (descriptor_), proc_ (shared_ptr_cast<ClapAudioProcessor> (aproc))
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
  ~ClapPluginHandleImpl()
  {
    destroy();
    assert_return (!_parent());
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
    auto plugin_get_extension = [this] (const char *extname) {
      const void *ext = plugin_->get_extension (plugin_, extname);
      CDEBUG ("%s: plugin_get_extension(\"%s\"): %p", clapid(), extname, ext);
      return ext;
    };
    plugin_gui = (const clap_plugin_gui*) plugin_get_extension (CLAP_EXT_GUI);
    plugin_params = (const clap_plugin_params*) plugin_get_extension (CLAP_EXT_PARAMS);
    plugin_timer_support = (const clap_plugin_timer_support*) plugin_get_extension (CLAP_EXT_TIMER_SUPPORT);
    plugin_audio_ports_config = (const clap_plugin_audio_ports_config*) plugin_get_extension (CLAP_EXT_AUDIO_PORTS_CONFIG);
    plugin_audio_ports = (const clap_plugin_audio_ports*) plugin_get_extension (CLAP_EXT_AUDIO_PORTS);
    plugin_note_ports = (const clap_plugin_note_ports*) plugin_get_extension (CLAP_EXT_NOTE_PORTS);
    plugin_posix_fd_support = (const clap_plugin_posix_fd_support*) plugin_get_extension (CLAP_EXT_POSIX_FD_SUPPORT);
    plugin_state = (const clap_plugin_state*) plugin_get_extension (CLAP_EXT_STATE);
    plugin_file_reference = (const clap_plugin_file_reference*) plugin_get_extension (CLAP_EXT_FILE_REFERENCE);
    const clap_plugin_render *plugin_render = nullptr;
    plugin_render = (const clap_plugin_render*) plugin_get_extension (CLAP_EXT_RENDER);
    (void) plugin_render;
    const clap_plugin_latency *plugin_latency = nullptr;
    plugin_latency = (const clap_plugin_latency*) plugin_get_extension (CLAP_EXT_LATENCY);
    (void) plugin_latency;
    const clap_plugin_tail *plugin_tail = nullptr;
    plugin_tail = (const clap_plugin_tail*) plugin_get_extension (CLAP_EXT_TAIL);
    (void) plugin_tail;
    get_port_infos();
    return true;
  }
  bool plugin_activated = false;
  bool plugin_processing = false;
  bool gui_visible_ = false;
  bool gui_canresize = false;
  ulong gui_windowid = 0;
  std::vector<uint> timers_;
  struct FdPoll { int fd = -1; uint source = 0; uint flags = 0; };
  std::vector<FdPoll> fd_polls_;
  std::vector<ClapParamInfoImpl> param_infos_;
  ClapParamInfoMap param_ids_;
  ClapParamUpdateS *loader_updates_ = nullptr;
  void get_port_infos ();
  String get_param_value_text (clap_id param_id, double value);
  double get_param_value_double (clap_id param_id, const String &text);
  ClapEventParamS* convert_param_updates (const ClapParamUpdateS &updates);
  void flush_event_params (const ClapEventParamS &events, ClapEventUnionS &output_events);
  void params_changed() override;
  void scan_params();
  void resolve_file_references (ClapResourceHashS loader_hashes);
  ClapParamInfoImpl*
  find_param_info (clap_id clapid)
  {
    auto it = param_ids_.find (clapid);
    return it != param_ids_.end() ? it->second : nullptr;
  }
  ClapParamInfoS
  param_infos () override
  {
    ClapParamInfoS infos;
    for (const auto &pinfo : param_infos_)
      infos.push_back (pinfo);
    return infos;
  }
  bool
  param_set_property (clap_id param_id, PropertyP prop) override
  {
    ClapParamInfoImpl *info = find_param_info (param_id);
    return_unless (info, false);
    info->aseprop_ = prop;
    return true;
  }
  PropertyP
  param_get_property (clap_id param_id) override
  {
    ClapParamInfoImpl *info = find_param_info (param_id);
    return_unless (info, nullptr);
    PropertyP prop = info->aseprop_.lock();
    return prop;
  }
  double
  param_get_value (clap_id param_id, String *text) override
  {
    ClapParamInfoImpl *info = find_param_info (param_id);
    const double v = info ? info->current_value : NAN;
    if (text)
      *text = !info ? "NAN" : get_param_value_text (param_id, v);
    return v;
  }
  bool
  param_set_value (clap_id param_id, const String &stringvalue) override
  {
    ClapParamInfoImpl *info = find_param_info (param_id);
    const double value = info ? get_param_value_double (param_id, stringvalue) : NAN;
    return isnan (value) ? false : param_set_value (param_id, value);
  }
  bool
  param_set_value (clap_id param_id, double v) override
  {
    ClapParamInfoImpl *info = find_param_info (param_id);
    return_unless (info, false);
    return_unless (!(info->flags & CLAP_PARAM_IS_READONLY), false);
    ClapParamUpdateS updates;
    if (info->flags & CLAP_PARAM_IS_STEPPED)
      v = round (v);
    ClapParamUpdate update = {
      .steady_time = 0, // NOW
      .param_id = param_id,
      .value = CLAMP (v, info->min_value, info->max_value),
    };
    updates.push_back (update);
    enqueue_updates (updates);
    return true;
  }
  void
  load_state (WritNode &xs) override
  {
    assert_return (loader_updates_ == nullptr);
    // queue parameter update events
    if (!plugin_state)
      {
        ClapParamIdValueS params;
        xs["param_values"] & params;
        PDEBUG ("%s: LOAD: %s\n", clapid(), json_stringify (params));
        loader_updates_ = new ClapParamUpdateS;
        for (const auto &[id, value] : params)
          loader_updates_->push_back ({
              .steady_time = 0, .param_id = id, .value = value,
            });
        // TODO: flush loader_updates_ right away
      }
    // load saved blob
    if (plugin_state)
      {
        String blobname;
        xs["state_blob"] & blobname;
        StreamReaderP blob = blobname.empty() ? nullptr : _project()->load_blob (blobname);
        const clap_istream istream = {
          .ctx = blob.get(),
          .read = [] (const clap_istream *stream, void *buffer, uint64_t size) -> int64_t {
            StreamReader *sr = (StreamReader*) stream->ctx;
            return sr->read (buffer, size);
          }
        };
        errno = ENOSYS;
        bool ok = !blob ? false : plugin_state->load (plugin_, &istream);
        ok &= !blob ? false : blob->close();
        if (!ok && blobname.size())
          printerr ("%s: blob read error: %s\n", clapid(), strerror (errno ? errno : EIO));
      }
    // update collected files
    ClapResourceHashS loader_hashes;
    xs["resource_hashes"] & loader_hashes;
    resolve_file_references (loader_hashes);
  }
  void
  save_state (WritNode &xs, const String &device_path) override
  {
    // first, flush plugin state to disk
    bool need_save_resources = false;
    if (plugin_file_reference && plugin_file_reference->save_resources)
      need_save_resources = !plugin_file_reference->save_resources (plugin_);
    // store params if plugin_state->save is unimplemented
    if (!plugin_state)
      {
        ClapParamIdValueS params;
        for (const auto &pinfo : param_infos_)
          if (pinfo.param_id != CLAP_INVALID_ID)
            params.push_back ({ pinfo.param_id, pinfo.current_value });
        xs["param_values"] & params;
        PDEBUG ("%s: SAVE: %s\n", clapid(), json_stringify (params));
      }
    // save state into blob file
    if (plugin_state)
      {
        String blobname = string_format ("clap-%s.bin", device_path);
        printerr ("SAVE: blobname: %s\n", blobname);
        const String blobfile = _project()->writer_file_name (blobname) + ".zst";
        StreamWriterP swp = stream_writer_zstd (stream_writer_create_file (blobfile));
        const clap_ostream ostream = {
          .ctx = swp.get(),
          .write = [] (const clap_ostream *stream, const void *buffer, uint64_t size) -> int64_t {
            StreamWriter *sw = (StreamWriter*) stream->ctx;
            return sw->write (buffer, size);
          }
        };
        errno = 0;
        bool ok = plugin_state->save (plugin_, &ostream);
        ok &= swp->close();
        if (!ok) // TODO: user_note
          printerr ("%s: %s: write error: %s\n", clapid(), blobfile, strerror (errno ? errno : EIO));
        // keep state only if size >0
        if (!ok || !Path::check (blobfile, "frs"))
          Path::rmrf (blobfile);
        else
          {
            Error err = _project()->writer_add_file (blobfile);
            if (!!err)
              printerr ("%s: %s: %s\n", program_alias(), blobfile, ase_error_blurb (err));
            else
              xs["state_blob"] & blobname;
          }
      }
    // collect external files
    if (plugin_file_reference && plugin_file_reference->count && plugin_file_reference->get)
      {
        ClapResourceHashS hashes;
        if (need_save_resources) // retry if we got false previously
          plugin_file_reference->save_resources (plugin_);
        const size_t n_files = plugin_file_reference->count (plugin_);
        for (size_t i = 0; i < n_files; i++)
          {
            char buffer[ASE_PATH_MAX + 2] = { 0, };
            const size_t path_capacity = sizeof (buffer) - 1;
            clap_file_reference pfile = {
              .resource_id = CLAP_INVALID_ID,
              .belongs_to_plugin_collection = false,
              .path_capacity = path_capacity,
              .path_size = 0, .path = buffer,
            };
            if (!plugin_file_reference->get (plugin_, i, &pfile) || pfile.path_size < 1 ||
                !pfile.path || pfile.path_size >= path_capacity) // ignore excessive path lengths
              continue;
            pfile.path[pfile.path_size] = 0;
            if (!Path::check (pfile.path, "r"))   // must exist and be readable
              continue;
            ClapResourceHash rhash = { pfile.resource_id, "" };
            Error err = _project()->writer_collect (pfile.path, &std::get<String> (rhash));
            if (!err)
              hashes.push_back (rhash);
            else
              ASE_SERVER.user_note (string_format ("## Note Missing File\n%s: \\\nFailed to store: `%s` \\\n%s",
                                                   clapid(), pfile.path, ase_error_blurb (err)));
          }
        xs["resource_hashes"] & hashes;
      }
  }
  bool
  clap_activated() const override
  {
    return plugin_activated;
  }
  bool
  enqueue_updates (const ClapParamUpdateS &updates)
  {
    ClapPluginHandleImplP selfp = shared_ptr_cast<ClapPluginHandleImpl> (this);
    return_unless (clap_activated(), false);
    ClapEventParamS *pevents = convert_param_updates (updates); // allocated in main_thread
    proc_->engine().async_jobs += [selfp, pevents] () {
      selfp->proc_->enqueue_events (pevents);
    };
    return true;
  }
  bool
  clap_activate() override
  {
    return_unless (plugin_ && !clap_activated(), clap_activated());
    // initial param scan
    if (plugin_params) {
      scan_params(); // needed for convert_param_updates
    }
    // load parameter updates and rescan
    if (plugin_params && loader_updates_) {
      ClapEventParamS *pevents = convert_param_updates (*loader_updates_);
      ClapEventUnionS output_events;
      flush_event_params (*pevents, output_events);
      output_events.clear(); // discard output_events, we just do a rescan
      scan_params();
      delete pevents;
    }
    if (loader_updates_) {
      delete loader_updates_;
      loader_updates_ = nullptr;
    }
    // activate, keeps param_ids_, param_infos_ locked now, start_processing
    plugin_activated = plugin_->activate (plugin_, proc_->engine().sample_rate(), 32, 4096);
    CDEBUG ("%s: %s: %d", clapid(), __func__, plugin_activated);
    if (plugin_activated) {
      ClapPluginHandleImplP selfp = shared_ptr_cast<ClapPluginHandleImpl> (this);
      // synchronize with start_processing
      ScopedSemaphore sem;
      proc_->engine().async_jobs += [&sem, selfp] () {
        selfp->proc_->start_processing (&selfp->param_ids_, &selfp->param_infos_[0], selfp->param_infos_.size());
        sem.post();
      };
      sem.wait();
      // active_state && processing_state
    }
    return clap_activated();
  }
  void
  clap_deactivate() override
  {
    return_unless (plugin_ && clap_activated());
    if (true) {
      ClapPluginHandleImplP selfp = shared_ptr_cast<ClapPluginHandleImpl> (this);
      ScopedSemaphore sem;
      proc_->engine().async_jobs += [&sem, selfp] () {
        selfp->proc_->stop_processing();
        sem.post();
      };
      sem.wait();
      // NOW: !processing && !clap_activated
    }
    plugin_activated = false;
    plugin_->deactivate (plugin_);
    CDEBUG ("%s: plugin->deactivated", clapid());
  }
  void show_gui     () override;
  void hide_gui     () override;
  void destroy_gui  () override;
  bool gui_visible  () override;
  bool supports_gui () override;
  void
  destroy () override
  {
    destroy_gui();
    if (plugin_) {
      if (clap_activated())
        clap_deactivate();
      CDEBUG ("%s: destroying", clapid());
    }
    param_ids_.clear();
    param_infos_.clear();
    while (fd_polls_.size())
      host_unregister_fd (&phost, fd_polls_.back().fd);
    while (timers_.size())
      host_unregister_timer (&phost, timers_.back());
    if (plugin_)
      plugin_->destroy (plugin_);
    plugin_ = nullptr;
    plugin_gui = nullptr;
    plugin_state = nullptr;
    plugin_file_reference = nullptr;
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

// == get_param_value_text ==
String
ClapPluginHandleImpl::get_param_value_text (clap_id param_id, double value)
{
  constexpr uint LEN = 256;
  char buffer[LEN + 1] = { 0 };
  if (!plugin_params || !plugin_params->value_to_text (plugin_, param_id, value, buffer, LEN))
    buffer[0] = 0;
  buffer[LEN] = 0;
  return buffer;
}

double
ClapPluginHandleImpl::get_param_value_double (clap_id param_id, const String &text)
{
  double value = NAN;
  if (!plugin_params || !plugin_params->text_to_value ||
      !plugin_params->text_to_value (plugin_, param_id, text.c_str(), &value))
    value = NAN;
  return value;
}

// == scan_params ==
void
ClapPluginHandleImpl::scan_params()
{
  param_ids_.clear();
  param_infos_.clear();
  const uint32_t count = plugin_params->count (plugin_);
  param_infos_.resize (count);
  param_ids_.reserve (param_infos_.size());
  for (size_t i = 0; i < param_infos_.size(); i++) {
    ClapParamInfoImpl &pinfo = param_infos_[i];
    pinfo.unset();
    clap_param_info cinfo = { CLAP_INVALID_ID, 0, nullptr, {0}, {0}, NAN, NAN, NAN };
    if (plugin_params->get_info (plugin_, i, &cinfo) && cinfo.id != CLAP_INVALID_ID) {
      pinfo = cinfo;
      param_ids_[cinfo.id] = &param_infos_[i];
    }
  }
  for (size_t i = 0; i < param_infos_.size(); i++) {
    ClapParamInfoImpl &pinfo = param_infos_[i];
    if (pinfo.param_id == CLAP_INVALID_ID)
      continue;
    pinfo.min_value_text = get_param_value_text (pinfo.param_id, pinfo.min_value);
    pinfo.max_value_text = get_param_value_text (pinfo.param_id, pinfo.max_value);
    pinfo.default_value_text = get_param_value_text (pinfo.param_id, pinfo.default_value);
    if (!plugin_params->get_value (plugin_, pinfo.param_id, &pinfo.current_value)) {
      pinfo.current_value = pinfo.default_value;
      pinfo.current_value_text = pinfo.default_value_text;
    } else
      pinfo.current_value_text = get_param_value_text (pinfo.param_id, pinfo.current_value);
    PDEBUG ("%s: SCAN: %08x=%f: %s (%s)\n", clapid(), pinfo.param_id, pinfo.current_value, pinfo.current_value_text, pinfo.name);
  }
}

// == params_changed ==
void
ClapPluginHandleImpl::params_changed ()
{
  for (AtomicBits::Iter it = proc_->atomic_bits_iter(); it.valid(); it.big_inc1())
    if (it.clear())
      {
        ClapParamInfoImpl &pinfo = param_infos_[it.position()];
        if (ASE_ISLIKELY (isnan (pinfo.next_value_)) || pinfo.param_id == CLAP_INVALID_ID)
          continue;
        pinfo.current_value = pinfo.next_value_;
        pinfo.next_value_ = NAN;
        pinfo.current_value_text = get_param_value_text (pinfo.param_id, pinfo.current_value);
        PDEBUG ("%s: UPDATE: %08x=%f: %s (%s)\n", clapid(), pinfo.param_id, pinfo.current_value, pinfo.current_value_text, pinfo.name);
        PropertyP prop = pinfo.aseprop_.lock();
        if (prop)
          dynamic_cast<EmittableImpl*> (prop.get())->emit_notify (pinfo.ident);
      }
}

// == convert_param_updates ==
ClapEventParamS*
ClapPluginHandleImpl::convert_param_updates (const ClapParamUpdateS &updates)
{
  ClapEventParamS *param_events = new ClapEventParamS();
  for (size_t i = 0; i < updates.size(); i++)
    {
      const ClapParamInfoImpl *pinfo = find_param_info (updates[i].param_id);
      if (!pinfo)
        continue;
      const clap_event_param_value event = {
        .header = {
          .size = sizeof (clap_event_param_value),
          .time = 0, // updates[i].steady_time
          .space_id = CLAP_CORE_EVENT_SPACE_ID,
          .type = CLAP_EVENT_PARAM_VALUE,
          .flags = CLAP_EVENT_DONT_RECORD,
        },
        .param_id = pinfo->param_id,
        .cookie = pinfo->cookie_,
        .note_id = -1,
        .port_index = -1,
        .channel = -1,
        .key = -1,
        .value = updates[i].value
      };
      param_events->push_back (event);
      PDEBUG ("%s: CONVERT: %08x=%f: (%s)\n", clapid(), pinfo->param_id, event.value, pinfo->name);
    }
  return param_events;
}

// == flush_event_params ==
void
ClapPluginHandleImpl::flush_event_params (const ClapEventParamS &inevents, ClapEventUnionS &outevents)
{
  const clap_output_events output_events = {
    .ctx = &outevents,
    .try_push = [] (const clap_output_events *list, const clap_event_header_t *event) {
      ClapEventUnionS *outevents = (ClapEventUnionS*) list->ctx;
      return event_unions_try_push (*outevents, event);
    }
  };
  const clap_input_events input_events = {
    .ctx = const_cast<ClapEventParamS*> (&inevents),
    .size = [] (const struct clap_input_events *list) {
      const ClapEventParamS *inevents = (const ClapEventParamS*) list->ctx;
      return uint32_t (inevents->size());
    },
    .get = [] (const clap_input_events *list, uint32_t index) -> const clap_event_header_t* {
      const ClapEventParamS *inevents = (const ClapEventParamS*) list->ctx;
      return index < inevents->size() ? &(*inevents)[index].header : nullptr;
    }
  };
  if (PDEBUG_ENABLED())
    for (const auto &p : inevents)
      PDEBUG ("%s: FLUSH: %08x=%f\n", clapid(), p.param_id, p.value);
  plugin_params->flush (plugin_, &input_events, &output_events);
}

// == get_port_infos ==
void
ClapPluginHandleImpl::get_port_infos()
{
  assert_return (!clap_activated());
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

static bool
event_unions_try_push (ClapEventUnionS &events, const clap_event_header_t *event)
{
  const size_t esize = event->size;
  if (esize <= sizeof (events[0]))
    {
      events.resize (events.size() + 1);
      memcpy (&events.back(), event, esize);
      return true;
    }
  return false;
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
  handlep->timers_.push_back (*timeridp);
  CDEBUG ("%s: %s: ms=%u: id=%u", clapid (host), __func__, period_ms, *timer_id);
  return true;
}

static bool
host_unregister_timer (const clap_host *host, clap_id timer_id)
{
  // NOTE: plugin_ might be destroying here
  ClapPluginHandleImpl *handle = handle_ptr (host);
  const bool deleted = Aux::erase_first (handle->timers_, [timer_id] (uint id) { return id == timer_id; });
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
  // FIXME: implement host_rescan, this is a shorthand for all-params-changed
}

static const clap_host_audio_ports host_ext_audio_ports = {
  .is_rescan_flag_supported = host_is_rescan_flag_supported,
  .rescan = host_rescan,
};

// == clap_host_posix_fd_support ==
static bool
host_register_fd (const clap_host_t *host, int fd, clap_posix_fd_flags_t flags)
{
  ClapPluginHandleImplP handlep = handle_sptr (host);
  auto plugin_on_fd = [handlep] (PollFD &pfd) {
    uint revents = 0;
    revents |= bool (pfd.revents & PollFD::IN) * CLAP_POSIX_FD_READ;
    revents |= bool (pfd.revents & PollFD::OUT) * CLAP_POSIX_FD_WRITE;
    revents |= bool (pfd.revents & (PollFD::ERR | PollFD::HUP | PollFD::NVAL)) * CLAP_POSIX_FD_ERROR;
    if (0)
      CDEBUG ("%s: plugin_on_fd: fd=%d revents=%u: %u", handlep->clapid(), pfd.fd, revents,
              handlep->plugin_posix_fd_support && handlep->plugin_posix_fd_support->on_fd);
    if (handlep->plugin_posix_fd_support)
      handlep->plugin_posix_fd_support->on_fd (handlep->plugin_, pfd.fd, revents);
    return true; // keep alive
  };
  String mode;
  if (flags & CLAP_POSIX_FD_READ)
    mode += "r";
  if (flags & CLAP_POSIX_FD_WRITE)
    mode += "w";
  ClapPluginHandleImpl::FdPoll fdpoll = {
    .fd = fd,
    .source = main_loop->exec_io_handler (plugin_on_fd, fd, mode),
    .flags = flags,
  };
  if (fdpoll.source)
    handlep->fd_polls_.push_back (fdpoll);
  CDEBUG ("%s: %s: fd=%d flags=%u mode=\"%s\" (nfds=%u): id=%u", clapid (host), __func__, fd, flags, mode,
          handlep->fd_polls_.size(), fdpoll.source);
  return fdpoll.source != 0;
}

static bool
host_unregister_fd (const clap_host_t *host, int fd)
{
  ClapPluginHandleImplP handlep = handle_sptr (host);
  for (size_t i = 0; i < handlep->fd_polls_.size(); i++)
    if (handlep->fd_polls_[i].fd == fd) {
      const bool deleted = main_loop->try_remove (handlep->fd_polls_[i].source);
      CDEBUG ("%s: %s: fd=%d id=%u (nfds=%u): deleted=%u", clapid (host), __func__, fd, handlep->fd_polls_[i].source,
              handlep->fd_polls_.size(), deleted);
      handlep->fd_polls_.erase (handlep->fd_polls_.begin() + i);
      return true;
    }
  CDEBUG ("%s: %s: fd=%d: deleted=0", clapid (host), __func__, fd);
  return false;
}

static bool
host_modify_fd (const clap_host_t *host, int fd, clap_posix_fd_flags_t flags)
{
  ClapPluginHandleImplP handlep = handle_sptr (host);
  for (size_t i = 0; i < handlep->fd_polls_.size(); i++)
    if (handlep->fd_polls_[i].fd == fd) {
      CDEBUG ("%s: %s: fd=%d flags=%u", clapid (host), __func__, fd, flags);
      if (handlep->fd_polls_[i].flags == flags)
        return true;
      // the following calls invalidate fd_polls_
      host_unregister_fd (host, fd);
      return host_register_fd (host, fd, flags);
    }
  CDEBUG ("%s: %s: fd=%d flags=%u: false", clapid (host), __func__, fd, flags);
  return false;
}

static const clap_host_posix_fd_support host_ext_posix_fd_support = {
  .register_fd = host_register_fd,
  .modify_fd = host_modify_fd,
  .unregister_fd = host_unregister_fd,
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

bool
ClapPluginHandleImpl::gui_visible ()
{
  return gui_visible_;
}

bool
ClapPluginHandleImpl::supports_gui ()
{
  return plugin_gui != nullptr;
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
          CDEBUG ("%s: gui_set_scale(%f): %d\n", clapid(), scale, scaled);
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
    gui_visible_ = plugin_gui->show (plugin_);
    CDEBUG ("%s: gui_show: %d\n", clapid(), gui_visible_);
    if (!gui_visible_)
      {} // do nothing, early JUCE versions have a bug returning false here
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
      gui_visible_ = false;
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
  handle->gui_visible_ = false;
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

// == clap_host_file_reference ==
void
ClapPluginHandleImpl::resolve_file_references (ClapResourceHashS loader_hashes)
{
  if (!plugin_file_reference || !plugin_file_reference->update_path ||
      !plugin_file_reference->count || !plugin_file_reference->get)
    return;
  const size_t n_files = plugin_file_reference->count (plugin_);
  for (size_t i = 0; i < n_files; i++)
    {
      char buffer[ASE_PATH_MAX + 2] = { 0, };
      const size_t path_capacity = sizeof (buffer) - 1;
      clap_file_reference pfile = {
        .resource_id = CLAP_INVALID_ID,
        .belongs_to_plugin_collection = false,
        .path_capacity = path_capacity,
        .path_size = 0, .path = buffer,
      };
      if (!plugin_file_reference->get (plugin_, i, &pfile) ||
          pfile.path_size >= path_capacity) // ignore excessive path lengths
        continue;
      if (pfile.path && Path::check (pfile.path, "e"))
        continue;       // path exists, nothing to resolve
      String hash;
      uint8_t digest[32] = { 0, };
      // fetch hash from plugin or cache
      if (plugin_file_reference->get_blake3_digest &&
          plugin_file_reference->get_blake3_digest (plugin_, pfile.resource_id, digest))
        hash = string_to_hex (String ((const char*) digest, sizeof (digest)));
      else
        for (size_t i = 0; i < loader_hashes.size(); i++)
          if (std::get<clap_id> (loader_hashes[i]) == pfile.resource_id)
            {
              hash = std::get<String> (loader_hashes[i]);
              break;
            }
      // resolve or warn user
      String content_file = hash.empty() ? "" : _project()->loader_resolve (hash);
      if (content_file.size())
        plugin_file_reference->update_path (plugin_, pfile.resource_id, content_file.c_str());
      else
        {
          String what = string_format ("id=%04x", pfile.resource_id);
          if (!hash.empty())
            what += " hash=" + hash;
          if (pfile.path && pfile.path[0])
            what += String (" ") + pfile.path;
          ASE_SERVER.user_note (string_format ("## Note Missing File\n%s: \\\nFailed to find file: \\\n%s",
                                               clapid(), what));
        }
    }
}

static void
host_file_reference_changed (const clap_host_t *host)
{
  CDEBUG ("%s: %s", clapid (host), __func__);
}

static void
host_file_reference_set_dirty (const clap_host_t *host, clap_id resource_id)
{
  CDEBUG ("%s: %s: %d", clapid (host), __func__, resource_id);
}

static const clap_host_file_reference host_ext_file_reference = {
  .changed = host_file_reference_changed,
  .set_dirty = host_file_reference_set_dirty,
};

// == clap_host extensions ==
static const void*
host_get_extension_mt (const clap_host *host, const char *extension_id)
{
  const String ext = extension_id;
  if (ext == CLAP_EXT_LOG)              return &host_ext_log;
  if (ext == CLAP_EXT_GUI)              return &host_ext_gui;
  if (ext == CLAP_EXT_FILE_REFERENCE)   return &host_ext_file_reference;
  if (ext == CLAP_EXT_TIMER_SUPPORT)    return &host_ext_timer_support;
  if (ext == CLAP_EXT_THREAD_CHECK)     return &host_ext_thread_check;
  if (ext == CLAP_EXT_AUDIO_PORTS)      return &host_ext_audio_ports;
  if (ext == CLAP_EXT_PARAMS)           return &host_ext_params;
  if (ext == CLAP_EXT_POSIX_FD_SUPPORT) return &host_ext_posix_fd_support;
  else return nullptr;
}

static void
host_request_restart_mt (const clap_host *host)
{
  CDEBUG ("%s: %s", clapid (host), __func__);
}

static void
host_request_process_mt (const clap_host *host)
{
  CDEBUG ("%s: %s", clapid (host), __func__);
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
  if (x11wrapper)
    TaskRegistry::set_gtk_thread_id (x11wrapper->gtk_thread_id());
}

Gtk2DlWrapEntry *
get_x11wrapper()
{
  try_load_x11wrapper();
  return x11wrapper;
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
