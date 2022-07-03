// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CLAP_PLUGIN_HH__
#define __ASE_CLAP_PLUGIN_HH__

#include <ase/device.hh>
#include <clap/clap.h>

namespace Ase {

class ClapFileHandle;
class ClapAudioWrapper;

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
  uint32_t flags = 0;
  double   value = NAN;
};
using ClapParamUpdateS = std::vector<ClapParamUpdate>;

// == ClapPluginHandle ==
class ClapPluginHandle : public std::enable_shared_from_this<ClapPluginHandle> {
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
  explicit                    ClapPluginHandle  (const ClapPluginDescriptor &descriptor);
  virtual                    ~ClapPluginHandle  ();
public:
  String                      clapid            () const { return descriptor.id; }
  virtual void                load_state        (StreamReaderP blob, const ClapParamUpdateS &updates) = 0;
  virtual void                save_state        (String &blobfilename, ClapParamUpdateS &updates) = 0;
  virtual void                params_changed    () = 0;
  virtual void                param_updates     (const ClapParamUpdateS &updates) = 0;
  virtual bool                activate          () = 0;
  virtual bool                activated         () const = 0;
  virtual void                deactivate        () = 0;
  virtual void                show_gui          () = 0;
  virtual void                hide_gui          () = 0;
  virtual bool                gui_visible       () = 0;
  virtual bool                supports_gui      () = 0;
  virtual void                destroy_gui       () = 0;
  virtual void                destroy           () = 0;
  virtual AudioProcessorP     audio_processor   () = 0;
  static ClapPluginHandleP    make_clap_handle  (const ClapPluginDescriptor &descriptor, AudioProcessorP audio_processor);
  static CString              audio_processor_type();
  friend class ClapAudioWrapper;
};

// == CLAP utilities ==
StringS     list_clap_files        ();
const char* clap_event_type_string (int etype);
String      clap_event_to_string   (const clap_event_note_t *enote);
DeviceInfo  clap_device_info       (const ClapPluginDescriptor &descriptor);

} // Ase

#endif // __ASE_CLAP_PLUGIN_HH__
