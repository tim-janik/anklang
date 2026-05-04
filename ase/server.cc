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
#include "sndfile.hh"
#include "wave.hh"
#include "internal.hh"
#include <atomic>

namespace Ase {

// == ServerImpl ==
static constexpr size_t telemetry_size = 4 * 1024 * 1024;

ServerImpl *SERVER = nullptr;

ServerImpl::ServerImpl () :
  telemetry_arena (telemetry_size)
{
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
ServerImpl::get_sndfile_version ()
{
  return libsndfile_version();
}

void
ServerImpl::shutdown ()
{
  // defer quit() slightly, so remote calls are still completed
  main_loop->add ([] () { main_loop->quit (0); }, std::chrono::milliseconds (5), LoopPriority::NORMAL);
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

StringS
ServerImpl::list_preferences ()
{
  const CStringS list = Preference::list();
  return { std::begin (list), std::end (list) };
}

PropertyP
ServerImpl::access_preference (const String &ident)
{
  return Preference::find (ident);
}

UiConfig
ServerImpl::ui_config ()
{
  UiConfig config;
  config.has_ui_tests = !App.ui_tests.empty();
  config.auto_exit = config.has_ui_tests; // Auto-exit when UI tests are present
  return config;
}

// UI test state tracking
static struct {
  StringS  tests;               // All requested test names
  ssize_t  current = -1;        // Index of next test to run
  bool     has_failed = false;  // True if any test failed
} ui_test_state;

String
ServerImpl::ui_test_fetch ()
{
  // Initialize tests on first call
  if (ui_test_state.tests.empty() && !App.ui_tests.empty()) {
    ui_test_state.tests = App.ui_tests;
    ui_test_state.current = -1;
    ui_test_state.has_failed = false;
  }
  // Keep running tests only as long they pass
  if (ui_test_state.has_failed || ui_test_state.current + 1 >= ui_test_state.tests.size())
    return "";
  // Yield current test name
  const String testname = ui_test_state.tests[++ui_test_state.current];
  printerr ("%s %s\n", "  CHECKING ", testname);
  return testname;
}

void
ServerImpl::ui_test_report (const String &testname, bool success)
{
  if (success && ui_test_state.current >= 0 &&
      ui_test_state.current < ui_test_state.tests.size() &&
      testname == ui_test_state.tests[ui_test_state.current])
    printerr ("%s %s\n", "  PASS     ", testname);
  else {
    printerr ("%s %s\n", "  FAIL     ", testname);
    ui_test_state.has_failed = true;
  }
  // Exit if all tests completed or failed
  if (ui_test_state.current >= ui_test_state.tests.size() || ui_test_state.has_failed)
    main_loop->add ([] () { main_loop->quit (ui_test_state.has_failed ? 1 : 0); });
}

String
ServerImpl::ui_js_fetch ()
{
  return App.ui_js;
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
  if (App.web_socket_server) {
    String dir = App.web_socket_server->map_url (url);
    if (!dir.empty())
      return FileCrawler::make_shared (dir, false, false);
  }
  return nullptr;
}

String
Server::engine_stats ()
{
  const String s = "Unused"; // TODO: get stats from trkn
  printerr ("Server::engine_stats:\n%s\n", s);
  return s;
}

void
Server::exit_program (int status)
{
  main_loop->quit (status);
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
      // errno aliases are left to strerror
    case Error::NONE:			return _("OK");
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
    default:
      return strerror (int (error));
    }
}

// Map errno onto Ase::Error.
Error
ase_error_from_errno (int sys_errno, Error fallback)
{
  if (sys_errno < int (Error::INTERNAL))
    return Error (sys_errno); // Error includes errnos
  else
    return fallback;
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
  json_parse (json_stringify (unote), vrec);
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
class TelemetryPlan : public std::enable_shared_from_this<TelemetryPlan> {
public:
  int32               interval_ms_ = -1;
  LoopID              timerid_ = LoopID::INVALID;
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
  if (timerid_ == LoopID::INVALID || interval_ms_ != interval_ms)
    {
      if (timerid_ != LoopID::INVALID)
        main_loop->cancel (timerid_);
      auto tplan = shared_from_this();
      auto send_telemetry = [tplan] () { tplan->send_telemetry(); return true; };
      interval_ms_ = interval_ms;
      timerid_ = interval_ms <= 0 || segments.empty() ? LoopID::INVALID : main_loop->add (send_telemetry, std::chrono::milliseconds (interval_ms));
    }
  if (timerid_ != LoopID::INVALID)
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
  // Safety check: don't send if telemetry memory or segments are invalid
  if (!telemem_ || segments_.empty() || payload_.empty())
    return;
  char *data = &payload_[0];
  size_t datapos = 0;
  for (const auto &seg : segments_)     // offsets and lengths were validated earlier
    {
      memcpy (data + datapos, telemem_ + seg.offset, seg.length);
      datapos += seg.length;
    }
  // send_blob_ handles the case where the connection is closed (returns false)
  (void) send_blob_ (payload_);
}

TelemetryPlan::~TelemetryPlan()
{
  if (timerid_ != LoopID::INVALID)
    {
      // Only cancel if the loop is still running
      if (!main_loop->has_quit())
        main_loop->cancel (timerid_);
      timerid_ = LoopID::INVALID;
    }
}

} // Ase
