// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_GTK2WRAP_HH__
#define __ASE_GTK2WRAP_HH__

#include <string>
#include <functional>
#include <thread>

#include <lv2/core/lv2.h>

namespace Ase {

struct Gtk2WindowSetup {
  std::string title;
  int width = 0, height = 0;
  std::function<void()> deleterequest_mt;
};

typedef void (*SPortWriteFunc) (void *controller, uint32_t port_index, uint32_t buffer_size, uint32_t protocol, void const* buffer);
typedef uint32_t (*SPortIndexFunc) (void *controller, const char* port_symbol);

struct Gtk2DlWrapEntry {
  ulong (*create_window)              (const Gtk2WindowSetup &windowsetup);
  bool  (*resize_window)              (ulong windowid, int width, int height);
  void  (*show_window)                (ulong windowid);
  void  (*hide_window)                (ulong windowid);
  void  (*destroy_window)             (ulong windowid);
  void  (*threads_enter)              ();
  void  (*threads_leave)              ();
  std::thread::id (*gtk_thread_id)    ();
  uint  (*register_timer)             (const std::function<bool()>& callback, uint interval_ms);
  bool  (*remove_timer)               (uint timer_id);
  void  (*exec_in_gtk_thread)         (const std::function<void()>& func);
  void *(*create_suil_host)           (SPortWriteFunc write_func, SPortIndexFunc index_func);
  void *(*create_suil_instance)       (void *host, void *controller,
                                       const std::string &container_type_uri, const std::string &plugin_uri,
                                       const std::string &ui_uri, const std::string &ui_type_uri,
                                       const std::string &ui_bundle_path, const std::string &ui_binary_path,
                                       const LV2_Feature *const *features);
  void  (*destroy_suil_instance)      (void *instance);
  void *(*create_suil_window)         (void *instance, const std::string &window_title, const std::function<void()>& deleterequest_mt);
  void  (*destroy_suil_window)        (void *window);
  uint  (*suil_ui_supported)          (const std::string& host_type_uri, const std::string& ui_type_uri);
  void  (*suil_instance_port_event_gtk_thread) (void *instance, uint32_t port_index, uint32_t buffer_size, uint32_t format, const void *buffer);
};

} // Ase

extern "C" {
extern Ase::Gtk2DlWrapEntry Ase__Gtk2__wrapentry;
}

#endif // __ASE_GTK2WRAP_HH__
