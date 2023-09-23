// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_API_HH__
#define __ASE_API_HH__

#include <ase/value.hh>

/// The Anklang C++ API namespace
namespace Ase {

// == Property hint constants ==
constexpr const char GUIONLY[] = ":G:r:w:";     ///< GUI READABLE WRITABLE
constexpr const char STORAGE[] = ":S:r:w:";     ///< STORAGE READABLE WRITABLE
constexpr const char STANDARD[] = ":S:G:r:w:";  ///< STORAGE GUI READABLE WRITABLE

/// Common base type for polymorphic classes managed by `std::shared_ptr<>`.
class SharedBase : public virtual VirtualBase,
                   public virtual std::enable_shared_from_this<SharedBase>
{};

/// Enum representing Error states.
enum class Error : int32_t {
  NONE                          = 0,
  PERMS                         = EPERM,
  IO                            = EIO,
  // resource exhaustion
  NO_MEMORY                     = ENOMEM,
  NO_SPACE                      = ENOSPC,
  NO_FILES                      = ENFILE,
  MANY_FILES                    = EMFILE,
  RETRY                         = EINTR,
  // file errors
  NOT_DIRECTORY                 = ENOTDIR,
  FILE_NOT_FOUND                = ENOENT,
  FILE_IS_DIR                   = EISDIR,
  FILE_EXISTS                   = EEXIST,
  FILE_BUSY                     = EBUSY,
  // Ase specific errors
  INTERNAL                      = 0x30000000,
  UNIMPLEMENTED,
  // File related errors
  FILE_EOF                      = 0x30001000,
  FILE_OPEN_FAILED,
  FILE_SEEK_FAILED,
  FILE_READ_FAILED,
  FILE_WRITE_FAILED,
  // content errors
  PARSE_ERROR                   = 0x30002000,
  NO_HEADER,
  NO_SEEK_INFO,
  NO_DATA_AVAILABLE,
  DATA_CORRUPT,
  WRONG_N_CHANNELS,
  FORMAT_INVALID,
  FORMAT_UNKNOWN,
  DATA_UNMATCHED,
  CODEC_FAILURE,
  BROKEN_ARCHIVE,
  BAD_PROJECT,
  NO_PROJECT_DIR,
  // Device errors
  DEVICE_NOT_AVAILABLE          = 0x30003000,
  DEVICE_ASYNC,
  DEVICE_BUSY,
  DEVICE_FORMAT,
  DEVICE_BUFFER,
  DEVICE_LATENCY,
  DEVICE_CHANNELS,
  DEVICE_FREQUENCY,
  DEVICES_MISMATCH,
  // miscellaneous errors
  WAVE_NOT_FOUND                = 0x30004000,
  INVALID_PROPERTY,
  INVALID_MIDI_CONTROL,
  OPERATION_BUSY,
};
ASE_DEFINE_ENUM_EQUALITY (Error);
constexpr bool operator! (Error error)  { return !std::underlying_type_t<Error> (error); }
const char* ase_error_blurb      (Error error);
Error       ase_error_from_errno (int sys_errno, Error fallback = Error::IO);

/// Musical tunings, see: http://en.wikipedia.org/wiki/Musical_tuning
enum class MusicalTuning : uint8 {
  // Equal Temperament: http://en.wikipedia.org/wiki/Equal_temperament
  OD_12_TET, OD_7_TET, OD_5_TET,
  // Rational Intonation: http://en.wikipedia.org/wiki/Just_intonation
  DIATONIC_SCALE, INDIAN_SCALE, PYTHAGOREAN_TUNING, PENTATONIC_5_LIMIT, PENTATONIC_BLUES, PENTATONIC_GOGO,
  // Meantone Temperament: http://en.wikipedia.org/wiki/Meantone_temperament
  QUARTER_COMMA_MEANTONE, SILBERMANN_SORGE,
  // Well Temperament: http://en.wikipedia.org/wiki/Well_temperament
  WERCKMEISTER_3, WERCKMEISTER_4, WERCKMEISTER_5, WERCKMEISTER_6, KIRNBERGER_3, YOUNG,
};
ASE_DEFINE_ENUM_EQUALITY (MusicalTuning);

/// Representation of one possible choice for selection properties.
struct Choice {
  String ident;   ///< Identifier used for serialization (may be derived from untranslated label).
  String icon;    ///< Icon (64x64 pixels) or unicode symbol (possibly wide).
  String label;   ///< Preferred user interface name.
  String blurb;   ///< Short description for overviews.
  String notice;  ///< Additional information of interest.
  String warning; ///< Potential problem indicator.
  Choice () = default;
  Choice (IconString icon, String label, String blurb = "");
  Choice (String ident, IconString icon, String label, String blurb = "", String notice = "", String warning = "");
  Choice (String ident, String label, String blurb = "", String notice = "", String warning = "");
};

/// Convenience ChoiceS construciton helper.
ChoiceS& operator+= (ChoiceS &choices, Choice &&newchoice);

/// Telemetry segment location.
struct TelemetryField {
  String name;          ///< Names like "bpm", etc
  String type;          ///< Types like "i32", "f32", "f64"
  int32  offset = 0;    ///< Position in bytes.
  int32  length = 0;    ///< Length in bytes.
};

/// Base type for classes with Event subscription.
class Emittable : public virtual SharedBase {
public:
  struct Connection : EventConnectionP {
    bool             connected  () const;
    void             disconnect () const;
  };
  virtual void       emit_event  (const String &type, const String &detail, const ValueR fields = {}) = 0;
  ASE_USE_RESULT
  virtual Connection on_event    (const String &eventselector, const EventHandler &eventhandler) = 0;
  void               js_trigger  (const String &eventselector, JsTrigger callback);
};

/// A Property allows querying, setting and monitoring of an object property.
class Property : public virtual Emittable {
protected:
  virtual         ~Property       () = 0;
public:
  virtual String   ident          () = 0;          ///< Unique name (per owner) of this Property.
  virtual String   label          () = 0;          ///< Preferred user interface name.
  virtual String   nick           () = 0;          ///< Abbreviated user interface name, usually not more than 6 characters.
  virtual String   unit           () = 0;          ///< Units of the values within range.
  virtual String   hints          () = 0;          ///< Hints for parameter handling.
  virtual String   group          () = 0;          ///< Group name for parameters of similar function.
  virtual String   blurb          () = 0;          ///< Short description for user interface tooltips.
  virtual String   descr          () = 0;          ///< Elaborate description for help dialogs.
  virtual double   get_min        () = 0;          ///< Get the minimum property value, converted to double.
  virtual double   get_max        () = 0;          ///< Get the maximum property value, converted to double.
  virtual double   get_step       () = 0;          ///< Get the property value stepping, converted to double.
  virtual void     reset          () = 0;          ///< Assign default as normalized property value.
  virtual Value    get_value      () = 0;          ///< Get the native property value.
  virtual bool     set_value      (const Value &v) = 0; ///< Set the native property value.
  virtual double   get_normalized () = 0;          ///< Get the normalized property value, converted to double.
  virtual bool     set_normalized (double v) = 0;  ///< Set the normalized property value as double.
  virtual String   get_text       () = 0;          ///< Get the current property value, converted to a text String.
  virtual bool     set_text       (String v) = 0;  ///< Set the current property value as a text String.
  virtual bool     is_numeric     () = 0;          ///< Whether the property settings can be represented as a floating point number.
  virtual ChoiceS  choices        () = 0;          ///< Enumerate choices for choosable properties.
};

/// Base type for classes with Property interfaces.
class Object : public virtual Emittable {
protected:
  virtual      ~Object () = 0;
public:
};

/// Base type for classes that have a Property.
class Gadget : public virtual Object {
public:
  // Hierarchical parenting.
  virtual GadgetImpl* _parent           () const = 0;             ///< Retrieve parent container.
  virtual void        _set_parent       (GadgetImpl *parent) = 0; ///< Assign parent container.
  ProjectImpl*        _project          () const;                 ///< Find Project in parent ancestry.
  // Naming
  virtual String      type_nick         () const = 0;
  virtual String      name              () const = 0;
  virtual void        name              (String newname) = 0;
  // Properties
  virtual StringS     list_properties   ();                 ///< List all property identifiers.
  virtual PropertyP   access_property   (String ident);     ///< Retrieve handle for a Property.
  virtual PropertyS   access_properties () = 0;             ///< Retrieve handles for all properties.
  Value               get_value         (String ident);     ///< Get native property value.
  bool                set_value         (String ident, const Value &v); ///< Set native property value.
  /// Assign session data, prefix ephemerals with '_'.
  virtual bool        set_data          (const String &key, const Value &v) = 0;
  /// Retrieve session data.
  virtual Value       get_data          (const String &key) const = 0;
};

/// Info for device types.
struct DeviceInfo {
  String uri;          ///< Unique identifier for de-/serialization.
  String name;         ///< Preferred user interface name.
  String category;     ///< Category to allow grouping for processors of similar function.
  String description;  ///< Elaborate description for help dialogs.
  String website_url;  ///< Website of/about this Processor.
  String creator_name; ///< Name of the creator.
  String creator_url;  ///< Internet contact of the creator.
};

/// Interface to access Device instances.
class Device : public virtual Gadget {
public:
  // internal
  Track*                  _track             () const;          ///< Find Track in parent ancestry.
  virtual AudioProcessorP _audio_processor   () const = 0;      ///< Retrieve the corresponding AudioProcessor.
  virtual void            _set_event_source  (AudioProcessorP esource) = 0;
  virtual void            _activate          () = 0;            ///< Add AudioProcessor to the Engine and start processing.
  virtual void            _deactivate        () = 0;            ///< Stop processing the corresponding AudioProcessor.
  virtual void            _disconnect_remove () = 0;            ///< Disconnect the device and remove all object references.
  // exported
  virtual bool       is_active     () = 0;      ///< Check whether this is the active synthesis engine project.
  virtual DeviceInfo device_info   () = 0;      ///< Describe this Device type.
  void               remove_self   ();          ///< Remove device from its container.
  // GUI handling
  virtual void       gui_toggle    () = 0;      ///< Toggle GUI display.
  virtual bool       gui_supported () = 0;      ///< Has GUI display facilities.
  virtual bool       gui_visible   () = 0;      ///< Is GUI currently visible.
};

/// Interface to access NativeDevice instances.
class NativeDevice : public virtual Device {
public:
  // subdevice handling
  virtual bool        is_combo_device   () = 0;                      ///< Retrieve wether this NativeDevice handles sub devices.
  virtual DeviceS     list_devices      () = 0;                      ///< List devices in order of processing, notified via "devices".
  DeviceInfoS         list_device_types ();                          ///< List registered Device types with their unique uri.
  virtual void        remove_device     (Device &sub) = 0;           ///< Remove a directly contained device.
  virtual DeviceP     append_device     (const String &uri) = 0;     ///< Append a new device, see list_device_types().
  virtual DeviceP     insert_device     (const String &uri,
                                         Device &beforesibling) = 0; ///< Insert a new device, before `beforesibling`.
};

/// Part specific note event representation.
struct ClipNote {
  int32  id = 0;            /// ID, > 0
  int8   channel = 0;       /// MIDI Channel
  int8   key = 0;           /// Musical note as MIDI key, 0 .. 127
  bool   selected = 0;      /// UI selection flag
  int64  tick = 0;          /// Position in ticks
  int64  duration = 0;      /// Duration in number of ticks
  float  velocity = 0;      /// Velocity, 0 .. +1
  float  fine_tune = 0;     /// Fine Tune, -100 .. +100
  bool   operator== (const ClipNote&) const;
};

/// Container for MIDI note and control events.
class Clip : public virtual Gadget {
public:
  virtual int64     start_tick     () const = 0; ///< Get the first tick intended for playback (this is >= 0), changes on `notify:start_tick`.
  virtual int64     stop_tick      () const = 0; ///< Get the tick to stop playback, not events should be played after this, changes on `notify:stop_tick`.
  virtual int64     end_tick       () const = 0; ///< Get the end tick, this tick is past any event ticks, changes on `notify:end_tick`.
  virtual void      assign_range   (int64 starttick, int64 stoptick) = 0; ///< Change start_tick() and stop_tick(); emits `notify:start_tick`, `notify:stop_tick`.
  virtual ClipNoteS all_notes      () const = 0; ///< List all notes of this Clip; changes on `notify:all_notes`.
  /// Change note `id` according to the arguments or add a new note if `id` < 0; emits `notify:notes`.
  virtual int32     change_batch   (const ClipNoteS &notes, const String &undogroup = "") = 0; ///< Insert, change, delete in a batch.
  virtual ClipNoteS list_all_notes () = 0; ///< List all notes of this Clip; changes on `notify:notes`.
};

/// Container for Clip objects and sequencing information.
class Track : public virtual Device {
public:
  virtual int32           midi_channel        () const = 0;          ///< Midi channel assigned to this track, 0 uses internal per-track channel.
  virtual void            midi_channel        (int32 midichannel) = 0;
  virtual bool            mute                () const = 0;          ///< Whether the track is muted
  virtual void            mute                (bool newmute) = 0;
  virtual bool            solo                () const = 0;          ///< Whether the track is solo
  virtual void            solo                (bool newsolo) = 0;
  virtual double          volume              () const = 0;          ///< Volume of the track [0..1]
  virtual void            volume              (double newvolume) = 0;
  virtual bool            is_master           () const = 0;          ///< Flag set on the main output track.
  virtual ClipS           launcher_clips      () = 0;                ///< Retrieve the list of clips that can be directly played.
  virtual DeviceP         access_device       () = 0;                ///< Retrieve Device handle for this track.
  virtual MonitorP        create_monitor      (int32 ochannel) = 0;  /// Create signal monitor for an output channel.
  virtual TelemetryFieldS telemetry           () const = 0;          ///< Retrieve track telemetry locations.
};

/// Bits representing a selection of probe sample data features.
struct ProbeFeatures {
  bool probe_range;     ///< Provide sample range probes.
  bool probe_energy;    ///< Provide sample energy measurement.
  bool probe_samples;   ///< Provide probe with bare sample values.
  bool probe_fft;       ///< Provide FFT analysis probe.
};

/// Interface for monitoring output signals.
class Monitor : public virtual Gadget {
public:
  virtual DeviceP get_output         () = 0;            ///< Retrieve output device the Monitor is connected to.
  virtual int32   get_ochannel       () = 0;            ///< Retrieve output channel the Monitor is connected to.
  virtual int64   get_mix_freq       () = 0;            ///< Mix frequency at which monitor values are calculated.
  virtual int64   get_frame_duration () = 0;            ///< Frame duration in µseconds for the calculation of monitor values.
  //int64         get_shm_offset     (MonitorField fld);  ///< Offset into shared memory for MonitorField values of `ochannel`.
  //void          set_probe_features (ProbeFeatures pf);  ///< Configure probe features.
  //ProbeFeatures get_probe_features ();                  ///< Get configured probe features.
};

/// Projects support loading, saving, playback and act as containers for all other sound objects.
class Project : public virtual Device {
public:
  virtual void            discard        () = 0;       ///< Discard project and associated resources.
  virtual void            start_playback () = 0;       ///< Start playback of a project, requires active sound engine.
  virtual void            stop_playback  () = 0;       ///< Stop project playback.
  virtual bool            is_playing     () = 0;       ///< Check whether a project is currently playing (song sequencing).
  virtual TrackP          create_track   () = 0;       ///< Create and append a new Track.
  virtual bool            remove_track   (Track&) = 0; ///< Remove a track owned by this Project.
  virtual TrackS          all_tracks     () = 0;       ///< List all tracks of the project.
  virtual TrackP          master_track   () = 0;       ///< Retrieve the master track.
  virtual Error           save_project   (const String &filename, bool collect) = 0; ///< Store Project and collect external files.
  virtual String          saved_filename () = 0;       ///< Retrieve filename for save or from load.
  virtual Error           load_project   (const String &filename) = 0; ///< Load project from file `filename`.
  virtual TelemetryFieldS telemetry      () const = 0; ///< Retrieve project telemetry locations.
  virtual void            group_undo     (const String &undoname) = 0; ///< Merge upcoming undo steps.
  virtual void            ungroup_undo   () = 0;                       ///< Stop merging undo steps.
  virtual void            undo           () = 0;       ///< Undo the last project modification.
  virtual bool            can_undo       () = 0;       ///< Check if any undo steps have been recorded.
  virtual void            redo           () = 0;       ///< Redo the last undo modification.
  virtual bool            can_redo       () = 0;       ///< Check if any redo steps have been recorded.
  static ProjectP         last_project   ();
};

enum class ResourceType {
  FOLDER = 1,
  FILE,
};

/// Description of a resource, possibly nested.
struct Resource {
  ResourceType type = {};       ///< Resource classification.
  String       label;           ///< UI display name.
  String       uri;             ///< Unique resource identifier.
  int64        size = 0;        ///< Resource size.
  int64        mtime = 0;       ///< Modification time in milliseconds.
};

/// Helper to crawl hierarchical resources.
class ResourceCrawler : public virtual Object {
public:
  virtual ResourceS list_entries   () = 0;                      ///< List entries of a folder.
  virtual Resource  current_folder () = 0;                      ///< Describe current folder.
  virtual void      assign         (const String &path) = 0;    ///< Move to a different path.
  /// Return absolute path, slash-terminated if directory, constrain to existing paths.
  virtual String    canonify       (const String &cwd, const String &fragment, bool constraindir, bool constrainfile) = 0;
};

/// Contents of user interface notifications.
struct UserNote {
  enum Flags { APPEND, CLEAR, TRANSIENT };
  uint   noteid = 0;
  Flags  flags = APPEND;
  String channel, text, rest;
};

/// Telemetry segment location.
struct TelemetrySegment {
  int32 offset = 0;     ///< Position in bytes.
  int32 length = 0;     ///< Length in bytes.
};

/// Central singleton, serves as API entry point.
class Server : public virtual Gadget {
public:
  // singleton
  using ServerP = std::shared_ptr<Server>;
  static Server& instance           ();         ///< Retrieve global Server instance.
  static ServerP instancep          ();         ///< Retrieve global Server instance as std::shared_ptr.
  virtual void   shutdown           () = 0;     ///< Shutdown ASE.
  virtual String get_version        () = 0;     ///< Retrieve ASE version.
  virtual String get_build_id       () = 0;     ///< Retrieve ASE build id.
  virtual String get_opus_version   () = 0;     ///< Retrieve Opus handler version.
  virtual String get_flac_version   () = 0;     ///< Retrieve FLAC handler version.
  virtual String get_clap_version   () = 0;     ///< Retrieve CLAP support version.
  virtual String error_blurb          (Error error) const = 0;
  virtual String musical_tuning_label (MusicalTuning musicaltuning) const = 0;
  virtual String musical_tuning_blurb (MusicalTuning musicaltuning) const = 0;
  virtual uint64 user_note            (const String &text, const String &channel = "misc", UserNote::Flags flags = UserNote::TRANSIENT, const String &rest = "") = 0;
  virtual bool   user_reply           (uint64 noteid, uint r) = 0;
  virtual bool   broadcast_telemetry  (const TelemetrySegmentS &segments,
                                       int32 interval_ms) = 0;   ///< Broadcast telemetry memory segments to the current Jsonipc connection.
  virtual StringS   list_preferences  () = 0;                    ///< Retrieve a list of all preference identifiers.
  virtual PropertyP access_preference (const String &ident) = 0; ///< Retrieve property handle for a Preference identifier.
  String            engine_stats      ();                        ///< Print engine state.
  // projects
  virtual ProjectP last_project   () = 0;       ///< Retrieve the last created project.
  virtual ProjectP create_project (String projectname) = 0; ///< Create a new project (name is modified to be unique if necessary.
  // Browsing
  ResourceCrawlerP dir_crawler    (const String &cwd = "");  ///< Create crawler to navigate directories.
  ResourceCrawlerP url_crawler    (const String &url = "/"); ///< Create crawler to navigate URL contents.
};
#define ASE_SERVER      (::Ase::Server::instance())

} // Ase

#endif // __ASE_API_HH__
