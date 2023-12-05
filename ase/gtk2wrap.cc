// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "gtk2wrap.hh"
#include <gtk/gtk.h>
#include <suil/suil.h>
#include <semaphore.h>
#include <thread>
#include <map>
#include <cassert>

namespace { // Anon
using namespace Ase;

using std::string;

static std::thread *gtk_thread = nullptr;
static std::thread::id gtk_thread_id {};

// == Semaphore ==
class Semaphore {
  /*copy*/         Semaphore (const Semaphore&) = delete;
  Semaphore& operator=       (const Semaphore&) = delete;
  sem_t sem = {};
public:
  explicit  Semaphore () noexcept { const int err = sem_init (&sem, 0, 0); g_return_if_fail (!err); }
  void      post      () noexcept { sem_post (&sem); }
  void      wait      () noexcept { sem_wait (&sem); }
  /*dtor*/ ~Semaphore () noexcept { sem_destroy (&sem); }
};

// == GBoolean lambda callback ==
using BWrapFunc = std::function<bool()>;
struct BWrap {
  void *data = this;
  void (*deleter) (void*) = [] (void *data) {
    BWrap *self = (BWrap*) data;
    self->deleter = nullptr;
    delete self;
  };
  gboolean (*boolfunc) (void*) = [] (void *data) -> int {
    BWrap *self = (BWrap*) data;
    return self->func();
  };
  gboolean (*truefunc) (void*) = [] (void *data) -> int {
    BWrap *self = (BWrap*) data;
    self->func();
    return true;
  };
  gboolean (*falsefunc) (void*) = [] (void *data) -> int {
    BWrap *self = (BWrap*) data;
    self->func();
    return true;
  };
  const BWrapFunc func;
  BWrap (const BWrapFunc &fun) : func (fun) {}
};
static BWrap* bwrap (const BWrapFunc &fun) {
  return new BWrap (fun);
}
static BWrap* vwrap (const std::function<void()> &fun) {
  return new BWrap ([fun]() { fun(); return false; });
}

// == functions ==
static void
gtkmain()
{
  pthread_setname_np (pthread_self(), "gtk2wrap:thread");
  gdk_threads_init();
  gdk_threads_enter();

  if (1) {
    int argc = 0;
    char **argv = nullptr;
    gtk_init (&argc, &argv);
  }

  gtk_main();
  gdk_threads_leave();
}

static std::unordered_map<ulong,GtkWidget*> windows;

static ulong
create_window (const Gtk2WindowSetup &wsetup)
{
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  // g_signal_connect (window, "destroy", gtk_main_quit, nullptr);
  if (wsetup.width > 0 && wsetup.height > 0)
    gtk_window_set_resizable (GTK_WINDOW (window), false);
  if (wsetup.deleterequest_mt) {
    BWrap *bw = vwrap (wsetup.deleterequest_mt);
    g_signal_connect_data (window, "delete-event", (GCallback) bw->truefunc, bw, (GClosureNotify) bw->deleter, G_CONNECT_SWAPPED);
  }
  GtkWidget *socket = gtk_socket_new();
  gtk_container_add (GTK_CONTAINER (window), socket);
  gtk_widget_set_size_request (socket, wsetup.width, wsetup.height);
  gtk_widget_realize (socket);
  ulong windowid = gtk_socket_get_id (GTK_SOCKET (socket));
  windows[windowid] = window;
  gtk_widget_show_all (gtk_bin_get_child (GTK_BIN (window)));
  gtk_window_set_title (GTK_WINDOW (window), wsetup.title.c_str());
  return windowid;
}

static void *
create_suil_window (const string &window_title, bool resizable, const std::function<void()>& deleterequest_mt)
{
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_resizable (GTK_WINDOW (window), resizable);
  BWrap *bw = vwrap (deleterequest_mt);
  g_signal_connect_data (window, "delete-event", (GCallback) bw->truefunc, bw, (GClosureNotify) bw->deleter, G_CONNECT_SWAPPED);
  gtk_window_set_title (GTK_WINDOW (window), window_title.c_str());
  return window;
}

static bool
destroy_window (ulong windowid)
{
  auto it = windows.find (windowid);
  if (it == windows.end()) return false;
  GtkWidget *window = it->second;
  windows.erase (it);
  gtk_widget_destroy (window);
  return true;
}

static bool
resize_window (ulong windowid, int width, int height)
{
  auto it = windows.find (windowid);
  if (it == windows.end()) return false;
  GtkWidget *window = it->second;
  gtk_widget_set_size_request (gtk_bin_get_child (GTK_BIN (window)), width, height);
  return true;
}

static bool
show_window (ulong windowid)
{
  auto it = windows.find (windowid);
  if (it == windows.end()) return false;
  GtkWidget *window = it->second;
  gtk_widget_show (window);
  return true;
}

static bool
hide_window (ulong windowid)
{
  auto it = windows.find (windowid);
  if (it == windows.end()) return false;
  GtkWidget *window = it->second;
  gtk_widget_hide (window);
  return true;
}

static std::thread::id
get_gtk_thread_id()
{
  return std::this_thread::get_id();
}

struct TimerHelper
{
  std::function<bool()> callback;
  guint id = 0;
};

std::map<guint, std::unique_ptr<TimerHelper>> timer_helper_map;

static guint
register_timer (const std::function<bool()>& callback, guint interval_ms)
{
  auto timer_helper_ptr = std::make_unique<TimerHelper> (callback, 0);

  guint id = g_timeout_add (interval_ms,
    [] (gpointer data) -> int
      {
        TimerHelper *helper = (TimerHelper *) data;
        auto again = helper->callback();
        if (!again)
          timer_helper_map.erase (helper->id);
        return again;
      },
    timer_helper_ptr.get());

  timer_helper_ptr->id = id;
  timer_helper_map[id] = std::move (timer_helper_ptr);
  return id;
}

static bool
remove_timer (uint timer_id)
{
  if (timer_helper_map.erase (timer_id) != 1)
    fprintf (stderr, "Gtk2Wrap: removing timer callback id=%u from map failed\n", timer_id);

  if (g_source_remove (timer_id))
    return true;

  fprintf (stderr, "Gtk2Wrap: removing timer with id=%u failed", timer_id);
  return false;
}

static bool
exec_in_gtk_thread (const std::function<void()>& func)
{
  func();
  return true;
}

template<typename Ret, typename ...Args, typename ...Params> static Ret
gtkidle_call (Ret (*func) (Params...), Args &&...args)
{
  if (!gtk_thread)        // FIXME: need thread cleanup
    {
      gtk_thread = new std::thread (gtkmain);
      gtk_thread_id = gtk_thread->get_id();
    }
  // using this function from gtk thread would block execution (and never return)
  assert (std::this_thread::get_id() != gtk_thread_id);
  Semaphore sem;
  Ret ret = {};
  BWrap *bw = bwrap ([&sem, &ret, func, &args...] () -> bool {
    GDK_THREADS_ENTER ();
    ret = func (std::forward<Args> (args)...);
    GDK_THREADS_LEAVE ();
    sem.post();
    return false;
  });
  g_idle_add_full (G_PRIORITY_HIGH, bw->boolfunc, bw, bw->deleter);
  // See gdk_threads_add_idle_full for LEAVE/ENTER reasoning
  sem.wait();
  return ret;
}

} // Anon

