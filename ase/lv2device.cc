// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "processor.hh"
#include "main.hh"
#include "internal.hh"
#include "strings.hh"
#include "loft.hh"
#include "serialize.hh"
#include "storage.hh"
#include "project.hh"
#include "path.hh"

// X11 wrapper
#include "clapplugin.hh"

#include "lv2/atom/atom.h"
#include "lv2/midi/midi.h"
#include "lv2/options/options.h"
#include "lv2/parameters/parameters.h"
#include "lv2/buf-size/buf-size.h"
#include "lv2/worker/worker.h"
#include "lv2/presets/presets.h"
#include "lv2/data-access/data-access.h"
#include "lv2/instance-access/instance-access.h"
#include "lv2/ui/ui.h"

#include "lv2externalui.hh"

#include "lv2evbuf.hh"
#include "lv2device.hh"

#include <lilv/lilv.h>

namespace Ase {

namespace
{

static Gtk2DlWrapEntry *x11wrapper = nullptr;

#define NS_EXT "http://lv2plug.in/ns/ext/"

using std::vector;
using std::string;
using std::map;
using std::max;
using std::min;

class ControlEvent
{
  LoftPtr<ControlEvent>       loft_ptr_;    // keep this object alive
  uint32_t                    port_index_;
  uint32_t                    protocol_;
  size_t                      size_;
  LoftPtr<void>               data_;

public:
  std::atomic<ControlEvent *> next_ = nullptr;

  static ControlEvent *
  loft_new (uint32_t port_index, uint32_t protocol, size_t size, const void *data = nullptr)
  {
    LoftPtr<ControlEvent> loft_ptr = loft_make_unique<ControlEvent>();
    ControlEvent *new_event = loft_ptr.get();
    new_event->loft_ptr_ = std::move (loft_ptr);
    new_event->port_index_ = port_index;
    new_event->protocol_ = protocol;
    new_event->size_ = size;
    new_event->data_ = loft_alloc (size);
    if (data)
      memcpy (new_event->data_.get(), data, size);
    return new_event;
  }
  void
  loft_free()
  {
    loft_ptr_.reset(); // do not access this after this line
  }
  uint32_t port_index() const { return port_index_; }
  uint32_t protocol() const   { return protocol_; }
  size_t   size() const       { return size_; }
  uint8_t *data() const       { return reinterpret_cast<uint8_t *> (data_.get()); }
};

class ControlEventVector
{
  AtomicIntrusiveStack<ControlEvent> events_;
public:
  template<typename Func>
  void
  for_each (ControlEventVector& trash_events, Func func)
  {
    ControlEvent *const events = events_.pop_reversed(), *last = nullptr;
    for (ControlEvent *event = events; event; last = event, event = event->next_)
      {
        const ControlEvent *cevent = event;
        func (cevent);
      }
    if (last)
      trash_events.events_.push_chain (events, last);
  }
  void
  free_all()
  {
    ControlEvent *event = events_.pop_all();
    while (event)
      {
        ControlEvent *old = event;
        event = event->next_;
        old->loft_free();
      }
  }
  void
  push (ControlEvent *event)
  {
    events_.push (event);
  }
  ~ControlEventVector()
  {
    free_all();
  }
};

static inline std::atomic<ControlEvent*>&
atomic_next_ptrref (ControlEvent *event)
{
  return event->next_;
}

class Map
{
  std::mutex            map_mutex;
  LV2_URID              next_id;
  map<string, LV2_URID> m_urid_map;
  map<LV2_URID, String> m_urid_unmap;

  LV2_URID_Map       lv2_urid_map;
  const LV2_Feature  lv2_urid_map_feature;
  LV2_URID_Unmap     lv2_urid_unmap;
  const LV2_Feature  lv2_urid_unmap_feature;
public:
  Map() :
    next_id (1),
    lv2_urid_map { this, urid_map },
    lv2_urid_map_feature { LV2_URID_MAP_URI, &lv2_urid_map },
    lv2_urid_unmap { this, urid_unmap },
    lv2_urid_unmap_feature { LV2_URID_UNMAP_URI, &lv2_urid_unmap }
  {
  }

  static LV2_URID
  urid_map (LV2_URID_Map_Handle handle, const char *str)
  {
    return static_cast<Map *> (handle)->urid_map (str);
  }
  static const char *
  urid_unmap (LV2_URID_Unmap_Handle handle, LV2_URID id)
  {
    return static_cast<Map *> (handle)->urid_unmap (id);
  }

  LV2_URID
  urid_map (const char *str)
  {
    std::lock_guard lg (map_mutex);

    LV2_URID& id = m_urid_map[str];
    if (id == 0)
      id = next_id++;

    m_urid_unmap[id] = str;
    printerr ("map %s -> %d\n", str, id);
    return id;
  }
  const char *
  urid_unmap (LV2_URID id)
  {
    std::lock_guard lg (map_mutex);

    auto it = m_urid_unmap.find (id);
    if (it != m_urid_unmap.end())
      return it->second.c_str();
    else
      return nullptr;
  }

  const LV2_Feature *
  map_feature() const
  {
    return &lv2_urid_map_feature;
  }
  const LV2_Feature *
  unmap_feature() const
  {
    return &lv2_urid_unmap_feature;
  }
  LV2_URID_Map *
  lv2_map()
  {
    return &lv2_urid_map;
  }
  LV2_URID_Unmap *
  lv2_unmap()
  {
    return &lv2_urid_unmap;
  }
};

class PluginHost;

class Options
{
  PluginHost& plugin_host;
  float       m_sample_rate;
  uint32_t    m_block_length;

  vector<LV2_Options_Option> const_opts;

  LV2_Feature  lv2_options_feature;
public:
  Options (PluginHost& plugin_host);
  void
  set (float sample_rate, int32_t block_length)
  {
    m_sample_rate   = sample_rate;
    m_block_length  = block_length;
  }
  const LV2_Feature *
  feature() const
  {
    return &lv2_options_feature;
  }
};

class Worker
{
  LV2_Worker_Schedule lv2_worker_sched;
  const LV2_Feature   lv2_worker_feature;

  const LV2_Worker_Interface *worker_interface = nullptr;
  LV2_Handle                  instance = nullptr;
  ControlEventVector          work_events_, response_events_, trash_events_;
  std::thread                 thread_;
  std::atomic<int>            quit_;
  ScopedSemaphore             sem_;
public:
  Worker() :
    lv2_worker_sched { this, schedule },
    lv2_worker_feature { LV2_WORKER__schedule, &lv2_worker_sched },
    quit_ (0)
  {
    thread_ = std::thread (&Worker::run, this);
  }
  void
  stop()
  {
    quit_ = 1;
    sem_.post();
    thread_.join();
    printerr ("worker thread joined\n");
  }

  void
  set_instance (LilvInstance *lilv_instance)
  {
    instance = lilv_instance_get_handle (lilv_instance);

    const LV2_Descriptor *descriptor = lilv_instance_get_descriptor (lilv_instance);
    if (descriptor && descriptor->extension_data)
       worker_interface = (const LV2_Worker_Interface *) (*descriptor->extension_data) (LV2_WORKER__interface);
  }

