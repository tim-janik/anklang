// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CLAP_PLUGIN_HH__
#define __ASE_CLAP_PLUGIN_HH__

#include <ase/device.hh>
#include <clap/clap.h>

namespace Ase {

class ClapFileHandle;

// == ClapPluginDescriptor ==
class ClapPluginDescriptor {
  ClapFileHandle &clapfile_;
public:
  using Collection = std::vector<ClapPluginDescriptor*>;
  std::string id, name, version, vendor, features;
  std::string description, url, manual_url, support_url;
  explicit                 ClapPluginDescriptor (ClapFileHandle &clapfile);
  void                     open                 ();
  void                     close                ();
  const clap_plugin_entry* entry                () const;
  static void              add_descriptor       (const String &pluginpath, Collection &infos);
  static const Collection& collect_descriptors ();
};

// == ClapPluginHandle ==
struct ClapPluginHandle : public std::enable_shared_from_this<ClapPluginHandle> {
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
  const ClapPluginDescriptor                    &descriptor;
  const std::vector<clap_audio_ports_config_t>  &audio_ports_configs = audio_ports_configs_;
  const std::vector<clap_audio_port_info_t>     &audio_iport_infos = audio_iport_infos_;
  const std::vector<clap_audio_port_info_t>     &audio_oport_infos = audio_oport_infos_;
  const std::vector<clap_note_port_info_t>      &note_iport_infos = note_iport_infos_;
  const std::vector<clap_note_port_info_t>      &note_oport_infos = note_oport_infos_;
  const std::vector<clap_audio_buffer_t>        &audio_inputs = audio_inputs_;
  const std::vector<clap_audio_buffer_t>        &audio_outputs = audio_outputs_;
public:
  explicit                    ClapPluginHandle  (const ClapPluginDescriptor &descriptor);
  virtual                     ~ClapPluginHandle ();
  String                      clapid            () const { return descriptor.id; }
  virtual bool                activate          (AudioEngine &engine) = 0;
  virtual bool                activated         () const = 0;
  virtual void                deactivate        () = 0;
  virtual bool                start_processing  ()  = 0;
  virtual clap_process_status process           (const clap_process_t *clapprocess)  = 0;
  virtual void                stop_processing   ()  = 0;
  virtual void                show_gui          () = 0;
  virtual void                hide_gui          () = 0;
  virtual void                destroy_gui       () = 0;
  virtual void                destroy           () = 0;
protected:
  std::vector<EventUnion> input_events_;
  std::vector<clap_event_header_t> output_events_;
  std::vector<clap_audio_ports_config_t> audio_ports_configs_;
  std::vector<clap_audio_port_info_t> audio_iport_infos_, audio_oport_infos_;
  std::vector<clap_note_port_info_t> note_iport_infos_, note_oport_infos_;
  std::vector<clap_audio_buffer_t> audio_inputs_, audio_outputs_;
  std::vector<float*> data32ptrs_;
};

// == ClapFileHandle ==
class ClapFileHandle {
  void *dlhandle_ = nullptr;
  uint  open_count_ = 0;
public:
  const std::string dlfile;
  const clap_plugin_entry *pluginentry = nullptr;
  explicit      ClapFileHandle  (const String &pathname);
  virtual      ~ClapFileHandle  ();
  static String get_dlerror     ();
  void          close           ();
  void          open            ();
  bool          opened          () const;
  template<typename Ptr>
  Ptr           symbol          (const char *symname) const;
};

// == CLAP utilities ==
StringS     list_clap_files     ();
const char* clap_event_type_string (int etype);
String      clap_event_to_string   (const clap_event_note_t *enote);

} // Ase

#endif // __ASE_CLAP_PLUGIN_HH__