extern "C" {
Ase::Gtk2DlWrapEntry Ase__Gtk2__wrapentry {
  .create_window = [] (const Gtk2WindowSetup &windowsetup) -> ulong {
    return gtkidle_call (create_window, windowsetup);
  },
  .resize_window = [] (ulong windowid, int width, int height) {
    return gtkidle_call (resize_window, windowid, width, height);
  },
  .show_window = [] (ulong windowid) {
    gtkidle_call (show_window, windowid);
  },
  .hide_window = [] (ulong windowid) {
    gtkidle_call (hide_window, windowid);
  },
  .destroy_window = [] (ulong windowid) {
    gtkidle_call (destroy_window, windowid);
  },
  .threads_enter = gdk_threads_enter,
  .threads_leave = gdk_threads_leave,
  .gtk_thread_id = [] () -> std::thread::id {
    return gtkidle_call (get_gtk_thread_id);
  },
  .register_timer = [] (const std::function<bool()>& callback, uint interval_ms) -> uint {
    return gtkidle_call (register_timer, callback, interval_ms);
  },
  .remove_timer = [] (uint timer_id) -> bool {
    return gtkidle_call (remove_timer, timer_id);
  },
  .exec_in_gtk_thread = [] (const std::function<void()>& func) {
    gtkidle_call (exec_in_gtk_thread, func);
  },
  .create_suil_host = [] (SPortWriteFunc write_func, SPortIndexFunc index_func) -> void * {
    return gtkidle_call (suil_host_new, write_func, index_func, nullptr, nullptr);
  },
  .create_suil_instance = [] (void *host, void *controller,
                              const string &container_type_uri, const string &plugin_uri,
                              const string &ui_uri, const string &ui_type_uri,
                              const string &ui_bundle_path, const string &ui_binary_path,
                              const LV2_Feature *const *features) -> void * {
    return gtkidle_call (suil_instance_new, (SuilHost *) host, (SuilController *) controller,
                         container_type_uri.c_str(), plugin_uri.c_str(),
                         ui_uri.c_str(), ui_type_uri.c_str(),
                         ui_bundle_path.c_str(), ui_binary_path.c_str(),
                         features);
  },
  .destroy_suil_instance = [] (void *instance) {
    gtkidle_call (exec_in_gtk_thread, [&] { suil_instance_free ((SuilInstance *) instance); });
  },
  .create_suil_window = [] (const string &window_title, bool resizable, const std::function<void()>& deleterequest_mt) -> void * {
    return gtkidle_call (create_suil_window, window_title, resizable, deleterequest_mt);
  },
  .add_suil_widget_to_window = [] (void *window, void *instance) {
    gtkidle_call (exec_in_gtk_thread, [&] {
      gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (suil_instance_get_widget ((SuilInstance *) instance)));
      gtk_widget_show_all (GTK_WIDGET (window));
    });
  },
  .destroy_suil_window = [] (void *window) -> void {
    gtkidle_call (exec_in_gtk_thread, [&] { gtk_widget_destroy (GTK_WIDGET (window)); });
  },
  .suil_ui_supported = [] (const string& host_type_uri, const string& ui_type_uri) -> unsigned {
    return suil_ui_supported (host_type_uri.c_str(), ui_type_uri.c_str());
  },
  .suil_instance_port_event_gtk_thread = [] (void *instance, uint32_t port_index, uint32_t buffer_size, uint32_t format, const void *buffer) {
    return suil_instance_port_event ((SuilInstance *) instance, port_index, buffer_size, format, buffer);
  },
  .get_suil_widget_gtk_thread = [] (void *instance) {
    return suil_instance_get_widget ((SuilInstance *) instance);
  }
};
} // "C"
