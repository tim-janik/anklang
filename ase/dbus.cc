// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "dbus.hh"
#include "strings.hh"

#if !__has_include(<dbus/dbus.h>)
#error "Missing <dbus/dbus.h> from libdbus-dev, please set CXXFLAGS and LDFLAGS"
#endif
#include <dbus/dbus.h>

namespace Ase::DBus {

static DBusConnection*
get_system_dbus()
{
  static DBusConnection *bcon = [] () {
    DBusError error;
    dbus_error_init (&error);
    DBusConnection *bcon = dbus_bus_get_private (DBUS_BUS_SYSTEM, &error);
    if (bcon)
      dbus_connection_set_exit_on_disconnect (bcon, false);
    dbus_error_free (&error);
    return bcon;
  } ();
  return bcon;
}

String
rtkit_make_high_priority (pid_t thread, int nice_level)
{
  const String realtimekit1 = "org.freedesktop.RealtimeKit1";
  const char *const DISABLE_RTKIT = getenv ("DISABLE_RTKIT");
  if (DISABLE_RTKIT && DISABLE_RTKIT[0])
    return string_format ("%s: %s", realtimekit1, strerror (ENOTSUP));
  DBusConnection *bcon = get_system_dbus();
  if (!bcon)
    return string_format ("%s: %s", realtimekit1, strerror (ECONNREFUSED));

  DBusMessage *m = dbus_message_new_method_call (realtimekit1.c_str(),                  // service name
                                                 "/org/freedesktop/RealtimeKit1",       // object path
                                                 "org.freedesktop.RealtimeKit1",        // object interface
                                                 "MakeThreadHighPriority");             // method
  if (m)
    {
      dbus_uint64_t tid = thread;
      dbus_int32_t prio = nice_level;
      if (!dbus_message_append_args (m, DBUS_TYPE_UINT64, &tid, DBUS_TYPE_INT32, &prio, DBUS_TYPE_INVALID)) {
        dbus_message_unref (m);
        m = nullptr;
      }
    }
  if (!m)
    return string_format ("%s: %s", realtimekit1, strerror (ENOMEM));

  DBusError error;
  dbus_error_init (&error);
  DBusMessage *r = dbus_connection_send_with_reply_and_block (bcon, m, -1, &error);
  dbus_message_unref (m);
  String emsg = "";
  if (r)
    {
      if (dbus_set_error_from_message (&error, r))
        emsg = error.name;
      dbus_message_unref (r);
    }
  else
    emsg = error.name;
  dbus_error_free (&error);
  return emsg.empty() ? "" : string_format ("%s: %s", realtimekit1, emsg);
}

int
rtkit_get_min_nice_level ()
{
  const char *const DISABLE_RTKIT = getenv ("DISABLE_RTKIT");
  if (DISABLE_RTKIT && DISABLE_RTKIT[0])
    return 0;
  DBusConnection *bcon = get_system_dbus();
  if (!bcon)
    return 0;

  DBusMessage *m = dbus_message_new_method_call ("org.freedesktop.RealtimeKit1",        // service name
                                                 "/org/freedesktop/RealtimeKit1",       // object path
                                                 "org.freedesktop.DBus.Properties",     // object interface
                                                 "Get");                                // method
  if (m)
    {
      const char *rtkinterface = "org.freedesktop.RealtimeKit1";
      const char *proeprty = "MinNiceLevel";
      if (!dbus_message_append_args (m, DBUS_TYPE_STRING, &rtkinterface, DBUS_TYPE_STRING, &proeprty, DBUS_TYPE_INVALID)) {
        dbus_message_unref (m);
        m = nullptr;
      }
    }
  if (!m)
    return 0;

  DBusError error;
  dbus_error_init (&error);
  DBusMessage *r = dbus_connection_send_with_reply_and_block (bcon, m, -1, &error);
  dbus_message_unref (m);
  int nice_level = 0;
  if (r)
    {
      if (!dbus_set_error_from_message (&error, r))
        {
          DBusMessageIter mit;
          dbus_message_iter_init (r, &mit);
          for (int mtype; mtype = dbus_message_iter_get_arg_type (&mit), mtype != DBUS_TYPE_INVALID; dbus_message_iter_next (&mit))
            if (mtype == DBUS_TYPE_VARIANT)
              {
                DBusMessageIter rit;
                dbus_message_iter_recurse (&mit, &rit);
                for (int rtype; rtype = dbus_message_iter_get_arg_type (&rit), rtype != DBUS_TYPE_INVALID; dbus_message_iter_next (&rit))
                  {
                    DBusBasicValue u = {};
                    dbus_message_iter_get_basic (&rit, &u);
                    if (rtype == DBUS_TYPE_INT32)
                      nice_level = u.i32;
                    if (rtype == DBUS_TYPE_INT64)
                      nice_level = u.i64;
                  }
              }
        }
      dbus_message_unref (r);
    }
  dbus_error_free (&error);
  return nice_level;
}

} // Ase::DBus