  void
  run()
  {
    while (!quit_)
      {
        sem_.wait();
        work_events_.for_each (trash_events_,
          [this] (const ControlEvent *event)
            {
              printerr ("got work %zd bytes\n", event->size());
              worker_interface->work (instance, respond, this, event->size(), event->data());
            });
        // free both: old worker events and old response events
        trash_events_.free_all();
      }
  }
  LV2_Worker_Status
  schedule (uint32_t size, const void *data)
  {
    if (!worker_interface)
      return LV2_WORKER_ERR_UNKNOWN;

    work_events_.push (ControlEvent::loft_new (0, 0, size, data));
    sem_.post();
    return LV2_WORKER_SUCCESS;
  }
  LV2_Worker_Status
  respond (uint32_t size, const void *data)
  {
    if (!worker_interface)
      return LV2_WORKER_ERR_UNKNOWN;

    printerr ("queue work response\n");
    response_events_.push (ControlEvent::loft_new (0, 0, size, data));
    return LV2_WORKER_SUCCESS;
  }
  void
  handle_responses()
  {
    response_events_.for_each (trash_events_,
      [this] (const ControlEvent *event)
        {
          printerr ("got work response %zd bytes\n", event->size());
          worker_interface->work_response (instance, event->size(), event->data());
        });
  }
  void
  end_run()
  {
    /* to be called after each run cycle */
    if (worker_interface && worker_interface->end_run)
      worker_interface->end_run (instance);
  }
  static LV2_Worker_Status
  schedule (LV2_Worker_Schedule_Handle handle,
            uint32_t                   size,
            const void*                data)
  {
    Worker *worker = static_cast<Worker *> (handle);
    return worker->schedule (size, data);
  }
  static LV2_Worker_Status
  respond  (LV2_Worker_Respond_Handle handle,
            uint32_t                  size,
            const void*               data)
  {
    Worker *worker = static_cast<Worker *> (handle);
    return worker->respond (size, data);
  }

  const LV2_Feature *
  feature() const
  {
    return &lv2_worker_feature;
  }
};


class Features
{
  std::vector<LV2_Feature> features;
  std::vector<const LV2_Feature *> null_terminated_ptrs;
public:
  const LV2_Feature * const*
  get_features()
  {
    assert_return (null_terminated_ptrs.empty(), nullptr);
    for (const auto& f : features)
      null_terminated_ptrs.push_back (&f);
    null_terminated_ptrs.push_back (nullptr);

    return null_terminated_ptrs.data();
  }
  void
  add (const LV2_Feature *lv2_feature)
  {
    assert (null_terminated_ptrs.empty());
    features.push_back (*lv2_feature);
  }
  void
  add (const char *uri, void *data)
  {
    features.emplace_back (uri, data);
  }
};

struct Port
{
  LV2_Evbuf  *evbuf;
  float       control;    /* for control ports */
  float       min_value;  /* min control */
  float       max_value;  /* max control */
  int         control_in_idx = -1; /* for control input ports */
  String      name;
  String      symbol;

  enum {
    UNKNOWN,
    CONTROL_IN,
    CONTROL_OUT
  }           type;

  Port() :
    evbuf (nullptr),
    control (0.0),
    type (UNKNOWN)
  {
  }
};

struct PresetInfo
{
  String          name;
  const LilvNode *preset = nullptr;
};

struct PluginUI;

struct PluginInstance
{
  PluginHost& plugin_host;
  std::unique_ptr<PluginUI>  plugin_ui;
  std::atomic<bool>          plugin_ui_is_active { false };

  LV2_Extension_Data_Feature lv2_ext_data;

  Features features;

  Worker   worker;

  PluginInstance (PluginHost& plugin_host);
  ~PluginInstance();

  const LilvPlugin             *plugin = nullptr;
  LilvInstance                 *instance = nullptr;
  const LV2_Worker_Interface   *worker_interface = nullptr;
  uint                          sample_rate = 0;
  std::vector<Port>             plugin_ports;
  std::vector<int>              atom_out_ports;
  std::vector<int>              atom_in_ports;
  std::vector<int>              audio_in_ports;
  std::vector<int>              audio_out_ports;
  std::vector<PresetInfo>       presets;
  bool                          active = false;
  std::function<void(int, float)> control_in_changed_callback;
  uint32_t                      ui_update_frame_count = 0;
  static constexpr double       ui_update_fps = 60;

  ControlEventVector            ui2dsp_events_, dsp2ui_events_, trash_events_;

  void init_ports();
  void init_presets();
  void reset_event_buffers();
  void write_midi (uint32_t time, size_t size, const uint8_t *data);
  void connect_audio_port (uint32_t port, float *buffer);
  void run (uint32_t n_frames);
  void activate();
  void deactivate();

  const LilvUI *get_plugin_ui();
  void toggle_ui();
  void delete_ui_request();
  void send_plugin_events_to_ui();
  void handle_dsp2ui_events();
  void send_ui_updates (uint32_t delta_frames);
  void set_initial_controls_ui();
};

void
host_ui_write (void          *controller,
               uint32_t       port_index,
               uint32_t       buffer_size,
               uint32_t       protocol,
               const void*    buffer)
{
  PluginInstance *plugin_instance = (PluginInstance *) controller;
  ControlEvent *event = ControlEvent::loft_new (port_index, protocol, buffer_size, buffer);
  plugin_instance->ui2dsp_events_.push (event);
}

// TODO: do we need to implement this function?
uint32_t
host_ui_index (void *controller, const char* symbol)
{
  printerr ("LV2: host_ui_index %s called but not implemented\n", symbol);
  return 0;
}

struct PluginHost
{
  LilvWorld   *world = nullptr;
  Map          urid_map;
  void        *suil_host;

  struct URIDs {
    LV2_URID param_sampleRate;
    LV2_URID atom_Double;
    LV2_URID atom_Float;
    LV2_URID atom_Int;
    LV2_URID atom_Long;
    LV2_URID atom_eventTransfer;
    LV2_URID bufsz_maxBlockLength;
    LV2_URID bufsz_minBlockLength;
    LV2_URID midi_MidiEvent;

    URIDs (Map& map) :
      param_sampleRate          (map.urid_map (LV2_PARAMETERS__sampleRate)),
      atom_Double               (map.urid_map (LV2_ATOM__Double)),
      atom_Float                (map.urid_map (LV2_ATOM__Float)),
      atom_Int                  (map.urid_map (LV2_ATOM__Int)),
      atom_Long                 (map.urid_map (LV2_ATOM__Long)),
      atom_eventTransfer        (map.urid_map (LV2_ATOM__eventTransfer)),
      bufsz_maxBlockLength      (map.urid_map (LV2_BUF_SIZE__maxBlockLength)),
      bufsz_minBlockLength      (map.urid_map (LV2_BUF_SIZE__minBlockLength)),
      midi_MidiEvent            (map.urid_map (LV2_MIDI__MidiEvent))
    {
    }
  } urids;

  struct Nodes {
    LilvNode *lv2_audio_class;
    LilvNode *lv2_atom_class;
    LilvNode *lv2_input_class;
    LilvNode *lv2_output_class;
    LilvNode *lv2_control_class;

    LilvNode *lv2_atom_Chunk;
    LilvNode *lv2_atom_Sequence;
    LilvNode *lv2_presets_Preset;

    LilvNode *lv2_ui_external;
    LilvNode *lv2_ui_externalkx;
    LilvNode *lv2_ui_fixedSize;
    LilvNode *lv2_ui_noUserResize;
    LilvNode *lv2_optionalFeature;
    LilvNode *lv2_requiredFeature;

    LilvNode *rdfs_label;

