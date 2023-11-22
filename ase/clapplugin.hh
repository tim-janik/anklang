// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CLAP_PLUGIN_HH__
#define __ASE_CLAP_PLUGIN_HH__

#include <ase/device.hh>
#include <clap/clap.h>
#include <ase/gtk2wrap.hh>

namespace Ase {

class ClapFileHandle;
class ClapAudioProcessor;

// == ClapPluginDescriptor ==
class ClapPluginDescriptor {
  ClapFileHandle &clapfile_;
public:
  using Collection = std::vector<ClapPluginDescriptor*>;
  std::string id, name, version, vendor, features;
  std::string description, url, manual_url, support_url;
  explicit                 ClapPluginDescriptor (ClapFileHandle &clapfile);
  void                     open                 () const;
  void                     close                () const;
  const clap_plugin_entry* entry                () const;
  ClapFileHandle&          file_handle          () const { return clapfile_; }
  static void              add_descriptor       (const String &pluginpath, Collection &infos);
  static const Collection& collect_descriptors ();
};

// == ClapParamUpdate ==
struct ClapParamUpdate {
  int64_t  steady_time = 0; // unimplemented
  clap_id  param_id = CLAP_INVALID_ID;
  double   value = NAN;
};
using ClapParamUpdateS = std::vector<ClapParamUpdate>;

// == ClapParamInfo ==
struct ClapParamInfo {
  clap_id  param_id = CLAP_INVALID_ID; // uint32_t
  uint32_t flags = 0; // clap_param_info_flags
  String ident, name, module;
  double min_value = NAN, max_value = NAN, default_value = NAN, current_value = NAN;
  String min_value_text, max_value_text, default_value_text, current_value_text;
  /*ctor*/      ClapParamInfo ();
  void          unset         ();
  static String hints_from_param_info_flags (clap_param_info_flags flags);
};
using ClapParamInfoS = std::vector<ClapParamInfo>;

// == ClapPluginHandle ==
class ClapPluginHandle : public GadgetImpl {
public:
  const ClapPluginDescriptor                    &descriptor;
  const std::vector<clap_audio_ports_config_t>  &audio_ports_configs = audio_ports_configs_;
  const std::vector<clap_audio_port_info_t>     &audio_iport_infos = audio_iport_infos_;
  const std::vector<clap_audio_port_info_t>     &audio_oport_infos = audio_oport_infos_;
  const std::vector<clap_note_port_info_t>      &note_iport_infos = note_iport_infos_;
  const std::vector<clap_note_port_info_t>      &note_oport_infos = note_oport_infos_;
protected:
  std::vector<clap_audio_ports_config_t> audio_ports_configs_;
  std::vector<clap_audio_port_info_t> audio_iport_infos_, audio_oport_infos_;
  std::vector<clap_note_port_info_t> note_iport_infos_, note_oport_infos_;
  std::vector<clap_audio_buffer_t> audio_inputs_, audio_outputs_;
  std::vector<float*> data32ptrs_;
  explicit                    ClapPluginHandle   (const ClapPluginDescriptor &descriptor);
  virtual                    ~ClapPluginHandle   ();
public:
  String                      clapid             () const { return descriptor.id; }
  virtual void                load_state         (WritNode &xs) = 0;
  virtual void                save_state         (WritNode &xs, const String &device_path) = 0;
  virtual bool                param_set_property (clap_id param_id, PropertyP prop) = 0;
  virtual PropertyP           param_get_property (clap_id param_id) = 0;
  virtual double              param_get_value    (clap_id param_id, String *text = nullptr) = 0;
  virtual bool                param_set_value    (clap_id param_id, double v) = 0;
  virtual bool                param_set_value    (clap_id param_id, const String &stringvalue) = 0;
  virtual ClapParamInfoS      param_infos        () = 0;
  virtual void                params_changed     () = 0;
  virtual bool                clap_activate      () = 0;
  virtual bool                clap_activated     () const = 0;
  virtual void                clap_deactivate    () = 0;
  virtual void                show_gui           () = 0;
  virtual void                hide_gui           () = 0;
  virtual bool                gui_visible        () = 0;
  virtual bool                supports_gui       () = 0;
  virtual void                destroy_gui        () = 0;
  virtual void                destroy            () = 0;
  virtual AudioProcessorP     audio_processor    () = 0;
  static ClapPluginHandleP    make_clap_handle   (const ClapPluginDescriptor &descriptor, AudioProcessorP audio_processor);
  static CString              audio_processor_type();
  friend class ClapAudioProcessor;
};

// == CLAP utilities ==
StringS     list_clap_files        ();
const char* clap_event_type_string (int etype);
String      clap_event_to_string   (const clap_event_note_t *enote);
DeviceInfo  clap_device_info       (const ClapPluginDescriptor &descriptor);

Gtk2DlWrapEntry *get_x11wrapper();

} // Ase

#endif // __ASE_CLAP_PLUGIN_HH__
