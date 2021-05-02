// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "server.hh"
#include "jsonipc/jsonipc.hh"
#include "crawler.hh"
#include "platform.hh"
#include "properties.hh"
#include "serialize.hh"
#include "main.hh"
#include "driver.hh"
#include "utils.hh"
#include "path.hh"
#include "internal.hh"

namespace Ase {

// == Preferences ==
static Choice
choice_from_driver_entry (const DriverEntry &e)
{
  String blurb;
  if (!e.device_info.empty() && !e.capabilities.empty())
    blurb = e.capabilities + "\n" + e.device_info;
  else if (!e.capabilities.empty())
    blurb = e.capabilities;
  else
    blurb = e.device_info;
  Choice c (e.devid, "", e.device_name, blurb);
  if (string_startswith (string_tolower (e.notice), "warn"))
    c.warning = e.notice;
  else
    c.notice = e.notice;
  // e.priority
  // e.readonly
  // e.writeonly
  // e.modem
  return c;
}

static ChoiceS
pcm_driver_choices (Properties::PropertyImpl&)
{
  ChoiceS choices;
  for (const DriverEntry &e : PcmDriver::list_drivers())
    choices.push_back (choice_from_driver_entry (e));
  return choices;
}

static ChoiceS
midi_driver_choices (Properties::PropertyImpl&)
{
  ChoiceS choices;
  for (const DriverEntry &e : MidiDriver::list_drivers())
    if (!e.writeonly)
      choices.push_back (choice_from_driver_entry (e));
  return choices;
}

PropertyS
Preferences::access_properties (const EventHandler &eventhandler)
{
  using namespace Properties;
  static PropertyBag bag (eventhandler);
  return_unless (bag.props.empty(), bag.props);
  bag.group = _("Synthesis Settings");
  bag += Text (&pcm_driver, _("PCM Driver"), "", pcm_driver_choices, STANDARD, _("Driver and device to be used for PCM input and output"));
  bag += Range (&synth_latency, _("Latency"), "", 0, 3000, 5, "ms", STANDARD + "step=5",
                _("Processing duration between input and output of a single sample, smaller values increase CPU load"));
  bag += Range (&synth_mixing_freq, _("Synth Mixing Frequency"), "", 48000, 48000, 48000, "Hz", STANDARD,
                _("Unused, synthesis mixing frequency is always 48000 Hz"));
  bag += Range (&synth_control_freq, _("Synth Control Frequency"), "", 1500, 1500, 1500, "Hz", STANDARD,
                _("Unused frequency setting"));
  bag.group = _("MIDI");
  bag += Bool (&invert_sustain, _("Invert Sustain"), "", false, STANDARD,
               _("Invert the state of sustain (damper) pedal so on/off meanings are reversed"));
  bag += Text (&midi_driver_1, _("MIDI Controller"), "", midi_driver_choices, STANDARD, _("MIDI controller device to be used for MIDI input"));
  bag += Text (&midi_driver_2, _("MIDI Controller"), "", midi_driver_choices, STANDARD, _("MIDI controller device to be used for MIDI input"));
  bag += Text (&midi_driver_3, _("MIDI Controller"), "", midi_driver_choices, STANDARD, _("MIDI controller device to be used for MIDI input"));
  bag += Text (&midi_driver_4, _("MIDI Controller"), "", midi_driver_choices, STANDARD, _("MIDI controller device to be used for MIDI input"));
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
  prefs.midi_driver_1 = "null";
  prefs.midi_driver_2 = "null";
  prefs.midi_driver_3 = "null";
  prefs.midi_driver_4 = "null";
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

// == FileCrawler ==
ResourceCrawlerP
Server::dir_crawler (const String &cwd)
{
  return FileCrawler::make_shared (cwd);
}

// == Choice ==
Choice::Choice (String ident_, String icon_, String label_, String blurb_, String notice_, String warning_) :
  ident (ident_.empty() ? string_to_identifier (label_) : ident_),
  icon (icon_), label (label_), blurb (blurb_), notice (notice_), warning (warning_)
{
  assert_return (ident.empty() == false);
}

Choice::Choice (String icon_, String label_, String blurb_) :
  Choice ("", icon_, label_, blurb_)
{}

ChoiceS&
operator+= (ChoiceS &choices, Choice &&newchoice)
{
  choices.push_back (std::move (newchoice));
  return choices;
}

// == Error ==
/// Describe Error condition.
const char*
ase_error_blurb (Error error)
{
  switch (error)
    {
    case Error::NONE:			return _("OK");
    case Error::INTERNAL:		return _("Internal error (please report)");
    case Error::UNKNOWN:		return _("Unknown error");
    case Error::IO:			return _("Input/output error");
    case Error::PERMS:			return _("Insufficient permissions");
      // out of resource conditions
    case Error::NO_MEMORY:		return _("Out of memory");
    case Error::MANY_FILES:		return _("Too many open files");
    case Error::NO_FILES:		return _("Too many open files in system");
    case Error::NO_SPACE:		return _("No space left on device");
      // file errors
    case Error::FILE_BUSY:		return _("Device or resource busy");
    case Error::FILE_EXISTS:		return _("File exists already");
    case Error::FILE_EOF:		return _("End of file");
    case Error::FILE_EMPTY:		return _("File empty");
    case Error::FILE_NOT_FOUND:		return _("No such file, device or directory");
    case Error::FILE_IS_DIR:		return _("Is a directory");
    case Error::FILE_OPEN_FAILED:	return _("Open failed");
    case Error::FILE_SEEK_FAILED:	return _("Seek failed");
    case Error::FILE_READ_FAILED:	return _("Read failed");
    case Error::FILE_WRITE_FAILED:	return _("Write failed");
      // content errors
    case Error::NO_HEADER:		return _("Failed to detect header");
    case Error::NO_SEEK_INFO:		return _("Failed to retrieve seek information");
    case Error::NO_DATA_AVAILABLE:	return _("No data available");
    case Error::DATA_CORRUPT:		return _("Data corrupt");
    case Error::WRONG_N_CHANNELS:	return _("Wrong number of channels");
    case Error::FORMAT_INVALID:		return _("Invalid format");
    case Error::FORMAT_UNKNOWN:		return _("Unknown format");
    case Error::DATA_UNMATCHED:		return _("Requested data values unmatched");
      // Device errors
    case Error::DEVICE_NOT_AVAILABLE:   return _("No device (driver) available");
    case Error::DEVICE_ASYNC:		return _("Device not async capable");
    case Error::DEVICE_BUSY:		return _("Device busy");
    case Error::DEVICE_FORMAT:		return _("Failed to configure device format");
    case Error::DEVICE_BUFFER:		return _("Failed to configure device buffer");
    case Error::DEVICE_LATENCY:		return _("Failed to configure device latency");
    case Error::DEVICE_CHANNELS:	return _("Failed to configure number of device channels");
    case Error::DEVICE_FREQUENCY:	return _("Failed to configure device frequency");
    case Error::DEVICES_MISMATCH:	return _("Device configurations mismatch");
      // miscellaneous errors
    case Error::TEMP:			return _("Temporary error");
    case Error::WAVE_NOT_FOUND:		return _("No such wave");
    case Error::CODEC_FAILURE:		return _("Codec failure");
    case Error::UNIMPLEMENTED:		return _("Functionality not implemented");
    case Error::INVALID_PROPERTY:	return _("Invalid object property");
    case Error::INVALID_MIDI_CONTROL:	return _("Invalid MIDI control type");
    case Error::PARSE_ERROR:		return _("Parsing error");
    case Error::SPAWN:			return _("Failed to spawn child process");
    default:                            return "";
    }
}

// Map errno onto Ase::Error.
Error
ase_error_from_errno (int sys_errno, Error fallback)
{
  switch (sys_errno)
    {
    case 0:             return Error::NONE;
    case ELOOP:
    case ENAMETOOLONG:
    case ENOENT:        return Error::FILE_NOT_FOUND;
    case EISDIR:        return Error::FILE_IS_DIR;
    case EROFS:
    case EPERM:
    case EACCES:        return Error::PERMS;
#ifdef ENODATA  /* GNU/kFreeBSD lacks this */
    case ENODATA:
#endif
    case ENOMSG:        return Error::FILE_EOF;
    case ENOMEM:        return Error::NO_MEMORY;
    case ENOSPC:        return Error::NO_SPACE;
    case ENFILE:        return Error::NO_FILES;
    case EMFILE:        return Error::MANY_FILES;
    case EFBIG:
    case ESPIPE:
    case EIO:           return Error::IO;
    case EEXIST:        return Error::FILE_EXISTS;
    case ETXTBSY:
    case EBUSY:         return Error::FILE_BUSY;
    case EAGAIN:
    case EINTR:         return Error::TEMP;
    case EFAULT:        return Error::INTERNAL;
    case EBADF:
    case ENOTDIR:
    case ENODEV:
    case EINVAL:
    default:            return fallback;
    }
}

String
ServerImpl::error_blurb (Error error) const
{
  return ase_error_blurb (error);
}

// == MusicalTuning ==
using BlurbDesc = std::pair<const char*, const char*>;

static BlurbDesc
musical_tuning_blurb_desc (MusicalTuning musicaltuning)
{
  switch (musicaltuning)
    {
      // Equal Temperament: http://en.wikipedia.org/wiki/Equal_temperament
    case MusicalTuning::OD_12_TET:
      return { _("12 Tone Equal Temperament"),  // http://en.wikipedia.org/wiki/Equal_temperament
               _("The most common tuning system for modern Western music, "
                 "is the twelve-tone equal temperament, abbreviated as 12-TET, "
                 "which divides the octave into 12 equal parts.") };
    case MusicalTuning::OD_7_TET:
      return { _("7 Tone Equal Temperament"),      // http://en.wikipedia.org/wiki/Equal_temperament
               _("A fairly common tuning system is the seven-tone equal temperament tuning system, "
                 "abbreviated as 7-TET. It divides the octave into 7 equal parts using 171 cent steps.") };
    case MusicalTuning::OD_5_TET:
      return { _("5 Tone Equal Temperament"),      // http://en.wikipedia.org/wiki/Equal_temperament
               _("A fairly common tuning system is the five-tone equal temperament tuning system, "
                 "abbreviated as 5-TET. It divides the octave into 5 equal parts using 240 cent steps.") };
      // Rational Intonation: http://en.wikipedia.org/wiki/Just_intonation
    case MusicalTuning::DIATONIC_SCALE:
      return { _("Diatonic Scale"),                 // http://en.wikipedia.org/wiki/Diatonic_scale
               _("In music theory, a diatonic scale (also: heptatonia prima) is a seven-note "
                 "musical scale comprising five whole-tone and two half-tone steps. "
                 "The half tones are maximally separated, so between two half-tone steps "
                 "there are either two or three whole tones, repeating per octave.") }; // Werckmeister I
    case MusicalTuning::INDIAN_SCALE:
      return { _("Indian Scale"),                   // http://en.wikipedia.org/wiki/Just_intonation#Indian_scales
               _("Diatonic scale used in Indian music with wolf interval at Dha, close to 3/2") };
    case MusicalTuning::PYTHAGOREAN_TUNING:
      return { _("Pythagorean Tuning"),             // http://en.wikipedia.org/wiki/Pythagorean_tuning
               _("Pythagorean tuning is the oldest way of tuning the 12-note chromatic scale, "
                 "in which the frequency relationships of all intervals are based on the ratio 3:2. "
                 "Its discovery is generally credited to Pythagoras.") };
    case MusicalTuning::PENTATONIC_5_LIMIT:
      return { _("Pentatonic 5-limit"),             // http://en.wikipedia.org/wiki/Pentatonic_scale
               _("Pentatonic scales are used in modern jazz and pop/rock contexts "
                 "because they work exceedingly well over several chords diatonic "
                 "to the same key, often better than the parent scale.") };
    case MusicalTuning::PENTATONIC_BLUES:
      return { _("Pentatonic Blues"),               // http://en.wikipedia.org/wiki/Pentatonic_scale
               _("The blues scale is the minor pentatonic with an additional augmented fourth, "
                 "which is referred to as the \"blues note\".") };
    case MusicalTuning::PENTATONIC_GOGO:
      return { _("Pentatonic Gogo"),                // http://en.wikipedia.org/wiki/Pentatonic_scale
               _("The Pentatonic Gogo scale is an anhemitonic pentatonic scale used to tune the "
                 "instruments of the Gogo people of Tanzania.") };
      // Meantone Temperament: http://en.wikipedia.org/wiki/Meantone_temperament
    case MusicalTuning::QUARTER_COMMA_MEANTONE:
      return { _("Quarter-Comma Meantone"),         // http://en.wikipedia.org/wiki/Quarter-comma_meantone
               _("Quarter-comma meantone was the most common meantone temperament in the "
                 "sixteenth and seventeenth centuries and sometimes used later.") };    // Werckmeister II
    case MusicalTuning::SILBERMANN_SORGE:
      return { _("Silbermann-Sorge Temperament"),   // http://de.wikipedia.org/wiki/Silbermann-Sorge-Temperatur
               _("The Silbermann-Sorge temperament is a meantone temperament used for "
                 "Baroque era organs by Gottfried Silbermann.") };
      // Well Temperament: http://en.wikipedia.org/wiki/Well_temperament
    case MusicalTuning::WERCKMEISTER_3:
      return { _("Werckmeister III"),               // http://en.wikipedia.org/wiki/Werckmeister_temperament
               _("This tuning uses mostly pure (perfect) fifths, as in Pythagorean tuning, but each "
                 "of the fifths C-G, G-D, D-A and B-F# is made smaller, i.e. tempered by 1/4 comma. "
                 "Werckmeister designated this tuning as particularly suited for playing chromatic music.") };
    case MusicalTuning::WERCKMEISTER_4:
      return { _("Werckmeister IV"),                // http://en.wikipedia.org/wiki/Werckmeister_temperament
               _("In this tuning the fifths C-G, D-A, E-B, F#-C#, and Bb-F are tempered narrow by 1/3 comma, "
                 "and the fifths G#-D# and Eb-Bb are widened by 1/3 comma. The other fifths are pure. "
                 "Most of its intervals are close to sixth-comma meantone. "
                 "Werckmeister designed this tuning for playing mainly diatonic music.") };
    case MusicalTuning::WERCKMEISTER_5:
      return { _("Werckmeister V"),                 // http://en.wikipedia.org/wiki/Werckmeister_temperament
               _("In this tuning the fifths D-A, A-E, F#-C#, C#-G#, and F-C are narrowed by 1/4 comma, "
                 "and the fifth G#-D# is widened by 1/4 comma. The other fifths are pure. "
                 "This temperament is closer to equal temperament than Werckmeister III or IV.") };
    case MusicalTuning::WERCKMEISTER_6:
      return { _("Werckmeister VI"),                // http://en.wikipedia.org/wiki/Werckmeister_temperament
               _("This tuning is also known as Septenarius tuning is based on a division of the monochord "
                 "length into 196 = 7 * 7 * 4 parts. "
                 "The resulting scale has rational frequency relationships, but in practice involves pure "
                 "and impure sounding fifths. "
                 "Werckmeister described the Septenarius as a \"temperament which has nothing at all to do "
                 "with the divisions of the comma, nevertheless in practice so correct that one can be really "
                 "satisfied with it\".") };
    case MusicalTuning::KIRNBERGER_3:
      return { _("Kirnberger III"),                 // http://en.wikipedia.org/wiki/Johann_Philipp_Kirnberger_temperament
               _("Kirnberger's method of compensating for and closing the circle of fifths is to split the \"wolf\" "
                 "interval known to those who have used meantone temperaments between four fifths instead, "
                 "allowing for four 1/4-comma wolves to take their place. "
                 "1/4-comma wolves are used extensively in meantone and are much easier to tune and to listen to. "
                 "Therefore, only one third remains pure (between C and E).") };
    case MusicalTuning::YOUNG:
      return { _("Young Temperament"),              // http://en.wikipedia.org/wiki/Young_temperament
               _("Thomas Young devised a form of musical tuning to make the harmony most perfect in those keys which "
                 "are the most frequently used (give better major thirds in those keys), but to not have any unplayable keys. "
                 "This is attempted by tuning upwards from C a sequence of six pure fourths, "
                 "as well as six equally imperfect fifths.") };
    default:
      return { "", "" };
    }
}

String
ServerImpl::musical_tuning_blurb (MusicalTuning musicaltuning) const
{
  return musical_tuning_blurb_desc (musicaltuning).first;
}

String
ServerImpl::musical_tuning_desc (MusicalTuning musicaltuning) const
{
  return musical_tuning_blurb_desc (musicaltuning).second;
}

} // Ase