    void init (LilvWorld *world)
    {
      lv2_audio_class   = lilv_new_uri (world, LILV_URI_AUDIO_PORT);
      lv2_atom_class    = lilv_new_uri (world, LILV_URI_ATOM_PORT);
      lv2_input_class   = lilv_new_uri (world, LILV_URI_INPUT_PORT);
      lv2_output_class  = lilv_new_uri (world, LILV_URI_OUTPUT_PORT);
      lv2_control_class = lilv_new_uri (world, LILV_URI_CONTROL_PORT);

      lv2_atom_Chunk    = lilv_new_uri (world, LV2_ATOM__Chunk);
      lv2_atom_Sequence = lilv_new_uri (world, LV2_ATOM__Sequence);

      lv2_ui_external     = lilv_new_uri(world, "http://lv2plug.in/ns/extensions/ui#external");
      lv2_ui_externalkx   = lilv_new_uri(world, "http://kxstudio.sf.net/ns/lv2ext/external-ui#Widget");
      lv2_ui_fixedSize    = lilv_new_uri (world, LV2_UI__fixedSize);
      lv2_ui_noUserResize = lilv_new_uri (world, LV2_UI__noUserResize);

      lv2_optionalFeature = lilv_new_uri (world, LV2_CORE__optionalFeature);
      lv2_requiredFeature = lilv_new_uri (world, LV2_CORE__requiredFeature);

      lv2_presets_Preset = lilv_new_uri (world, LV2_PRESETS__Preset);
      rdfs_label         = lilv_new_uri (world, LILV_NS_RDFS "label");
    }
  } nodes;

  Options  options;

private:
  PluginHost() :
    urids (urid_map),
    options (*this)
  {
    if (!x11wrapper)
      x11wrapper = get_x11wrapper();
    if (x11wrapper)
      {
        suil_host = x11wrapper->create_suil_host (host_ui_write, host_ui_index);
        // TODO: free suil_host when done
      }

    world = lilv_world_new();
    lilv_world_load_all (world);

    nodes.init (world);
  }
public:
  static PluginHost&
  the()
  {
    static PluginHost host;
    return host;
  }
  PluginInstance *instantiate (const char *plugin_uri, uint sample_rate);

private:
  DeviceInfoS devs;
  map<string, DeviceInfo> lv2_device_info_map;
public:

  DeviceInfo
  lv2_device_info (const string& uri)
  {
    if (devs.empty())
      list_plugins();

    return lv2_device_info_map[uri];
  }

  DeviceInfoS
  list_plugins()
  {
    if (!devs.empty())
      return devs;

    const LilvPlugins* plugins = lilv_world_get_all_plugins (world);
    LILV_FOREACH(plugins, i, plugins)
      {
        const LilvPlugin* p = lilv_plugins_get (plugins, i);
        DeviceInfo device_info;
        string lv2_uri = lilv_node_as_uri (lilv_plugin_get_uri (p));
        device_info.uri = "LV2:" + lv2_uri;

        LilvNode* n = lilv_plugin_get_name (p);
        device_info.name = lilv_node_as_string (n);
        lilv_node_free (n);

        auto plugin_class = lilv_plugin_get_class (p);
        device_info.category = string_format ("LV2 %s", lilv_node_as_string (lilv_plugin_class_get_label (plugin_class)));

        devs.push_back (device_info);

        lv2_device_info_map[lv2_uri] = device_info;
      }
    std::stable_sort (devs.begin(), devs.end(), [] (auto& d1, auto& d2) { return string_casecmp (d1.name, d2.name) < 0; });
    return devs;
  }
};

static const LilvNode *ui_type; // FIXME: not static

class PluginUI
{
  bool init_ok_ = false;
  bool ui_is_visible_ = false;
  bool external_ui_ = false;
  struct lv2_external_ui_host external_ui_host_;
  lv2_external_ui *external_ui_widget_ = nullptr;

  bool ui_is_resizable (const LilvUI *ui);
public:
  const LV2UI_Idle_Interface *idle_iface_ = nullptr;
  LV2UI_Handle               handle_ = nullptr;
  void                      *window_ = nullptr;
  uint                       timer_id_ = 0;
  PluginInstance *plugin_instance_ = nullptr;
  void *ui_instance_ = nullptr;

