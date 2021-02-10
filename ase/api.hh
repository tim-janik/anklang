// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_API_HH__
#define __ASE_API_HH__

#include <ase/value.hh>

namespace Ase {

extern const String STORAGE; // ":r:w:S:";
extern const String STANDARD; // ":r:w:S:G:";

using StringS = std::vector<std::string>;
struct Event;
class EventConnection;
using EventHandler = std::function<void (const Event&)>;

/// Common base type for polymorphic classes managed by `std::shared_ptr<>`.
struct SharedBase : public virtual VirtualBase,
                    public virtual std::enable_shared_from_this<SharedBase> {
};
using SharedBaseP = std::shared_ptr<SharedBase>;

// Error enum
enum class Error {
  NONE                      = 0, // Enum (0, "", _("OK")),
  INTERNAL                  ,//= Enum (11, "", _("Internal error (please report)")),
  UNKNOWN                   ,//= Enum (12, "", _("Unknown error")),
  IO                        ,//= Enum (13, "", _("Input/output error")),
  PERMS                     ,//= Enum (14, "", _("Insufficient permissions")),
  // out of resource conditions
  NO_MEMORY                 ,//= Enum (101, "", _("Out of memory")),
};

/// Details about property choice values.
struct Choice {
  String ident;         ///< Identifier used for serialization.
  String label;         ///< Preferred user interface name.
  String subject;       ///< Subject line, a brief one liner or elaborate title.
  String icon;          ///< Stringified icon, SVG and PNG should be supported (64x64 pixels recommended).
};
using ChoiceS = std::vector<Choice>;

/// Base type for classes with Event subscription.
class Emittable : public virtual SharedBase {
public:
  struct Connection : std::weak_ptr<EventConnection> {
    friend class Emittable;
    bool             connected  () const;
    void             disconnect () const;
  };
  virtual void       emit_event  (const String &type, const String &detail, ValueR fields) = 0;
  virtual Connection on_event    (const String &eventselector, const EventHandler &eventhandler) = 0;
  void               js_trigger  (const String &eventselector, JsTrigger callback);
};

/// A Property allows querying, setting and monitoring of an object property.
class Property : public virtual Emittable {
protected:
  virtual         ~Property       () = 0;
public:
  virtual String   identifier     () = 0;          ///< Unique name (per owner) of this Property.
  virtual String   label          () = 0;          ///< Preferred user interface name.
  virtual String   nick           () = 0;          ///< Abbreviated user interface name, usually not more than 6 characters.
  virtual String   unit           () = 0;          ///< Units of the values within range.
  virtual String   hints          () = 0;          ///< Hints for parameter handling.
  virtual String   group          () = 0;          ///< Group name for parameters of similar function.
  virtual String   blurb          () = 0;          ///< Short description for user interface tooltips.
  virtual String   description    () = 0;          ///< Elaborate description for help dialogs.
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
using PropertyP = std::shared_ptr<Property>;
using PropertyS = std::vector<PropertyP>;

// Preferences
struct Preferences {
  // Synthesis Settings
  String pcm_driver;                    ///< Driver and device to be used for PCM input and output.
  int32  synth_latency = 5;             ///< Processing duration between input and output of a single sample, smaller values increase CPU load.
  int32  synth_mixing_freq = 48000;     ///< Unused, synthesis mixing frequency is always 48000 Hz.
  int32  synth_control_freq = 1500;     ///< Unused frequency setting.
  // MIDI
  String midi_driver;                   ///< Driver and device to be used for MIDI input and output.
  bool   invert_sustain = false;
  // Default Values
  String author_default;                ///< Default value for 'Author' fields.
  String license_default;               ///< Default value for 'License' fields.
  String sample_path;                   ///< Search path of directories, seperated by ";", used to find audio samples.
  String effect_path;                   ///< Search path of directories, seperated by ";", used to find effect files.
  String instrument_path;               ///< Search path of directories, seperated by ";", used to find instrument files.
  String plugin_path;                   ///< Search path of directories, seperated by \";\", used to find plugins.
                                        ///< This path is searched for in addition to the standard plugin location on this system.
private:
  PropertyS access_properties (const EventHandler&);    ///< Retrieve handles for all properties.
  friend class ServerImpl;
};

/// Driver information for PCM and MIDI handling.
struct DriverEntry {
  String devid;
  String device_name;
  String capabilities;
  String device_info;
  String notice;
  int32  priority = 0;
  bool   readonly = false;
  bool   writeonly = false;
  bool   modem = false;
};
using DriverEntryS = std::vector<DriverEntry>;

/// Base type for classes with Property interfaces.
class Object : public virtual Emittable {
protected:
  virtual      ~Object () = 0;
public:
};
using ObjectP = std::shared_ptr<Object>;

/// Base type for classes that have a Property.
class Gadget : public virtual Object {
public:
  virtual std::string type_nick         () const = 0;
  virtual std::string name              () const = 0;
  virtual void        name              (std::string newname) = 0;
  // Parameters
  virtual StringS     list_properties   () = 0;             ///< List all property identifiers.
  virtual PropertyP   access_property   (String ident) = 0; ///< Retrieve handle for a Property.
  virtual PropertyS   access_properties () = 0;             ///< Retrieve handles for all properties.
};
using GadgetP = std::shared_ptr<Gadget>;

/// Container for MIDI note and control events.
class Clip : public virtual Object {
public:
  virtual int32 start_tick () = 0; ///< Get the first tick intended for playback (this is >= 0), changes on `notify:start_tick`.
  virtual int32 stop_tick  () = 0; ///< Get the tick to stop playback, not events should be played after this, changes on `notify:stop_tick`.
  virtual int32 end_tick   () = 0; ///< Get the end tick, this tick is past any event ticks, changes on `notify:end_tick`.
};
using ClipP = std::shared_ptr<Clip>;

/// Container for Clip objects and sequencing information.
class Track : public virtual Object {
public:
  virtual int32 midi_channel () const = 0;      ///< Midi channel assigned to this track, 0 uses internal per-track channel.
  virtual void  midi_channel (int32 midichannel) = 0;
};
using TrackP = std::shared_ptr<Track>;

/// Projects support loading, saving, playback and act as containers for all other sound objects.
class Project : public virtual Gadget {
public:
  using ProjectP = std::shared_ptr<Project>;
  virtual void destroy        () = 0;   ///< Release project and associated resources.
  virtual void start_playback () = 0;   ///< Start playback of a project, requires active sound engine.
  virtual void stop_playback  () = 0;   ///< Stop project playback.
  virtual bool is_playing     () = 0;   ///< Check whether a project is currently playing (song sequencing).
  static ProjectP create       (const String &projectname);
  static ProjectP last_project ();
};
using ProjectP = Project::ProjectP;

/// Central singleton, serves as API entry point.
class Server : public virtual Gadget {
public:
  // singleton
  using ServerP = std::shared_ptr<Server>;
  static Server& instance           ();         ///< Retrieve global Server instance.
  static ServerP instancep          ();         ///< Retrieve global Server instance as std::shared_ptr.
  virtual void   shutdown           () = 0;     ///< Shutdown ASE.
  virtual String get_version        () = 0;     ///< Retrieve ASE version.
  virtual String get_vorbis_version () = 0;     ///< Retrieve Vorbis handler version.
  virtual String get_mp3_version    () = 0;     ///< Retrieve MP3 handler version.
  // preferences
  virtual PropertyS   access_prefs  () = 0;     ///< Retrieve property handles for Preferences fields.
  // projects
  virtual ProjectP last_project   () = 0;       ///< Retrieve the last created project.
  virtual ProjectP create_project (String projectname) = 0; ///< Create a new project (name is modified to be unique if necessary.
  // drivers
  virtual DriverEntryS list_pcm_drivers  () = 0; ///< List available drivers for PCM input/output handling.
  virtual DriverEntryS list_midi_drivers () = 0; ///< List available drivers for MIDI input/output handling.
  // testing
  void         set_session_data (const String &key, const Value &v); ///< Assign session data.
  const Value& get_session_data (const String &key) const;           ///< Retrieve session data.
};
using ServerP = Server::ServerP;
#define ASE_SERVER      (::Ase::Server::instance())

} // Ase

#endif // __ASE_API_HH__
