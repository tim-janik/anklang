// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PROCESSOR_HH__
#define __ASE_PROCESSOR_HH__

#include <ase/gadget.hh>
#include <ase/midievent.hh>
#include <ase/datautils.hh>
#include <ase/properties.hh>
#include <ase/engine.hh>
#include <ase/atomics.hh>
#include <any>

namespace Ase {

/// ID type for AudioProcessor parameters, the ID numbers are user assignable.
enum class ParamId : uint32 {};
ASE_DEFINE_ENUM_EQUALITY (ParamId);

/// ID type for AudioProcessor input buses, buses are numbered with increasing index.
enum class IBusId : uint16 {};
ASE_DEFINE_ENUM_EQUALITY (IBusId);

/// ID type for AudioProcessor output buses, buses are numbered with increasing index.
enum class OBusId : uint16 {};
ASE_DEFINE_ENUM_EQUALITY (OBusId);

/// Detailed information and common properties of AudioProcessor subclasses.
struct AudioProcessorInfo {
  CString label;        ///< Preferred user interface name.
  CString version;      ///< Version identifier.
  CString category;     ///< Category to allow grouping for processors of similar function.
  CString blurb;        ///< Short description for overviews.
  CString description;  ///< Elaborate description for help dialogs.
  CString website_url;  ///< Website of/about this AudioProcessor.
  CString creator_name; ///< Name of the creator.
  CString creator_url;  ///< Internet contact of the creator.
};

/// Add an AudioProcessor derived type to the audio processor registry.
template<typename T> CString register_audio_processor (const char *aseid = nullptr);

/// Detailed information and common properties of parameters.
struct ParamInfo {
  CString    ident;        ///< Identifier used for serialization.
  CString    label;        ///< Preferred user interface name.
  CString    nick;         ///< Abbreviated user interface name, usually not more than 6 characters.
  CString    unit;         ///< Units of the values within range.
  CString    hints;        ///< Hints for parameter handling.
  CString    group;        ///< Group for parameters of similar function.
  CString    blurb;        ///< Short description for user interface tooltips.
  CString    description;  ///< Elaborate description for help dialogs.
  using MinMax = std::pair<double,double>;
  void       clear       ();
  MinMax     get_minmax  () const;
  double     get_stepping() const;
  double     get_initial () const;
  void       get_range   (double &fmin, double &fmax, double &fstep) const;
  void       set_range   (double fmin, double fmax, double fstep = 0);
  void       set_choices (const ChoiceS &centries);
  void       set_choices (ChoiceS &&centries);
  const
  ChoiceS&   get_choices () const;
  void       copy_fields (const ParamInfo &src);
  /*ctor*/   ParamInfo   (ParamId pid = ParamId (0), uint porder = 0);
  virtual   ~ParamInfo   ();
  const uint order;
private:
  uint       union_tag = 0;
  union {
    struct { double fmin, fmax, fstep; };
    uint64_t mem[sizeof (ChoiceS) / sizeof (uint64_t)];
    ChoiceS* centries() const { return (ChoiceS*) mem; }
  } u;
  double     initial_ = 0;
  /*copy*/   ParamInfo   (const ParamInfo&) = delete;
  void       release     ();
  std::weak_ptr<Property> aprop_;
  friend class AudioProcessor;
};

/// Structure providing supplementary information about input/output buses.
struct BusInfo {
  CString            ident;     ///< Identifier used for serialization.
  CString            label;     ///< Preferred user interface name.
  CString            hints;     ///< Hints for parameter handling.
  CString            blurb;     ///< Short description for user interface tooltips.
  SpeakerArrangement speakers = SpeakerArrangement::NONE; ///< Channel to speaker arrangement.
  uint               n_channels () const;
};

/// Audio signal AudioProcessor base class, implemented by all effects and instruments.
class AudioProcessor : public std::enable_shared_from_this<AudioProcessor>, public FastMemory::NewDeleteBase {
  struct IBus;
  struct OBus;
  struct EventStreams;
  union  PBus;
  struct PParam;
  class FloatBuffer;
  friend class ProcessorManager;
  friend class DeviceImpl;
  friend class NativeDeviceImpl;
  friend class AudioEngineThread;
  struct OConnection {
    AudioProcessor *proc = nullptr; IBusId ibusid = {};
    bool operator== (const OConnection &o) const { return proc == o.proc && ibusid == o.ibusid; }
  };
  using OBRange = std::pair<FloatBuffer*,FloatBuffer*>;
  AudioProcessor          *sched_next_ = nullptr;
protected:
#ifndef DOXYGEN
  // Inherit `AudioSignal` concepts in derived classes from other namespaces
  using MinMax = std::pair<double,double>;
#endif
  enum { INITIALIZED   = 1 << 0,
         //            = 1 << 1,
         SCHEDULED     = 1 << 2,
         PARAMCHANGE   = 1 << 3,
         BUSCONNECT    = 1 << 4,
         BUSDISCONNECT = 1 << 5,
         INSERTION     = 1 << 6,
         REMOVAL       = 1 << 7,
         ENGINE_OUTPUT = 1 << 8,
  };
  std::atomic<uint32>      flags_ = 0;
private:
  uint32                   output_offset_ = 0;
  FloatBuffer             *fbuffers_ = nullptr;
  std::vector<PBus>        iobuses_;
  std::vector<PParam>      params_; // const once is_initialized()
  std::vector<OConnection> outputs_;
  EventStreams            *estreams_ = nullptr;
  AtomicBits              *atomic_bits_ = nullptr;
  uint64_t                 render_stamp_ = 0;
  const PParam*      find_pparam        (Id32 paramid) const;
  const PParam*      find_pparam_       (ParamId paramid) const;
  void               assign_iobufs      ();
  void               release_iobufs     ();
  void               ensure_initialized (DeviceP devicep);
  const FloatBuffer& float_buffer       (IBusId busid, uint channelindex) const;
  FloatBuffer&       float_buffer       (OBusId busid, uint channelindex, bool resetptr = false);
  static
  const FloatBuffer& zero_buffer        ();
  void               render_block       (uint64 target_stamp);
  void               reset_state        (uint64 target_stamp);
  /*copy*/           AudioProcessor     (const AudioProcessor&) = delete;
  virtual void       render             (uint n_frames) = 0;
  virtual void       reset              (uint64 target_stamp) = 0;
  PropertyP          access_property    (ParamId id) const;
protected:
  AudioEngine  &engine_;
  explicit      AudioProcessor    (AudioEngine &engine);
  virtual      ~AudioProcessor    ();
  virtual void  initialize        (SpeakerArrangement busses) = 0;
  void          enotify_enqueue_mt (uint32 pushmask);
  uint          schedule_processor ();
  void          reschedule        ();
  virtual uint  schedule_children () { return 0; }
  static uint   schedule_processor (AudioProcessor &p)  { return p.schedule_processor(); }
  // Parameters
  virtual void  adjust_param      (Id32 tag) {}
  ParamId       nextid            () const;
  ParamId       add_param         (Id32 id, const ParamInfo &infotmpl, double value);
  ParamId       add_param         (Id32 id, const String &clabel, const String &nickname,
                                   double pmin, double pmax, double value,
                                   const String &unit = "", String hints = "",
                                   const String &blurb = "", const String &description = "");
  ParamId       add_param         (Id32 id, const String &clabel, const String &nickname,
                                   ChoiceS &&centries, double value, String hints = "",
                                   const String &blurb = "", const String &description = "");
  ParamId       add_param         (Id32 id, const String &clabel, const String &nickname,
                                   bool boolvalue, String hints = "",
                                   const String &blurb = "", const String &description = "");
  void          start_group       (const String &groupname) const;
  ParamId       add_param         (const String &clabel, const String &nickname,
                                   double pmin, double pmax, double value,
                                   const String &unit = "", String hints = "",
                                   const String &blurb = "", const String &description = "");
  ParamId       add_param         (const String &clabel, const String &nickname,
                                   ChoiceS &&centries, double value, String hints = "",
                                   const String &blurb = "", const String &description = "");
  ParamId       add_param         (const String &clabel, const String &nickname,
                                   bool boolvalue, String hints = "",
                                   const String &blurb = "", const String &description = "");
  double        peek_param_mt     (Id32 paramid) const;
  // Buses
  IBusId        add_input_bus     (CString uilabel, SpeakerArrangement speakerarrangement,
                                   const String &hints = "", const String &blurb = "");
  OBusId        add_output_bus    (CString uilabel, SpeakerArrangement speakerarrangement,
                                   const String &hints = "", const String &blurb = "");
  void          remove_all_buses  ();
  OBus&         iobus             (OBusId busid);
  IBus&         iobus             (IBusId busid);
  const OBus&   iobus             (OBusId busid) const { return const_cast<AudioProcessor*> (this)->iobus (busid); }
  const IBus&   iobus             (IBusId busid) const { return const_cast<AudioProcessor*> (this)->iobus (busid); }
  void          disconnect_ibuses ();
  void          disconnect_obuses ();
  void          disconnect        (IBusId ibus);
  void          connect           (IBusId ibus, AudioProcessor &oproc, OBusId obus);
  float*        oblock            (OBusId b, uint c);
  void          assign_oblock     (OBusId b, uint c, float val);
  void          redirect_oblock   (OBusId b, uint c, const float *block);
  // event stream handling
  void             prepare_event_input  ();
  MidiEventRange   get_event_input      ();
  void             prepare_event_output ();
  MidiEventStream& get_event_output     ();
  // Atomic notification bits
  void             atomic_bits_resize (size_t count);
  bool             atomic_bit_notify  (size_t nth);
public:
  AtomicBits::Iter atomic_bits_iter   (size_t pos = 0) const;
  using MakeProcessor = AudioProcessorP (*) (AudioEngine&);
  using MaybeParamId = std::pair<ParamId,bool>;
  static const String GUIONLY;     ///< ":G:r:w:" - GUI READABLE WRITABLE
  static const String STANDARD;    ///< ":G:S:r:w:" - GUI STORAGE READABLE WRITABLE
  static const String STORAGEONLY; ///< ":S:r:w:" - STORAGE READABLE WRITABLE
  float         note_to_freq      (int note) const;
  String        debug_name        () const;
  AudioEngine&          engine      () const ASE_CONST;
  const AudioTransport& transport   () const ASE_CONST;
  uint                  sample_rate () const ASE_CONST;
  double                nyquist     () const ASE_CONST;
  double                inyquist    () const ASE_CONST;
  // Parameters
  double              get_param             (Id32 paramid);
  bool                set_param             (Id32 paramid, double value, bool sendnotify = true);
  ParamInfoP          param_info            (Id32 paramid) const;
  MaybeParamId        find_param            (const String &identifier) const;
  MinMax              param_range           (Id32 paramid) const;
  bool                check_dirty           (Id32 paramid) const;
  void                adjust_params         (bool include_nondirty = false);
  virtual String      param_value_to_text   (Id32 paramid, double value) const;
  virtual double      param_value_from_text (Id32 paramid, const String &text) const;
  virtual double      value_to_normalized   (Id32 paramid, double value) const;
  virtual double      value_from_normalized (Id32 paramid, double normalized) const;
  double              get_normalized        (Id32 paramid);
  bool                set_normalized        (Id32 paramid, double normalized, bool sendnotify = true);
  bool                is_initialized        () const;
  // Buses
  IBusId        find_ibus         (const String &name) const;
  OBusId        find_obus         (const String &name) const;
  uint          n_ibuses          () const;
  uint          n_obuses          () const;
  uint          n_ichannels       (IBusId busid) const;
  uint          n_ochannels       (OBusId busid) const;
  BusInfo       bus_info          (IBusId busid) const;
  BusInfo       bus_info          (OBusId busid) const;
  bool          connected         (OBusId obusid) const;
  const float*  ifloats           (IBusId b, uint c) const;
  const float*  ofloats           (OBusId b, uint c) const;
  static uint64 timestamp         ();
  DeviceP       get_device        () const;
  bool          has_event_input   () const;
  bool          has_event_output  () const;
  void          connect_event_input    (AudioProcessor &oproc);
  void          disconnect_event_input ();
  void          enable_engine_output   (bool onoff);
  // MT-Safe accessors
  static double          param_peek_mt   (const AudioProcessorP proc, Id32 paramid);
  // AudioProcessor Registry
  using StaticInfo = void (*) (AudioProcessorInfo&);
  using MakeDeviceP = std::function<DeviceP (const String&, StaticInfo, AudioProcessorP)>;
  using MakeProcessorP = AudioProcessorP (*) (AudioEngine&);
  static void    registry_add     (CString aseid, StaticInfo, MakeProcessorP);
  static DeviceP registry_create  (const String &aseid, AudioEngine &engine, const MakeDeviceP&);
  static void    registry_foreach (const std::function<void (const String &aseid, StaticInfo)> &fun);
  template<class AudioProc, class ...Args> std::shared_ptr<AudioProc>
  static         create_processor (AudioEngine &engine, const Args &...args);
private:
  static bool   enotify_pending   ();
  static void   enotify_dispatch  ();
  std::atomic<AudioProcessor*> nqueue_next_ { nullptr }; ///< No notifications queued while == nullptr
  AudioProcessorP              nqueue_guard_;            ///< Only used while nqueue_next_ != nullptr
  std::weak_ptr<Device>        device_;
  static constexpr uint32 NOTIFYMASK = PARAMCHANGE | BUSCONNECT | BUSDISCONNECT | INSERTION | REMOVAL;
  static __thread uint64  tls_timestamp;
};

/// Aggregate structure for input/output buffer state and values in AudioProcessor::render().
/// The floating point #buffer array is cache-line aligned (to 64 byte) to optimize
/// SIMD access and avoid false sharing.
class AudioProcessor::FloatBuffer {
  void          check      ();
  /// Floating point memory when #buffer is not redirected, 64-byte aligned.
  alignas (64) float fblock[AUDIO_BLOCK_MAX_RENDER_SIZE] = { 0, };
  const uint64       canary0_ = const_canary;
  const uint64       canary1_ = const_canary;
  struct { uint64 d1, d2, d3, d4; }; // dummy mem
  SpeakerArrangement speaker_arrangement_ = SpeakerArrangement::NONE;
  SpeakerArrangement speaker_arrangement () const;
  static constexpr uint64 const_canary = 0xE14D8A302B97C56F;
  friend class AudioProcessor;
  /// Pointer to the IO samples, this can be redirected or point to #fblock.
  float             *buffer = &fblock[0];
};

// == ProcessorManager ==
/// Interface for management, connecting and processing of AudioProcessor instances.
class ProcessorManager {
protected:
  static auto pm_remove_all_buses    (AudioProcessor &p)       { return p.remove_all_buses(); }
  static auto pm_disconnect_ibuses   (AudioProcessor &p)       { return p.disconnect_ibuses(); }
  static auto pm_disconnect_obuses   (AudioProcessor &p)       { return p.disconnect_obuses(); }
  static auto pm_connect             (AudioProcessor &p, IBusId i, AudioProcessor &d, OBusId o)
                                     { return p.connect (i, d, o); }
  static auto pm_connect_event_input (AudioProcessor &oproc, AudioProcessor &iproc)
                                     { return iproc.connect_event_input (oproc); }
};

// == Inlined Internals ==
struct AudioProcessor::IBus : BusInfo {
  AudioProcessor *proc = {};
  OBusId     obusid = {};
  explicit IBus (const String &ident, const String &label, SpeakerArrangement sa);
};
struct AudioProcessor::OBus : BusInfo {
  uint fbuffer_concounter = 0;
  uint fbuffer_count = 0;
  uint fbuffer_index = ~0;
  explicit OBus (const String &ident, const String &label, SpeakerArrangement sa);
};
// AudioProcessor internal input/output bus book keeping
union AudioProcessor::PBus {
  IBus    ibus;
  OBus    obus;
  BusInfo pbus;
  explicit PBus (const String &ident, const String &label, SpeakerArrangement sa);
};

// AudioProcessor internal input/output event stream book keeping
struct AudioProcessor::EventStreams {
  static constexpr auto EVENT_ISTREAM = IBusId (0xff01); // *not* an input bus, ID is used for OConnection
  AudioProcessor      *oproc = nullptr;
  MidiEventStream estream;
  bool            has_event_input = false;
  bool            has_event_output = false;
};

// AudioProcessor internal parameter book keeping
struct AudioProcessor::PParam {
  explicit PParam          (ParamId id);
  explicit PParam          (ParamId id, uint order, const ParamInfo &pinfo);
  /*copy*/ PParam          (const PParam &);
  PParam& operator=        (const PParam &);
  double   fetch_and_clean ()       { dirty (false); return value_; }
  double   peek            () const { return value_; }
  bool     dirty           () const { return flags_ & DIRTY; }
  void     dirty           (bool b) { if (b) flags_ |= DIRTY; else flags_ &= ~uint32 (DIRTY); }
  bool     changed         () const { return flags_ & CHANGED; }
  bool     changed         (bool b) { return CHANGED & (b ? flags_.fetch_or (CHANGED) :
                                                        flags_.fetch_and (~uint32 (CHANGED))); }
  static int // Helper to keep PParam structures sorted.
  cmp (const PParam &a, const PParam &b)
  {
    return a.id < b.id ? -1 : a.id > b.id;
  }
public:
  ParamId             id = {};  ///< Tag to identify parameter in APIs.
private:
  enum { DIRTY = 1, CHANGED = 2, };
  std::atomic<uint32> flags_ = 1;
  std::atomic<double> value_ = NAN;
public:
  ParamInfoP          info;
  bool
  assign (double f)
  {
    const double old = value_;
    value_ = f;
    if (ASE_ISLIKELY (old != value_))
      {
        const uint32 prev = flags_.fetch_or (DIRTY | CHANGED);
        if (!ASE_ISLIKELY (prev & CHANGED))
          return true; // need notify
      }
    return false; // no notify needed
  }
};

/// Number of channels described by `speakers`.
inline uint
BusInfo::n_channels () const
{
  return speaker_arrangement_count_channels (speakers);
}

/// Retrieve AudioEngine handle for this AudioProcessor.
inline AudioEngine&
AudioProcessor::engine () const
{
  return engine_;
}

/// Sample rate mixing frequency in Hz as unsigned, used for render().
inline const AudioTransport&
AudioProcessor::transport () const
{
  return engine_.transport();
}

inline uint
AudioProcessor::sample_rate () const
{
  return engine_.sample_rate();
}

/// Half the sample rate in Hz as double, used for render().
inline double
AudioProcessor::nyquist () const
{
  return engine_.nyquist();
}

/// Inverse Nyquist frequency, i.e. 1.0 / nyquist().
inline double
AudioProcessor::inyquist () const
{
  return engine_.inyquist();
}

/// Returns `true` if this AudioProcessor has an event input stream.
inline bool
AudioProcessor::has_event_input () const
{
  return estreams_ && estreams_->has_event_input;
}

/// Returns `true` if this AudioProcessor has an event output stream.
inline bool
AudioProcessor::has_event_output () const
{
  return estreams_ && estreams_->has_event_output;
}

/// Number of input buses configured for this AudioProcessor.
inline uint
AudioProcessor::n_ibuses () const
{
  return output_offset_;
}

/// Number of output buses configured for this AudioProcessor.
inline uint
AudioProcessor::n_obuses () const
{
  return iobuses_.size() - output_offset_;
}

/// Retrieve BusInfo for an input bus.
inline BusInfo
AudioProcessor::bus_info (IBusId busid) const
{
  return iobus (busid);
}

/// Retrieve BusInfo for an output bus.
inline BusInfo
AudioProcessor::bus_info (OBusId busid) const
{
  return iobus (busid);
}

/// Number of channels of input bus `busid` configured for this AudioProcessor.
inline uint
AudioProcessor::n_ichannels (IBusId busid) const
{
  const IBus &ibus = iobus (busid);
  return ibus.n_channels();
}

/// Number of channels of output bus `busid` configured for this AudioProcessor.
inline uint
AudioProcessor::n_ochannels (OBusId busid) const
{
  const OBus &obus = iobus (busid);
  return obus.n_channels();
}

// Call adjust_param() for all or just dirty parameters.
inline void
AudioProcessor::adjust_params (bool include_nondirty)
{
  for (const PParam &p : params_)
    if (include_nondirty || p.dirty())
      adjust_param (p.id);
}

// Find parameter for internal access.
inline const AudioProcessor::PParam*
AudioProcessor::find_pparam (Id32 paramid) const
{
  // fast path via sequential ids
  const size_t idx = paramid.id - 1;
  if (ASE_ISLIKELY (idx < params_.size()) && ASE_ISLIKELY (params_[idx].id == ParamId (paramid.id)))
    return &params_[idx];
  return find_pparam_ (ParamId (paramid.id));
}

/// Fetch `value` of parameter `id` and clear its `dirty` flag.
inline double
AudioProcessor::get_param (Id32 paramid)
{
  const PParam *pparam = find_pparam (ParamId (paramid.id));
  return ASE_ISLIKELY (pparam) ? const_cast<PParam*> (pparam)->fetch_and_clean() : FP_NAN;
}

/// Check if the parameter `dirty` flag is set.
/// Return `true` if set_param() changed the parameter value since the last get_param() call.
inline bool
AudioProcessor::check_dirty (Id32 paramid) const
{
  const PParam *param = this->find_pparam (ParamId (paramid.id));
  return ASE_ISLIKELY (param) ? param->dirty() : false;
}

/// Access readonly float buffer of input bus `b`, channel `c`, see also ofloats().
inline const float*
AudioProcessor::ifloats (IBusId b, uint c) const
{
  return float_buffer (b, c).buffer;
}

/// Access readonly float buffer of output bus `b`, channel `c`, see also oblock().
inline const float*
AudioProcessor::ofloats (OBusId b, uint c) const
{
  return const_cast<AudioProcessor*> (this)->float_buffer (b, c).buffer;
}

/// Reset buffer redirections and access float buffer of output bus `b`, channel `c`.
/// See also ofloats() for readonly access and redirect_oblock() for redirections.
inline float*
AudioProcessor::oblock (OBusId b, uint c)
{
  return float_buffer (b, c, true).buffer;
}

/// The current timestamp in sample frames
inline uint64
AudioProcessor::timestamp ()
{
  return tls_timestamp;
}

/// Retrieve the speaker assignment.
inline SpeakerArrangement
AudioProcessor::FloatBuffer::speaker_arrangement () const
{
  return speaker_arrangement_;
}

/// Add an AudioProcessor derived type to the audio processor registry.
template<typename T> extern inline CString
register_audio_processor (const char *caseid)
{
  CString aseid = caseid ? caseid : typeid_name<T>();
  if constexpr (std::is_constructible<T, AudioEngine&>::value)
    {
      auto make_shared = [] (AudioEngine &engine) -> AudioProcessorP {
        return std::make_shared<T> (engine);
      };
      AudioProcessor::registry_add (aseid, &T::static_info, make_shared);
    }
  else
    static_assert (sizeof (T) < 0, "type `T` must be constructible from `T(AudioEngine&)`");
  return aseid;
}

template<class AudioProc, class ...Args> std::shared_ptr<AudioProc>
AudioProcessor::create_processor (AudioEngine &engine, const Args &...args)
{
  std::shared_ptr<AudioProc> proc = std::make_shared<AudioProc> (engine, args...);
  if (proc) {
    AudioProcessorP aproc = proc;
    aproc->ensure_initialized (nullptr);
  }
  return proc;
}

} // Ase

namespace std {
template<>
struct hash<::Ase::ParamInfo> {
  /// Hash value for Ase::ParamInfo.
  size_t
  operator() (const ::Ase::ParamInfo &pi) const
  {
    size_t h = ::std::hash<::Ase::CString>() (pi.ident);
    // h ^= ::std::hash (pi.label);
    // h ^= ::std::hash (pi.nick);
    // h ^= ::std::hash (pi.description);
    h ^= ::std::hash<::Ase::CString>() (pi.unit);
    h ^= ::std::hash<::Ase::CString>() (pi.hints);
    // min, max, step
    return h;
  }
};
} // std

#endif // __ASE_PROCESSOR_HH__