  PluginUI (PluginInstance *plugin_instance, const string& plugin_uri, const LilvUI *ui);
  ~PluginUI();
  bool init_ok() const { return init_ok_; }
};

PluginUI::PluginUI (PluginInstance *plugin_instance, const string& plugin_uri,
                    const LilvUI *ui)
{
  plugin_instance_ = plugin_instance;

  external_ui_ = lilv_ui_is_a (ui, plugin_instance_->plugin_host.nodes.lv2_ui_external) ||
                 lilv_ui_is_a (ui, plugin_instance_->plugin_host.nodes.lv2_ui_externalkx);

  string window_title = PluginHost::the().lv2_device_info (plugin_uri).name;

  const char* bundle_uri  = lilv_node_as_uri (lilv_ui_get_bundle_uri (ui));
  const char* binary_uri  = lilv_node_as_uri (lilv_ui_get_binary_uri (ui));
  char*       bundle_path = lilv_file_uri_parse (bundle_uri, nullptr);
  char*       binary_path = lilv_file_uri_parse (binary_uri, nullptr);

  Features ui_features;
  ui_features.add (LV2_INSTANCE_ACCESS_URI, lilv_instance_get_handle (plugin_instance->instance));
  ui_features.add (LV2_DATA_ACCESS_URI, &plugin_instance->lv2_ext_data);
  ui_features.add (plugin_instance->plugin_host.urid_map.map_feature());
  ui_features.add (plugin_instance->plugin_host.urid_map.unmap_feature());
  ui_features.add (plugin_instance->plugin_host.options.feature()); /* TODO: maybe make a local version */

  if (external_ui_)
    {
      external_ui_host_.ui_closed = [] (LV2UI_Controller controller)
        {
          PluginInstance *plugin_instance = (PluginInstance *) controller;
          if (plugin_instance->plugin_ui)
            {
              plugin_instance->plugin_ui->ui_is_visible_ = false; /* don't want to pass dsp events to ui if it has been closed */
              main_loop->exec_callback ([plugin_instance]() { plugin_instance->delete_ui_request(); });
            }
        };
      external_ui_host_.plugin_human_id = strdup (window_title.c_str());

      LV2_Feature external_ui_feature (LV2_EXTERNAL_UI_URI, &external_ui_host_);
      LV2_Feature external_kxui_feature (LV2_EXTERNAL_UI_KX__Host, &external_ui_host_);

      ui_features.add (&external_kxui_feature);
      ui_features.add (&external_ui_feature);
    }
  else
    {
      window_ = x11wrapper->create_suil_window (window_title, ui_is_resizable (ui),
        [this] ()
          {
            ui_is_visible_ = false; /* don't want to pass dsp events to ui if it has been closed */
            main_loop->exec_callback ([this]() { plugin_instance_->delete_ui_request(); });
          });
      ui_features.add (LV2_UI__parent, window_);
    }

  /* enable DSP->UI notifications - we need to do this before creating the instance because
   * the newly created instance and the DSP code can already start to communicate while
   * the rest of the UI initialization is still being performed
   */
  plugin_instance->plugin_ui_is_active.store (true);

  string ui_uri = lilv_node_as_uri (lilv_ui_get_uri (ui));
  string container_ui_uri = external_ui_ ? lilv_node_as_uri (ui_type) : "http://lv2plug.in/ns/extensions/ui#GtkUI";
  ui_instance_ = x11wrapper->create_suil_instance (PluginHost::the().suil_host,
                                                   plugin_instance,
                                                   container_ui_uri,
                                                   plugin_uri,
                                                   ui_uri,
                                                   lilv_node_as_uri (ui_type),
                                                   bundle_path,
                                                   binary_path,
                                                   ui_features.get_features());
  if (!ui_instance_)
    {
      printerr ("LV2: ui for plugin %s could not be created\n", plugin_uri);
      return;
    }
  if (external_ui_)
    {
      x11wrapper->exec_in_gtk_thread (
        [&]()
          {
            external_ui_widget_ = (lv2_external_ui *) x11wrapper->get_suil_widget_gtk_thread (ui_instance_);
            external_ui_widget_->show (external_ui_widget_);
          });
    }
  else
    {
      x11wrapper->add_suil_widget_to_window (window_, ui_instance_);
    }
  ui_is_visible_ = true;

  int period_ms = 1000. / plugin_instance->ui_update_fps;
  timer_id_ = x11wrapper->register_timer (
    [this]
      {
        if (ui_is_visible_)
          plugin_instance_->handle_dsp2ui_events();
        if (external_ui_ && external_ui_widget_)
          external_ui_widget_->run (external_ui_widget_);
        return true;
      },
    period_ms);

  plugin_instance->set_initial_controls_ui();

  init_ok_ = true;
}

bool
PluginUI::ui_is_resizable (const LilvUI *ui)
{
  auto& host = plugin_instance_->plugin_host;

  const LilvNode *s = lilv_ui_get_uri (ui);
  bool fixed_size = lilv_world_ask (host.world, s, host.nodes.lv2_optionalFeature, host.nodes.lv2_ui_fixedSize) ||
                    lilv_world_ask (host.world, s, host.nodes.lv2_optionalFeature, host.nodes.lv2_ui_noUserResize);
  return !fixed_size;
}

PluginUI::~PluginUI()
{
  // disable DSP->UI notifications
  plugin_instance_->plugin_ui_is_active.store (false);

  if (window_)
    {
      x11wrapper->destroy_suil_window (window_);
      window_ = nullptr;
    }
  if (ui_instance_)
    {
      x11wrapper->destroy_suil_instance (ui_instance_);
      ui_instance_ = nullptr;
    }
  if (timer_id_)
    {
      x11wrapper->remove_timer (timer_id_);
      timer_id_ = 0;
    }
}

Options::Options (PluginHost& plugin_host) :
  plugin_host (plugin_host),
  lv2_options_feature { LV2_OPTIONS__options, nullptr }
{
  const_opts.push_back ({ LV2_OPTIONS_INSTANCE, 0, plugin_host.urids.param_sampleRate,
                          sizeof(float), plugin_host.urids.atom_Float, &m_sample_rate });
  const_opts.push_back ({ LV2_OPTIONS_INSTANCE, 0, plugin_host.urids.bufsz_minBlockLength,
                          sizeof(int32_t), plugin_host.urids.atom_Int, &m_block_length });
  const_opts.push_back ({ LV2_OPTIONS_INSTANCE, 0, plugin_host.urids.bufsz_maxBlockLength,
                          sizeof(int32_t), plugin_host.urids.atom_Int, &m_block_length });
  const_opts.push_back ({ LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr });

  lv2_options_feature.data = const_opts.data();
}

PluginInstance *
PluginHost::instantiate (const char *plugin_uri, uint sample_rate)
{
  LilvNode* uri = lilv_new_uri (world, plugin_uri);
  if (!uri)
    {
      printerr ("Invalid plugin URI <%s>\n", plugin_uri);
      return nullptr;
    }
  if (!x11wrapper)
    {
      printerr ("LV2: cannot instantiate plugin: missing x11wrapper\n");
      return nullptr;
    }

  const LilvPlugins* plugins = lilv_world_get_all_plugins (world);

  const LilvPlugin*  plugin  = lilv_plugins_get_by_uri (plugins, uri);

  if (!plugin)
    {
      printerr ("plugin is nil\n");
      return nullptr;
    }
  lilv_node_free (uri);

  PluginInstance *plugin_instance = new PluginInstance (*this);


  LilvInstance *instance = nullptr;
  x11wrapper->exec_in_gtk_thread ([&]() {
    instance = lilv_plugin_instantiate (plugin, sample_rate, plugin_instance->features.get_features());
  });
  if (!instance)
    {
      printerr ("plugin instantiate failed\n");
      delete plugin_instance;

      return nullptr;
    }

  plugin_instance->sample_rate = sample_rate;
  plugin_instance->instance = instance;
  plugin_instance->plugin = plugin;
  plugin_instance->init_ports();
  plugin_instance->init_presets();
  plugin_instance->worker.set_instance (instance);
  plugin_instance->lv2_ext_data.data_access = lilv_instance_get_descriptor (instance)->extension_data;

  return plugin_instance;
}

PluginInstance::PluginInstance (PluginHost& plugin_host) :
  plugin_host (plugin_host)
{
  features.add (plugin_host.urid_map.map_feature());
  features.add (plugin_host.urid_map.unmap_feature());
  features.add (worker.feature());
  features.add (plugin_host.options.feature()); /* TODO: maybe make a local version */
}

PluginInstance::~PluginInstance()
{
  worker.stop();

  if (instance)
    {
      if (active)
        deactivate();

      if (instance)
        {
          x11wrapper->exec_in_gtk_thread ([&]() {
            lilv_instance_free (instance);
          });
          instance = nullptr;
        }
    }
}

void
PluginInstance::init_ports()
{
  const int n_ports = lilv_plugin_get_num_ports (plugin);

  // don't resize later, otherwise control connections get lost
  plugin_ports.resize (n_ports);

  vector<float> defaults (n_ports);
  vector<float> min_values (n_ports);
  vector<float> max_values (n_ports);

  size_t n_control_ports = 0;

  lilv_plugin_get_port_ranges_float (plugin, min_values.data(), max_values.data(), defaults.data());
  for (int i = 0; i < n_ports; i++)
    {
      const LilvPort *port = lilv_plugin_get_port_by_index (plugin, i);
      if (port)
        {
          if (lilv_port_is_a (plugin, port, plugin_host.nodes.lv2_input_class))
            {
              if (lilv_port_is_a (plugin, port, plugin_host.nodes.lv2_audio_class))
                {
                  audio_in_ports.push_back (i);
                }
              else if (lilv_port_is_a (plugin, port, plugin_host.nodes.lv2_atom_class))
                {
                  /* TODO: need to support explicit port size from .ttl */
                  const int buf_size = 4096;
                  plugin_ports[i].evbuf = lv2_evbuf_new (buf_size, LV2_EVBUF_ATOM, plugin_host.urid_map.urid_map (lilv_node_as_string (plugin_host.nodes.lv2_atom_Chunk)),
                                                                                   plugin_host.urid_map.urid_map (lilv_node_as_string (plugin_host.nodes.lv2_atom_Sequence)));
                  lilv_instance_connect_port (instance, i, lv2_evbuf_get_buffer (plugin_ports[i].evbuf));

                  atom_in_ports.push_back (i);
                }
              else if (lilv_port_is_a (plugin, port, plugin_host.nodes.lv2_control_class))
                {
                  plugin_ports[i].control = defaults[i];      // start with default value
                  plugin_ports[i].type = Port::CONTROL_IN;
                  plugin_ports[i].min_value = min_values[i];
                  plugin_ports[i].max_value = max_values[i];

                  LilvNode *nname = lilv_port_get_name (plugin, port);
                  plugin_ports[i].name = lilv_node_as_string (nname);
                  lilv_node_free (nname);

                  const LilvNode *nsymbol = lilv_port_get_symbol (plugin, port);
                  plugin_ports[i].symbol = lilv_node_as_string (nsymbol);

                  lilv_instance_connect_port (instance, i, &plugin_ports[i].control);

                  plugin_ports[i].control_in_idx = n_control_ports;

                  n_control_ports++;
                }
              else
                {
                  printerr ("found unknown input port\n");
                }
            }
          if (lilv_port_is_a (plugin, port, plugin_host.nodes.lv2_output_class))
            {
              if (lilv_port_is_a (plugin, port, plugin_host.nodes.lv2_audio_class))
                {
                  audio_out_ports.push_back (i);
                }
              else if (lilv_port_is_a (plugin, port, plugin_host.nodes.lv2_atom_class))
                {
                  atom_out_ports.push_back (i);

                  printerr ("found atom output port\n");
                  const int buf_size = 4096;
                  plugin_ports[i].evbuf = lv2_evbuf_new (buf_size, LV2_EVBUF_ATOM, plugin_host.urid_map.urid_map (lilv_node_as_string (plugin_host.nodes.lv2_atom_Chunk)),
                                                                                   plugin_host.urid_map.urid_map (lilv_node_as_string (plugin_host.nodes.lv2_atom_Sequence)));
                  lilv_instance_connect_port (instance, i, lv2_evbuf_get_buffer (plugin_ports[i].evbuf));

                }
              else if (lilv_port_is_a (plugin, port, plugin_host.nodes.lv2_control_class))
                {
                  plugin_ports[i].control = defaults[i];      // start with default value
                  plugin_ports[i].type = Port::CONTROL_OUT;

                  lilv_instance_connect_port (instance, i, &plugin_ports[i].control);
                }
              else
                {
                  printerr ("found unknown output port\n");
                }
            }
        }
    }

  printerr ("--------------------------------------------------\n");
  printerr ("audio IN:%zd OUT:%zd\n", audio_in_ports.size(), audio_out_ports.size());
  printerr ("control IN:%zd\n", n_control_ports);
  printerr ("--------------------------------------------------\n");
}

void
PluginInstance::init_presets()
{
  LilvNodes* lilv_presets = lilv_plugin_get_related (plugin, plugin_host.nodes.lv2_presets_Preset);
  LILV_FOREACH (nodes, i, lilv_presets)
    {
      const LilvNode* preset = lilv_nodes_get (lilv_presets, i);
      lilv_world_load_resource (plugin_host.world, preset);
      LilvNodes* labels = lilv_world_find_nodes (plugin_host.world, preset, plugin_host.nodes.rdfs_label, NULL);
      if (labels)
        {
          const LilvNode* label = lilv_nodes_get_first (labels);
          presets.push_back ({lilv_node_as_string (label), lilv_node_duplicate (preset)}); // TODO: preset leak
          lilv_nodes_free (labels);
        }
    }
  lilv_nodes_free (lilv_presets);
}

void
PluginInstance::write_midi (uint32_t time, size_t size, const uint8_t *data)
{
  if (!atom_in_ports.empty())
    {
      /* TODO: we use the first atom in port for midi, is there a better strategy? */
      int p = atom_in_ports[0];

      LV2_Evbuf           *evbuf = plugin_ports[p].evbuf;
      LV2_Evbuf_Iterator    iter = lv2_evbuf_end (evbuf);

      lv2_evbuf_write (&iter, time, 0, plugin_host.urids.midi_MidiEvent, size, data);
    }
}

void
PluginInstance::reset_event_buffers()
{
  for (int p : atom_out_ports)
    {
      /* Clear event output for plugin to write to */
      LV2_Evbuf *evbuf = plugin_ports[p].evbuf;

      lv2_evbuf_reset (evbuf, false);
    }
  for (int p : atom_in_ports)
    {
      LV2_Evbuf *evbuf = plugin_ports[p].evbuf;

      lv2_evbuf_reset (evbuf, true);
    }
}

void
PluginInstance::activate()
{
  if (!active)
    {
      x11wrapper->exec_in_gtk_thread ([&]() {
        printerr ("activate\n");
        lilv_instance_activate (instance);
      });

      active = true;
    }
}

void
PluginInstance::deactivate()
{
  if (active)
    {
      x11wrapper->exec_in_gtk_thread ([&]() {
        printerr ("deactivate\n");
        lilv_instance_deactivate (instance);
      });

      active = false;
    }
}

void
PluginInstance::connect_audio_port (uint32_t port, float *buffer)
{
  lilv_instance_connect_port (instance, port, buffer);
}

void
PluginInstance::run (uint32_t n_frames)
{
  ui2dsp_events_.for_each (trash_events_,
    [&] (const ControlEvent *event)
      {
        assert (event->port_index() < plugin_ports.size());
        Port* port = &plugin_ports[event->port_index()];
        if (event->protocol() == 0)
          {
            assert (event->size() == sizeof (float));
            port->control = *(float *)event->data();

            control_in_changed_callback (port->control_in_idx, port->control);
          }
        else if (event->protocol() == plugin_host.urids.atom_eventTransfer)
          {
            LV2_Evbuf_Iterator    e    = lv2_evbuf_end (port->evbuf);
            const LV2_Atom* const atom = (const LV2_Atom *) event->data();
            lv2_evbuf_write (&e, n_frames, 0, atom->type, atom->size, (const uint8_t*)LV2_ATOM_BODY_CONST(atom));
          }
        else
          {
            printerr ("LV2: PluginInstance: protocol: %d not implemented\n", event->protocol());
          }
      });

  lilv_instance_run (instance, n_frames);

  worker.handle_responses();
  worker.end_run();

  if (plugin_ui_is_active)
    {
      send_plugin_events_to_ui();
      send_ui_updates (n_frames);
    }
}

void
PluginInstance::send_plugin_events_to_ui()
{
  for (int port_index : atom_out_ports)
    {
      LV2_Evbuf *evbuf = plugin_ports[port_index].evbuf;

      for (LV2_Evbuf_Iterator i = lv2_evbuf_begin (evbuf); lv2_evbuf_is_valid (i); i = lv2_evbuf_next (i))
        {
          uint32_t frames, subframes, type, size;
          uint8_t *body;
          lv2_evbuf_get (i, &frames, &subframes, &type, &size, &body);

          ControlEvent *event = ControlEvent::loft_new (port_index, plugin_host.urids.atom_eventTransfer, sizeof (LV2_Atom) + size);

          LV2_Atom *atom = (LV2_Atom *) event->data();
          atom->type = type;
          atom->size = size;

          memcpy (event->data() + sizeof (LV2_Atom), body, size);
          // printerr ("send_plugin_events_to_ui: push event: index=%zd, protocol=%d, sz=%zd\n", event->port_index, event->protocol, event->data.size());
          dsp2ui_events_.push (event);
        }
    }
}

void
PluginInstance::handle_dsp2ui_events()
{
  assert_return (this_thread_is_gtk());
  dsp2ui_events_.for_each (trash_events_,
    [&] (const ControlEvent *event)
      {
        assert (event->port_index() < plugin_ports.size());
        // printerr ("handle_dsp2ui_events: pop event: index=%zd, protocol=%d, sz=%zd\n", event->port_index, event->protocol, event->data.size());
        if (plugin_ui)
          x11wrapper->suil_instance_port_event_gtk_thread (plugin_ui->ui_instance_, event->port_index(), event->size(), event->protocol(), event->data());
      });
  // free both: old dsp2ui events and old ui2dsp events
  trash_events_.free_all();
}

void
PluginInstance::set_initial_controls_ui()
{
  /* Set initial control values on UI */
  for (size_t port_index = 0; port_index < plugin_ports.size(); port_index++)
    {
      const auto& port = plugin_ports[port_index];

      if (port.type == Port::CONTROL_IN || port.type == Port::CONTROL_OUT)
        {
          ControlEvent *event = ControlEvent::loft_new (port_index, 0, sizeof (float), &port.control);
          dsp2ui_events_.push (event);
        }
    }
}

void
PluginInstance::send_ui_updates (uint32_t delta_frames)
{
  ui_update_frame_count += delta_frames;

  uint update_n_frames = sample_rate / ui_update_fps;
  if (ui_update_frame_count >= update_n_frames)
    {
      ui_update_frame_count -= update_n_frames;
      if (ui_update_frame_count > update_n_frames)
        {
          /* corner case: if block size is very large, we simply need to update every time */
          ui_update_frame_count = update_n_frames;
        }
      for (size_t port_index = 0; port_index < plugin_ports.size(); port_index++)
        {
          const Port& port = plugin_ports[port_index];

          if (port.type == Port::CONTROL_OUT)
            {
              ControlEvent *event = ControlEvent::loft_new (port_index, 0, sizeof (float), &port.control);
              dsp2ui_events_.push (event);
            }
        }
    }
}

const LilvUI *
PluginInstance::get_plugin_ui()
{
  static LilvUIs* uis;            ///< All plugin UIs (RDF data) FIXME not static

  uis = lilv_plugin_get_uis (plugin);

  const LilvNode* native_ui_type = lilv_new_uri (plugin_host.world, "http://lv2plug.in/ns/extensions/ui#GtkUI");
  LILV_FOREACH(uis, u, uis)
    {
      const LilvUI* this_ui = lilv_uis_get (uis, u);

      if (lilv_ui_is_supported (this_ui,
                                [](const char *host_type_uri, const char *ui_type_uri)
                                  {
                                    return x11wrapper->suil_ui_supported (host_type_uri, ui_type_uri);
                                  },
                                native_ui_type,
                               &ui_type))
        return this_ui;
    }
  // if no suil supported UI is available try external UI
  LILV_FOREACH(uis, u, uis)
    {
      const LilvUI* this_ui = lilv_uis_get (uis, u);

      if (lilv_ui_is_a (this_ui, plugin_host.nodes.lv2_ui_externalkx))
        {
          ui_type = plugin_host.nodes.lv2_ui_externalkx;
          return this_ui;
        }
      if (lilv_ui_is_a (this_ui, plugin_host.nodes.lv2_ui_external))
        {
          ui_type = plugin_host.nodes.lv2_ui_external;
          return this_ui;
        }
    }
  return nullptr;
}

void
PluginInstance::toggle_ui()
{
  if (plugin_ui) // ui already opened? -> close!
    {
      plugin_ui.reset();
      return;
    }
  // ---------------------ui------------------------------
  const LilvUI *ui = get_plugin_ui();
  const char* plugin_uri  = lilv_node_as_uri (lilv_plugin_get_uri (plugin));

  plugin_ui = std::make_unique <PluginUI> (this, plugin_uri, ui);

  // if UI could not be created (for whatever reason) reset pointer to nullptr to free stuff and avoid crashes
  if (!plugin_ui->init_ok())
    plugin_ui.reset();
}

void
PluginInstance::delete_ui_request()
{
  plugin_ui.reset();
}

// TODO: may want to use this for preset loading as well
struct PortRestoreHelper
{
  PluginHost& plugin_host;
  map<string, double> values;

