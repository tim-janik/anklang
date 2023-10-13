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
#include "project.hh"
#include "path.hh"
#include "clapdevice.hh"
#include "wave.hh"
#include "internal.hh"
#include <atomic>

namespace Ase {

// == Preferences ==
static Choice
choice_from_driver_entry (const DriverEntry &e, const String &icon_keywords)
{
  String blurb;
  if (!e.device_info.empty() && !e.capabilities.empty())
    blurb = e.capabilities + "\n" + e.device_info;
  else if (!e.capabilities.empty())
    blurb = e.capabilities;
  else
    blurb = e.device_info;
  Choice c (e.devid, e.device_name, blurb);
  if (string_startswith (string_tolower (e.notice), "warn"))
    c.warning = e.notice;
  else
    c.notice = e.notice;
  // e.priority
  // e.readonly
  // e.writeonly
  // e.modem
  c.icon = MakeIcon::KwIcon (icon_keywords + "," + e.hints);
  return c;
}

static ChoiceS
pcm_driver_choices (Properties::PropertyImpl&)
{
  ChoiceS choices;
  for (const DriverEntry &e : PcmDriver::list_drivers())
    choices.push_back (choice_from_driver_entry (e, "pcm"));
  return choices;
}

static ChoiceS
midi_driver_choices (Properties::PropertyImpl&)
{
  ChoiceS choices;
  for (const DriverEntry &e : MidiDriver::list_drivers())
    if (!e.writeonly)
      choices.push_back (choice_from_driver_entry (e, "midi"));
  return choices;
}

PropertyS
Preferences::access_properties (const EventHandler &eventhandler)
{
  using namespace Properties;
  static PropertyBag bag;
  return_unless (bag.props.empty(), bag.props);
  bag.group = _("Synthesis Settings");
  bag += Text (&pcm_driver, _("PCM Driver"), "", pcm_driver_choices, STANDARD, _("Driver and device to be used for PCM input and output"));
  bag += Range (&synth_latency, _("Latency"), "", 0, 3000, 5, "ms", STANDARD + String ("step=5"),
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
  bag += Text (&sample_path, _("Sample Path"), "", STANDARD + String ("searchpath"),
               _("Search path of directories, seperated by \";\", used to find audio samples."));
  bag += Text (&effect_path, _("Effect Path"), "", STANDARD + String ("searchpath"),
               _("Search path of directories, seperated by \";\", used to find effect files."));
  bag += Text (&instrument_path, _("Instrument Path"), "", STANDARD + String ("searchpath"),
               _("Search path of directories, seperated by \";\", used to find instrument files."));
  bag += Text (&plugin_path, _("Plugin Path"), "", STANDARD + String ("searchpath"),
               _("Search path of directories, seperated by \";\", used to find plugins. This path "
                 "is searched for in addition to the standard plugin location on this system."));
  bag.on_events ("notify", eventhandler);
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

static constexpr size_t telemetry_size = 4 * 1024 * 1024;

ServerImpl *SERVER = nullptr;

ServerImpl::ServerImpl () :
  telemetry_arena (telemetry_size)
{
  // initialize preferences to defaults
  prefs_ = preferences_defaults();
  // create preference properties, capturing defaults
  prefs_properties_ = prefs_.access_properties ([this] (const Event&) {
    ValueR args { { "prefs", json_parse<ValueR> (json_stringify (prefs_)) } };
    emit_event ("change", "prefs", args);
  });
  // load preferences
  const String jsontext = Path::stringread (pathname_anklangrc());
  if (!jsontext.empty())
    json_parse (jsontext, prefs_);
  pchange_ =
    on_event ("change:prefs", [this] (auto...) {
      Path::stringwrite (pathname_anklangrc(), json_stringify (prefs_, Writ::RELAXED | Writ::SKIP_EMPTYSTRING), true);
    });
  assert_return (telemetry_arena.reserved() >= telemetry_size);
  Block telemetry_header = telemetry_arena.allocate (64);
  assert_return (telemetry_arena.location() == uint64 (telemetry_header.block_start));
  const uint8_t header_sentinel[64] = {
    0xff, 0xff, 0xff, 0xff, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x0a, 0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0b, 0x0b,
    0x0c, 0x0c, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0d, 0x0e, 0x0e, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f,
  };
  assert_return (telemetry_header.block_length == sizeof (header_sentinel));
  memcpy (telemetry_header.block_start, header_sentinel, telemetry_header.block_length);
  if (!SERVER)
    SERVER = this;
}

ServerImpl::~ServerImpl ()
{
  fatal_error ("ServerImpl references must persist");
  if (SERVER == this)
    SERVER = nullptr;
}

String
ServerImpl::get_version ()
{
  return ase_version();
}

String
ServerImpl::get_build_id ()
{
  return ase_build_id();
}

String
ServerImpl::get_opus_version ()
{
  return wave_writer_opus_version();
}

String
ServerImpl::get_flac_version ()
{
  return wave_writer_flac_version();
}

String
ServerImpl::get_clap_version()
{
  return ClapDeviceImpl::clap_version();
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
  return ProjectImpl::create (projectname);
}

PropertyP
ServerImpl::access_preference (const String &ident)
{
  return Preference::find (ident);
}

ServerImplP
ServerImpl::instancep ()
{
  static ServerImplP *sptr = new ServerImplP (std::make_shared<ServerImpl>());
  return *sptr;
}

bool
ServerImpl::set_data (const String &key, const Value &value)
{
  const String ckey = canonify_key (key);
  if (ckey.size() && ckey[0] != '_')
    {
      const String filename = Path::join (Path::xdg_dir ("CONFIG"), "anklang", ckey);
      emit_event ("data", key);
      return Path::stringwrite (filename, value.as_string());
    }
  else
    return GadgetImpl::set_data (ckey, value);
}

Value
ServerImpl::get_data (const String &key) const
{
  const String ckey = canonify_key (key);
  if (ckey.size() && ckey[0] != '_')
    {
      const String filename = Path::join (Path::xdg_dir ("CONFIG"), "anklang", ckey);
      return Path::stringread (filename);
    }
  else
    return GadgetImpl::get_data (ckey);
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

// == FileCrawler ==
ResourceCrawlerP
Server::dir_crawler (const String &cwd)
{
  return FileCrawler::make_shared (cwd, true, false);
}

ResourceCrawlerP
Server::url_crawler (const String &url)
{
  if (main_config.web_socket_server) {
    String dir = main_config.web_socket_server->map_url (url);
    if (!dir.empty())
      return FileCrawler::make_shared (dir, false, false);
  }
  return nullptr;
}

// == Choice ==
Choice::Choice (String ident_, String label_, String blurb_, String notice_, String warning_) :
  ident (ident_.empty() ? string_to_identifier (label_) : ident_),
  label (label_), blurb (blurb_), notice (notice_), warning (warning_)
{
  assert_return (ident.empty() == false);
}

Choice::Choice (String ident_, IconString icon_, String label_, String blurb_, String notice_, String warning_) :
  ident (ident_.empty() ? string_to_identifier (label_) : ident_),
  icon (icon_), label (label_), blurb (blurb_), notice (notice_), warning (warning_)
{
  assert_return (ident.empty() == false);
}

Choice::Choice (IconString icon_, String label_, String blurb_) :
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
      // errno aliases
    case Error::NONE:			return _("OK");
    case Error::PERMS:
    case Error::IO:
    case Error::NO_MEMORY:
    case Error::NO_SPACE:
    case Error::NO_FILES:
    case Error::MANY_FILES:
    case Error::RETRY:
    case Error::NOT_DIRECTORY:
    case Error::FILE_NOT_FOUND:
    case Error::FILE_IS_DIR:
    case Error::FILE_EXISTS:
    case Error::FILE_BUSY:              break; // fallback to strerror
      // Ase specific errors
    case Error::INTERNAL:		return _("Internal error (please report)");
      // file errors
    case Error::FILE_EOF:		return _("End of file");
    case Error::FILE_OPEN_FAILED:	return _("Open failed");
    case Error::FILE_SEEK_FAILED:	return _("Seek failed");
    case Error::FILE_READ_FAILED:	return _("Read failed");
    case Error::FILE_WRITE_FAILED:	return _("Write failed");
      // content errors
    case Error::PARSE_ERROR:		return _("Parsing error");
    case Error::NO_HEADER:		return _("Failed to detect header");
    case Error::NO_SEEK_INFO:		return _("Failed to retrieve seek information");
    case Error::NO_DATA_AVAILABLE:	return _("No data available");
    case Error::DATA_CORRUPT:		return _("Data corrupt");
    case Error::WRONG_N_CHANNELS:	return _("Wrong number of channels");
    case Error::FORMAT_INVALID:		return _("Invalid format");
    case Error::FORMAT_UNKNOWN:		return _("Unknown format");
    case Error::DATA_UNMATCHED:		return _("Requested data values unmatched");
    case Error::CODEC_FAILURE:		return _("Codec failure");
    case Error::BROKEN_ARCHIVE:         return _("Broken archive");
    case Error::BAD_PROJECT:            return _("Not a valid project");
    case Error::NO_PROJECT_DIR:         return _("Missing project directory");
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
    case Error::WAVE_NOT_FOUND:		return _("No such wave");
    case Error::UNIMPLEMENTED:		return _("Functionality not implemented");
    case Error::INVALID_PROPERTY:	return _("Invalid object property");
    case Error::INVALID_MIDI_CONTROL:	return _("Invalid MIDI control type");
    case Error::OPERATION_BUSY:		return _("Operation already in prgress");
    }
  return strerror (int (error));
}

// Map errno onto Ase::Error.
Error
ase_error_from_errno (int sys_errno, Error fallback)
{
  switch (sys_errno)
    {
    case 0:             return Ase::Error::NONE;
    case ELOOP:
    case ENAMETOOLONG:
    case ENOENT:        return Ase::Error::FILE_NOT_FOUND;
    case EISDIR:        return Ase::Error::FILE_IS_DIR;
    case EROFS:
    case EPERM:
    case EACCES:        return Ase::Error::PERMS;
#ifdef ENODATA  /* GNU/kFreeBSD lacks this */
    case ENODATA:
#endif
    case ENOMSG:        return Ase::Error::FILE_EOF;
    case ENOMEM:        return Ase::Error::NO_MEMORY;
    case ENOSPC:        return Ase::Error::NO_SPACE;
    case ENFILE:        return Ase::Error::NO_FILES;
    case EMFILE:        return Ase::Error::MANY_FILES;
    case EFBIG:
    case ESPIPE:
    case EIO:           return Ase::Error::IO;
    case EEXIST:        return Ase::Error::FILE_EXISTS;
    case ETXTBSY:
    case EBUSY:         return Ase::Error::FILE_BUSY;
    case EAGAIN:
    case EINTR:         return Ase::Error::RETRY;
    case EFAULT:        return Ase::Error::INTERNAL;
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
static EnumInfo
musical_tuning_info (MusicalTuning musicaltuning)
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
static bool musical_tuning_info__ = EnumInfo::impl (musical_tuning_info);

String
ServerImpl::musical_tuning_blurb (MusicalTuning musicaltuning) const
{
  return EnumInfo::value_info (musicaltuning).blurb;
}

String
ServerImpl::musical_tuning_label (MusicalTuning musicaltuning) const
{
  return EnumInfo::value_info (musicaltuning).label;
}

static std::atomic<uint> user_note_id = 1;

uint64
ServerImpl::user_note (const String &text, const String &channel, UserNote::Flags flags, const String &rest)
{
  UserNote unote { user_note_id++, flags, channel.empty() ? "misc" : channel, text, rest };
  ValueR vrec;
  json_parse (json_stringify (unote, Writ::SKIP_EMPTYSTRING), vrec);
  this->emit_event ("usernote", "", vrec);
  String s;
  s += string_format ("%s: usernote[%04x]: %s: %s", program_alias(), unote.noteid, unote.channel, unote.text);
  if (!unote.rest.empty())
    s += " (" + unote.rest + ")";
  printerr ("%s\n", string_replace (s, "\n", "\t"));
  return unote.noteid;
}

bool
ServerImpl::user_reply (uint64 noteid, uint r)
{
  return false; // unhandled
}

ServerImpl::Block
ServerImpl::telemem_allocate (uint32 length) const
{
  return telemetry_arena.allocate (length);
}

void
ServerImpl::telemem_release (Block telememblock) const
{
  telemetry_arena.release (telememblock);
}

ptrdiff_t
ServerImpl::telemem_start () const
{
  return telemetry_arena.location();
}

static bool
validate_telemetry_segments (const TelemetrySegmentS &segments, size_t *payloadlength)
{
  *payloadlength = 0;
  const TelemetrySegment *last = nullptr;
  for (const auto &seg : segments)
    {
      if (last && seg.offset < last->offset + last->length)
        return false;   // check sorting and non-overlapping
      if (seg.offset < 0 || (seg.offset & 3) || seg.length <= 0 || (seg.length & 3) ||
          size_t (seg.offset + seg.length) > telemetry_size)
        return false;
      *payloadlength += seg.length;
      last = &seg;
    }
  return true;
}

ASE_CLASS_DECLS (TelemetryPlan);
class TelemetryPlan {
public:
  int32               interval_ms_ = -1;
  uint                timerid_ = 0;
  JsonapiBinarySender send_blob_;
  TelemetrySegmentS   segments_;
  const char         *telemem_ = nullptr;
  String              payload_;
  void send_telemetry();
  void setup (const char *start, size_t payloadlength, const TelemetrySegmentS &plan, int32 interval_ms);
  ~TelemetryPlan();
};
static CustomDataKey<TelemetryPlanP> telemetry_key;

bool
ServerImpl::broadcast_telemetry (const TelemetrySegmentS &segments, int32 interval_ms)
{
  size_t payloadlength = 0;
  if (!validate_telemetry_segments (segments, &payloadlength))
    {
      warning ("%s: invalid segment list", "Ase::ServerImpl::broadcast_telemetry");
      return false;
    }
  CustomDataContainer *cdata = jsonapi_connection_data();
  if (!cdata)
    {
      warning ("%s: cannot broadcast telemetry without jsonapi connection", "Ase::ServerImpl::broadcast_telemetry");
      return false;
    }
  TelemetryPlanP tplan = cdata->get_custom_data (&telemetry_key);
  if (!tplan)
    {
      tplan = std::make_shared<TelemetryPlan> ();
      cdata->set_custom_data (&telemetry_key, tplan);
      tplan->send_blob_ = jsonapi_connection_sender();
    }
  tplan->setup ((const char*) telemetry_arena.location(), payloadlength, segments, interval_ms);
  return true;
}

void
TelemetryPlan::setup (const char *start, size_t payloadlength, const TelemetrySegmentS &segments, int32 interval_ms)
{
  if (timerid_ == 0 || interval_ms_ != interval_ms)
    {
      if (timerid_)
        main_loop->remove (timerid_);
      auto send_telemetry = [this] () { this->send_telemetry(); return true; };
      interval_ms_ = interval_ms;
      timerid_ = interval_ms <= 0 || segments.empty() ? 0 : main_loop->exec_timer (send_telemetry, interval_ms, interval_ms);
    }
  if (timerid_)
    {
      telemem_ = start;
      segments_ = segments;
      payload_.resize (payloadlength);
    }
  else
    {
      telemem_ = nullptr;
      segments_ = {};
      payload_.clear();
    }
}

void
TelemetryPlan::send_telemetry ()
{
  char *data = &payload_[0];
  size_t datapos = 0;
  for (const auto &seg : segments_)     // offsets and lengths were validated earlier
    {
      memcpy (data + datapos, telemem_ + seg.offset, seg.length);
      datapos += seg.length;
    }
  send_blob_ (payload_);
}

TelemetryPlan::~TelemetryPlan()
{
  if (timerid_)
    {
      main_loop->remove (timerid_);
      timerid_ = 0;
    }
}

} // Ase
