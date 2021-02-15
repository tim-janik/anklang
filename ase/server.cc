// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "server.hh"
#include "jsonipc/jsonipc.hh"
#include "platform.hh"
#include "properties.hh"
#include "serialize.hh"
#include "main.hh"
#include "utils.hh"
#include "path.hh"
#include "internal.hh"

namespace Ase {

// == Preferences ==
PropertyS
Preferences::access_properties (const EventHandler &eventhandler)
{
  using namespace Properties;
  static PropertyBag bag (eventhandler);
  return_unless (bag.props.empty(), bag.props);
  bag.group = _("Synthesis Settings");
  bag += Text (&pcm_driver, _("PCM Driver"), "", "auto", STANDARD, _("Driver and device to be used for PCM input and output"));
  bag += Range (&synth_latency, _("Latency"), "", 0, 3000, 5, "ms", STANDARD + "step=5",
                _("Processing duration between input and output of a single sample, smaller values increase CPU load"));
  bag += Range (&synth_mixing_freq, _("Synth Mixing Frequency"), "", 48000, 48000, 48000, "Hz", STANDARD,
                _("Unused, synthesis mixing frequency is always 48000 Hz"));
  bag += Range (&synth_control_freq, _("Synth Control Frequency"), "", 1500, 1500, 1500, "Hz", STANDARD,
                _("Unused frequency setting"));
  bag.group = _("MIDI");
  bag += Text (&midi_driver, _("MIDI Driver"), "", STANDARD, _("Driver and device to be used for MIDI input and output"));
  bag += Bool (&invert_sustain, _("Invert Sustain"), "", false, STANDARD,
               _("Invert the state of sustain (damper) pedal so on/off meanings are reversed"));
  bag.group = _("Default Values");
  bag += Text (&author_default, _("Default Author"), "", STANDARD, _("Default value for 'Author' fields"));
  bag += Text (&license_default, _("Default License"), "", STANDARD, _("Default value for 'License' fields"));
  bag.group = _("Search Paths");
  bag += Text (&sample_path, _("Sample Path"), "", STANDARD + "searchpath",
               _("Search path of directories, seperated by \";\", used to find audio samples."));
  bag += Text (&effect_path, _("Effect Path"), "", STANDARD + "searchpath",
               _("Search path of directories, seperated by \";\", used to find effect files."));
  bag += Text (&instrument_path, _("Instrument Path"), "", STANDARD + "searchpath",
               _("Search path of directories, seperated by \";\", used to find instrument files."));
  bag += Text (&plugin_path, _("Plugin Path"), "", STANDARD + "searchpath",
               _("Search path of directories, seperated by \";\", used to find plugins. This path "
                 "is searched for in addition to the standard plugin location on this system."));
  return bag.props;
}

static Preferences
preferences_defaults()
{
  // Server is *not* yet available
  Preferences prefs;
  // static defaults
  prefs.pcm_driver = "auto";
  prefs.synth_latency = 22;
  prefs.synth_mixing_freq = 48000;
  prefs.synth_control_freq = 1500;
  prefs.midi_driver = "auto";
  prefs.invert_sustain = false;
  prefs.license_default = "Creative Commons Attribution-ShareAlike 4.0 (https://creativecommons.org/licenses/by-sa/4.0/)";
  // dynamic defaults
  const String default_user_path = Path::join (Path::user_home(), "Anklang");
  prefs.effect_path     = default_user_path + "/Effects";
  prefs.instrument_path = default_user_path + "/Instruments";
  prefs.plugin_path     = default_user_path + "/Plugins";
  prefs.sample_path     = default_user_path + "/Samples";
  String user = user_name();
  if (!user.empty())
    {
      String name = user_real_name();
      if (!name.empty() && user != name)
        prefs.author_default = name;
      else
        prefs.author_default = user;
    }
  return prefs;
}

static String
pathname_anklangrc()
{
  static const String anklangrc = Path::join (Path::config_home(), "anklang", "anklangrc.json");
  return anklangrc;
}

// == ServerImpl ==
JSONIPC_INHERIT (ServerImpl, Server);

ServerImpl::ServerImpl ()
{
  prefs_ = preferences_defaults();
  const String jsontext = Path::stringread (pathname_anklangrc());
  if (!jsontext.empty())
    json_parse (jsontext, prefs_);
  on_event ("change:prefs", [this] (auto...) {
    Path::stringwrite (pathname_anklangrc(), json_stringify (prefs_, Writ::INDENT | Writ::SKIP_EMPTYSTRING), true);
  });
}

ServerImpl::~ServerImpl ()
{
  fatal_error ("ServerImpl references must persist");
}

String
ServerImpl::get_version ()
{
  return ase_version();
}

String
ServerImpl::get_vorbis_version ()
{
  return "-";
}

String
ServerImpl::get_mp3_version ()
{
  return "-";
}

void
ServerImpl::shutdown ()
{
  // defer quit() slightly, so remote calls are still completed
  main_loop->exec_timer ([] () { main_loop->quit (0); }, 5, -1, EventLoop::PRIORITY_NORMAL);
}

ProjectP
ServerImpl::last_project ()
{
  return Project::last_project();
}

ProjectP
ServerImpl::create_project (String projectname)
{
  return Project::create (projectname);
}

PropertyS
ServerImpl::access_prefs()
{
  return prefs_.access_properties ([this] (const Event&) {
    ValueR args { { "prefs", json_parse<ValueR> (json_stringify (prefs_)) } };
    emit_event ("change", "prefs", args);
  });
}

DriverEntryS
ServerImpl::list_pcm_drivers ()
{
  return {};
}

DriverEntryS
ServerImpl::list_midi_drivers ()
{
  return {};
}

ServerImplP
ServerImpl::instancep ()
{
  static ServerImplP *sptr = new ServerImplP (std::make_shared<ServerImpl>());
  return *sptr;
}

// == Server ==
ServerP
Server::instancep ()
{
  return ServerImpl::instancep();
}

Server&
Server::instance ()
{
  return *instancep();
}

static ValueR session_data;

void
Server::set_session_data (const String &key, const Value &v)
{
  session_data[key] = v;
  // printerr ("%s: %s = %s\n", __func__, key, session_data[key].repr());
}

const Value&
Server::get_session_data (const String &key) const
{
  return session_data[key];
}

} // Ase