  PortRestoreHelper (PluginHost& host) :
    plugin_host (host)
  {
  }

  static void
  set (const char*         port_symbol,
       void*               user_data,
       const void*         value,
       uint32_t            size,
       uint32_t            type)
  {
    auto &port_restore = *(PortRestoreHelper *) user_data;
    auto &plugin_host = port_restore.plugin_host;

    double dvalue = 0;
    if (type == plugin_host.urids.atom_Float)
      {
        dvalue = *(const float*)value;
      }
    else if (type == plugin_host.urids.atom_Double)
      {
        dvalue = *(const double*)value;
      }
    else if (type == plugin_host.urids.atom_Int)
      {
        dvalue = *(const int32_t*)value;
      }
    else if (type == plugin_host.urids.atom_Long)
      {
        dvalue = *(const int64_t*)value;
      }
    else
      {
        printerr ("error: port restore symbol `%s' value has bad type <%s>\n", port_symbol, plugin_host.urid_map.urid_unmap (type));
        return;
      }
    port_restore.values[port_symbol] = dvalue;
  }
};

}

class LV2Processor : public AudioProcessor {
  IBusId stereo_in_;
  OBusId stereo_out_;
  vector<IBusId> mono_ins_;
  vector<OBusId> mono_outs_;

  PluginInstance *plugin_instance;
  PluginHost& plugin_host;

