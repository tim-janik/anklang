// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_GTK2WRAP_HH__
#define __ASE_GTK2WRAP_HH__

#include <string>
#include <functional>
#include <suil/suil.h>

namespace Ase {

struct Gtk2WindowSetup {
  std::string title;
  int width = 0, height = 0;
  std::function<void()> deleterequest_mt;
};

struct Gtk2DlWrapEntry {
  ulong (*create_window)  (const Gtk2WindowSetup &windowsetup);
  bool  (*resize_window)  (ulong windowid, int width, int height);
  void  (*show_window)    (ulong windowid);
  void  (*hide_window)    (ulong windowid);
  void  (*destroy_window) (ulong windowid);
  void  (*threads_enter) ();
  void  (*threads_leave) ();

  void* (*create_suil_window) (std::function<SuilInstance*()> func);
};

} // Ase

extern "C" {
extern Ase::Gtk2DlWrapEntry Ase__Gtk2__wrapentry;
}

#endif // __ASE_GTK2WRAP_HH__
