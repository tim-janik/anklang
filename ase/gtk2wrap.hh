// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_GTK2WRAP_HH__
#define __ASE_GTK2WRAP_HH__

#include <string>
#include <functional>
#include <thread>

namespace Ase {

struct Gtk2WindowSetup {
  std::string title;
  int width = 0, height = 0;
  std::function<void()> deleterequest_mt;
};

struct Gtk2DlWrapEntry {
  ulong (*create_window)              (const Gtk2WindowSetup &windowsetup);
  bool  (*resize_window)              (ulong windowid, int width, int height);
  bool  (*resize_window_gtk_thread)   (ulong windowid, int width, int height);
  void  (*show_window)                (ulong windowid);
  void  (*hide_window)                (ulong windowid);
  void  (*destroy_window)             (ulong windowid);
  void  (*threads_enter)              ();
  void  (*threads_leave)              ();
  std::thread::id (*gtk_thread_id)    ();
  uint  (*register_timer)             (const std::function<bool()>& callback, uint interval_ms);
  bool  (*remove_timer)               (uint timer_id);
  void  (*exec_in_gtk_thread)         (const std::function<void()>& func);
};

} // Ase

extern "C" {
extern Ase::Gtk2DlWrapEntry Ase__Gtk2__wrapentry;
}

#endif // __ASE_GTK2WRAP_HH__