  vector<Port *> param_id_port;
  int current_preset = 0;

  enum
    {
      PID_PRESET         = 1,
      PID_CONTROL_OFFSET = 10
    };

  string lv2_uri_;

  void
  initialize (SpeakerArrangement busses) override
  {
    plugin_host.options.set (sample_rate(), AUDIO_BLOCK_MAX_RENDER_SIZE);
    plugin_instance = plugin_host.instantiate (lv2_uri_.c_str(), sample_rate());
    if (!plugin_instance)
      return;

    ParameterMap pmap;

    if (plugin_instance->presets.size()) /* choice with 1 entry will crash */
      {
        ChoiceS centries;
        int preset_num = 0;
        centries += { "0", "-none-" };
        for (auto preset : plugin_instance->presets)
          centries += { string_format ("%d", ++preset_num), preset.name };
        pmap[PID_PRESET] = Param { "device_preset", "Device Preset", "Preset", 0, "", std::move (centries), GUIONLY, "Device Preset to be used" };
      }
    current_preset = 0;

    for (auto& port : plugin_instance->plugin_ports)
      if (port.type == Port::CONTROL_IN)
        {
          // TODO: lv2 port numbers are not reliable for serialization, should use port.symbol instead
          // TODO: special case boolean, enumeration, logarithmic,... controls
          int pid = PID_CONTROL_OFFSET + port.control_in_idx;
          pmap[pid] = Param { port.symbol, port.name, "", port.control, "", { port.min_value, port.max_value }, GUIONLY };
          param_id_port.push_back (&port);
        }

    // call if parameters are changed using the LV2 custom UI during render
    plugin_instance->control_in_changed_callback = [this] (int id, float value) {
      set_param_from_render (id + PID_CONTROL_OFFSET, value);
    };

    // TODO: deactivate?
    // TODO: is this the right place?
    plugin_instance->activate();

    install_params (pmap);

    prepare_event_input();

    /* map audio inputs/outputs to busses;
     *
     *   channels == 1 -> one mono bus
     *   channels == 2 -> one stereo bus
     *   channels >= 3 -> N mono busses (TODO: is this the best mapping for all plugins?)
     */
    mono_ins_.clear();
    mono_outs_.clear();
    if (plugin_instance->audio_in_ports.size() == 2)
      {
        stereo_in_ = add_input_bus ("Stereo In", SpeakerArrangement::STEREO);
        assert_return (bus_info (stereo_in_).ident == "stereo_in");
      }
    else
      {
        for (size_t i = 0; i < plugin_instance->audio_in_ports.size(); i++)
          mono_ins_.push_back (add_input_bus (string_format ("Mono In %zd", i + 1), SpeakerArrangement::MONO));
      }

    if (plugin_instance->audio_out_ports.size() == 2)
      {
        stereo_out_ = add_output_bus ("Stereo Out", SpeakerArrangement::STEREO);
        assert_return (bus_info (stereo_out_).ident == "stereo_out");
      }
    else
      {
        for (size_t i = 0; i < plugin_instance->audio_out_ports.size(); i++)
          mono_outs_.push_back (add_output_bus (string_format ("Mono Out %zd", i + 1), SpeakerArrangement::MONO));
      }
  }
  void
  reset (uint64 target_stamp) override
  {
    if (!plugin_instance)
      return;

    adjust_all_params();
  }
  void
  adjust_param (uint32_t tag) override
  {
    if (!plugin_instance)
      return;

    // controls for LV2Device
    if (int (tag) == PID_PRESET)
      {
        int want_preset = irintf (get_param (tag));
        if (current_preset != want_preset)
          {
            current_preset = want_preset;

            if (want_preset > 0 && want_preset <= int (plugin_instance->presets.size()))
              {
                // TODO: this should not be done in audio thread

                auto preset_info = plugin_instance->presets[want_preset - 1];
                printerr ("load preset %s\n", preset_info.name.c_str());
                LilvState *state = lilv_state_new_from_world (plugin_host.world, plugin_host.urid_map.lv2_map(), preset_info.preset);
                const LV2_Feature* state_features[] = { // TODO: more features
                  plugin_host.urid_map.map_feature(),
                  plugin_host.urid_map.unmap_feature(),
                  NULL
                };
                lilv_state_restore (state, plugin_instance->instance, set_port_value, this, 0, state_features);
              }
          }
      }

    // real LV2 controls start at PID_CONTROL_OFFSET
    auto control_id = tag - PID_CONTROL_OFFSET;
    if (control_id >= 0 && control_id < param_id_port.size())
      {
        Port *port = param_id_port[control_id];
        port->control = get_param (tag);

        ControlEvent *event = ControlEvent::loft_new (port->control_in_idx, 0, sizeof (float), &port->control);
        plugin_instance->dsp2ui_events_.push (event);
      }
  }
  void
  render (uint n_frames) override
  {
    if (!plugin_instance)
      {
        if (plugin_instance->audio_out_ports.size() == 2)
          {
            floatfill (oblock (stereo_out_, 0), 0.f, n_frames);
            floatfill (oblock (stereo_out_, 1), 0.f, n_frames);
          }
        else
          {
            for (size_t i = 0; i < plugin_instance->audio_out_ports.size(); i++)
              floatfill (oblock (mono_outs_[i], 0), 0.f, n_frames);
          }
        return;
      }

    // reset event buffers and write midi events
    plugin_instance->reset_event_buffers();

    MidiEventInput evinput = midi_event_input();
    for (const auto &ev : evinput)
      {
        const int time_stamp = std::max<int> (ev.frame, 0);
        uint8_t midi_data[3] = { 0, };

        switch (ev.message())
          {
          case MidiMessage::NOTE_OFF:
            midi_data[0] = 0x80 | ev.channel;
            midi_data[1] = ev.key;
            plugin_instance->write_midi (time_stamp, 3, midi_data);
            break;
          case MidiMessage::NOTE_ON:
            midi_data[0] = 0x90 | ev.channel;
            midi_data[1] = ev.key;
            midi_data[2] = std::clamp (irintf (ev.velocity * 127), 0, 127);
            plugin_instance->write_midi (time_stamp, 3, midi_data);
            break;
#if 0
          case Message::ALL_NOTES_OFF:
          case Message::ALL_SOUND_OFF:
            synth_.all_sound_off();    // NOTE: there is no extra "all notes off" in liquidsfz
            break;
#endif
          case MidiMessage::PARAM_VALUE:
            apply_event (ev);
            adjust_param (ev.param);
            break;
          default: ;
          }
      }

    if (plugin_instance->audio_in_ports.size() == 2)
      {
        plugin_instance->connect_audio_port (plugin_instance->audio_in_ports[0], const_cast<float *> (ifloats (stereo_in_, 0)));
        plugin_instance->connect_audio_port (plugin_instance->audio_in_ports[1], const_cast<float *> (ifloats (stereo_in_, 1)));
      }
    else
      {
        for (size_t i = 0; i < plugin_instance->audio_in_ports.size(); i++)
          plugin_instance->connect_audio_port (plugin_instance->audio_in_ports[i], const_cast<float *> (ifloats (mono_ins_[i], 0)));
      }

    if (plugin_instance->audio_out_ports.size() == 2)
      {
        plugin_instance->connect_audio_port (plugin_instance->audio_out_ports[0], oblock (stereo_out_, 0));
        plugin_instance->connect_audio_port (plugin_instance->audio_out_ports[1], oblock (stereo_out_, 1));
      }
    else
      {
        for (size_t i = 0; i < plugin_instance->audio_out_ports.size(); i++)
          plugin_instance->connect_audio_port (plugin_instance->audio_out_ports[i], oblock (mono_outs_[i], 0));
      }
    plugin_instance->run (n_frames);
  }
  void
  set_port_value (const char*         port_symbol,
                  const void*         value,
                  uint32_t            size,
                  uint32_t            type)
  {
    double dvalue = 0;
    if (type == plugin_host.urids.atom_Float)
      {
        dvalue = *(const float*)value;
      }
    else if (type == plugin_host.urids.atom_Double)
      {
        dvalue = *(const double*)value;
      }
    else if (type == plugin_host.urids.atom_Int)
      {
        dvalue = *(const int32_t*)value;
      }
    else if (type == plugin_host.urids.atom_Long)
      {
        dvalue = *(const int64_t*)value;
      }
    else
      {
        printerr ("error: Preset `%s' value has bad type <%s>\n",
                  port_symbol, plugin_instance->plugin_host.urid_map.urid_unmap (type));
        return;
      }
    printerr ("%s = %f\n", port_symbol, dvalue);
    for (int i = 0; i < (int) param_id_port.size(); i++)
      {
        if (param_id_port[i]->symbol == port_symbol)
          {
            // TODO: should set the parameters here
            //set_param (i + PID_CONTROL_OFFSET, dvalue);
          }
      }
  }

  static void
  set_port_value (const char*         port_symbol,
                  void*               user_data,
                  const void*         value,
                  uint32_t            size,
                  uint32_t            type)
  {
    LV2Processor *dev = (LV2Processor *) user_data;
    dev->set_port_value (port_symbol, value, size, type);
  }
public:
  LV2Processor (const ProcessorSetup &psetup) :
    AudioProcessor (psetup),
    plugin_host (PluginHost::the())
  {}
  ~LV2Processor()
  {
    if (plugin_instance)
      {
        delete plugin_instance;
        plugin_instance = nullptr;
      }
  }
  static void
  static_info (AudioProcessorInfo &info)
  {
    // info.uri = "Bse.LV2Device";
    // info.version = "0";
    info.version ="1";
    info.label = "LV2Processor";
    info.category = "Synth";
    info.creator_name = "Stefan Westerfeld";
    info.website_url  = "https://anklang.testbit.eu";
  }
  void
  set_uri (const string& lv2_uri)
  {
    lv2_uri_ = lv2_uri;
  }
  PluginInstance *
  instance()
  {
    return plugin_instance;
  }
  static const void*
  get_port_value_for_save (const char *port_symbol,
                           void       *user_data,
                           uint32_t   *size,
                           uint32_t   *type)
  {
    auto *port_values = (map<string, float> *) user_data;

    auto it = port_values->find (port_symbol);
    if (it != port_values->end())
      {
        *size = sizeof (float);
        *type = PluginHost::the().urids.atom_Float;

        return &it->second;
      }
    else
      {
        *size = *type = 0;
        return nullptr;
      }
  }
  // lilv_state_to_string requires a non-empty URI
  static constexpr auto ANKLANG_STATE_URI = "urn:anklang:state";
  void
  save_state (WritNode &xs, const string& device_path, ProjectImpl *project)
  {
    String blobname = string_format ("lv2-%s.ttl", device_path);
    const String blobfile = project->writer_file_name (blobname);
    printerr ("blobfile %s\n", blobfile.c_str());
    /* build a map containing all the port values */
    map<string, float> port_values;
    for (size_t i = 0; i < param_id_port.size(); i++)
      port_values[param_id_port[i]->symbol] = get_param (int (i) + PID_CONTROL_OFFSET);

    LilvState *state;
    x11wrapper->exec_in_gtk_thread (
      [&]()
        {
          state = lilv_state_new_from_instance (plugin_instance->plugin,
                                                plugin_instance->instance,
                                                plugin_host.urid_map.lv2_map(),
                                                nullptr,
                                                nullptr,
                                                nullptr,
                                                nullptr,
                                                get_port_value_for_save,
                                                &port_values,
                                                0,
                                                nullptr);
        });
    char *str = lilv_state_to_string (plugin_host.world,
                                      plugin_host.urid_map.lv2_map(),
                                      plugin_host.urid_map.lv2_unmap(),
                                      state,
                                      ANKLANG_STATE_URI,
                                      nullptr);
    if (!Path::memwrite (blobfile, strlen (str), (uint8 *) str, false))
      printerr ("%s: %s: memwrite failed\n", program_alias(), blobfile);
    else
      {
        Error err = project->writer_add_file (blobfile);
        if (!!err)
          printerr ("%s: %s: %s\n", program_alias(), blobfile, ase_error_blurb (err));
        else
          xs["state_blob"] & blobname;
      }
    free (str);
    lilv_state_free (state);
  }
  void
  load_state (WritNode &xs, ProjectImpl *project)
  {
    String blobname;
    xs["state_blob"] & blobname;
    StreamReaderP blob = blobname.empty() ? nullptr : project->load_blob (blobname);
    if (blob)
      {
        int ret = 0;
        String blob_data;
        std::vector<char> buffer (StreamReader::buffer_size);
        while ((ret = blob->read (buffer.data(), buffer.size())) > 0)
          {
            blob_data.insert (blob_data.end(), buffer.data(), buffer.data() + ret);
          }
        if (ret == 0)
          {
            LilvState *state = lilv_state_new_from_string (plugin_host.world,
                                                           plugin_host.urid_map.lv2_map(),
                                                           blob_data.c_str());
            if (state)
              {
                PortRestoreHelper port_restore_helper (plugin_host);
                x11wrapper->exec_in_gtk_thread (
                  [&]()
                    {
                      const LV2_Feature* state_features[] = { // TODO: more features
                        plugin_host.urid_map.map_feature(),
                        plugin_host.urid_map.unmap_feature(),
                        NULL
                      };
                      lilv_state_restore (state, plugin_instance->instance, PortRestoreHelper::set, &port_restore_helper, 0, state_features);
                    });
                for (int i = 0; i < (int) param_id_port.size(); i++)
                  {
                    auto it = port_restore_helper.values.find (param_id_port[i]->symbol);
                    if (it != port_restore_helper.values.end())
                      send_param (i + PID_CONTROL_OFFSET, it->second);
                  }
                lilv_state_free (state);
              }
            else
              {
                printerr ("%s: LV2Device: blob read error: '%s' LV2 state from string failed\n", program_alias(), blobname);
              }
          }
        else
          {
            printerr ("%s: LV2Device: blob read error: '%s' read failed\n", program_alias(), blobname);
          }
        blob->close();
      }
    else
      {
        printerr ("%s: LV2Device: blob read error: '%s' open failed\n", program_alias(), blobname);
      }
  }
};

DeviceInfoS
LV2DeviceImpl::list_lv2_plugins()
{
  PluginHost& plugin_host = PluginHost::the();
  return plugin_host.list_plugins();
}

DeviceP
LV2DeviceImpl::create_lv2_device (AudioEngine &engine, const String &lv2_uri_with_prefix)
{
  assert_return (string_startswith (lv2_uri_with_prefix, "LV2:"), nullptr);
  const String lv2_uri = lv2_uri_with_prefix.substr (4);

  auto make_device = [lv2_uri] (const String &aseid, AudioProcessor::StaticInfo static_info, AudioProcessorP aproc) -> LV2DeviceImplP {
    /* TODO: is this good code to handle LV2Processor URI initialization */
    auto lv2aproc = dynamic_cast<LV2Processor *> (aproc.get());
    lv2aproc->set_uri (lv2_uri);

    return LV2DeviceImpl::make_shared (lv2_uri, aproc);
  };
  DeviceP devicep = AudioProcessor::registry_create ("Ase::Devices::LV2Processor", engine, make_device);
  // return_unless (devicep && devicep->_audio_processor("Ase::Device::LV2Processor", nullptr);
  return devicep;
}

bool
LV2DeviceImpl::gui_supported()
{
  auto lv2aproc = dynamic_cast<LV2Processor *> (proc_.get());
  auto ui = lv2aproc->instance()->get_plugin_ui();
  return ui != nullptr;
}

void
LV2DeviceImpl::gui_toggle()
{
  auto lv2aproc = dynamic_cast<LV2Processor *> (proc_.get());
  lv2aproc->instance()->toggle_ui();
}

LV2DeviceImpl::LV2DeviceImpl (const String &lv2_uri, AudioProcessorP proc) :
  proc_ (proc), info_ (PluginHost::the().lv2_device_info (lv2_uri))
{
}

PropertyS
LV2DeviceImpl::access_properties ()
{
  return proc_->access_properties();
}

String
LV2DeviceImpl::get_device_path ()
{
  // TODO: deduplicate this with clapdevice.cc
  std::vector<String> nums;
  NativeDevice *parent = dynamic_cast<NativeDevice*> (this->_parent());
  for (Device *dev = this; parent; dev = parent, parent = dynamic_cast<NativeDevice*> (dev->_parent()))
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
LV2DeviceImpl::serialize (WritNode &xs)
{
  DeviceImpl::serialize (xs);

  if (auto lv2aproc = dynamic_cast<LV2Processor *> (proc_.get()))
    {
      if (xs.in_save())
        lv2aproc->save_state (xs, get_device_path(), _project());
      if (xs.in_load())
        lv2aproc->load_state (xs, _project());
    }
}

static auto lv2processor = register_audio_processor<LV2Processor> ("Ase::Devices::LV2Processor");

} // Ase

/* --- TODO ---
 *
 * - some plugins (with lots of properties?) freeze UI - padthv1, drmr (#31)
 * - serialization (state extension)
 * - ui resizable
 * - port size for notifications
 * - provide bpm,...
 * - restore top level Makefile.mk
 */
