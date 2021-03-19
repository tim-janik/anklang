// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PROCESSOR_HH__
#define __ASE_PROCESSOR_HH__

#include <ase/gadget.hh>
#include <ase/midievent.hh>
#include <ase/datautils.hh>
#include <ase/properties.hh>
#include <ase/engine.hh>
#include <atomic>
#include <any>

namespace Ase {

ASE_CLASS_DECLS (AudioCombo);
ASE_CLASS_DECLS (AudioProcessor);
ASE_CLASS_DECLS (DeviceImpl);
ASE_STRUCT_DECLS (ParamInfo);

/// ID type for AudioProcessor parameters, the ID numbers are user assignable.
enum class ParamId : uint32 {};

/// ID type for AudioProcessor input buses, buses are numbered with increasing index.
enum class IBusId : uint16 {};

/// ID type for AudioProcessor output buses, buses are numbered with increasing index.
enum class OBusId : uint16 {};

/// ID type for the AudioProcessor registry.
struct RegistryId { struct Entry; const Entry &entry; };

/// Add an AudioProcessor derived type to the audio processor registry.
template<typename T> RegistryId register_audio_processor (const char *bfile = __builtin_FILE(), int bline = __builtin_LINE());

/// Flags to indicate channel arrangements of a bus.
/// See also: https://en.wikipedia.org/wiki/Surround_sound
enum class SpeakerArrangement : uint64_t {
  NONE                  = 0,
  FRONT_LEFT            =         0x1,  ///< Stereo Left (FL)
  FRONT_RIGHT           =         0x2,  ///< Stereo Right (FR)
  FRONT_CENTER          =         0x4,  ///< (FC)
  LOW_FREQUENCY         =         0x8,  ///< Low Frequency Effects (LFE)
  BACK_LEFT             =        0x10,  ///< (BL)
  BACK_RIGHT            =        0x20,  ///< (BR)
  // WAV reserved       =  0xyyy00000,
  AUX                   = uint64_t (1) << 63,   ///< Flag for side chain uses
  MONO                  = FRONT_LEFT,   ///< Single Channel (M)
  STEREO                = FRONT_LEFT | FRONT_RIGHT,
  STEREO_21             = STEREO | LOW_FREQUENCY,
  STEREO_30             = STEREO | FRONT_CENTER,
  STEREO_31             = STEREO_30 | LOW_FREQUENCY,
  SURROUND_50           = STEREO_30 | BACK_LEFT | BACK_RIGHT,
  SURROUND_51           = SURROUND_50 | LOW_FREQUENCY,
#if 0 // TODO: dynamic multichannel support
  FRONT_LEFT_OF_CENTER  =        0x40,  ///< (FLC)
  FRONT_RIGHT_OF_CENTER =        0x80,  ///< (FRC)
  BACK_CENTER           =       0x100,  ///< (BC)
  SIDE_LEFT             =       0x200,  ///< (SL)
  SIDE_RIGHT            =       0x400,  ///< (SR)
  TOP_CENTER            =       0x800,  ///< (TC)
  TOP_FRONT_LEFT        =      0x1000,  ///< (TFL)
  TOP_FRONT_CENTER      =      0x2000,  ///< (TFC)
  TOP_FRONT_RIGHT       =      0x4000,  ///< (TFR)
  TOP_BACK_LEFT         =      0x8000,  ///< (TBL)
  TOP_BACK_CENTER       =     0x10000,  ///< (TBC)
  TOP_BACK_RIGHT        =     0x20000,  ///< (TBR)
  SIDE_SURROUND_50      = STEREO_30 | SIDE_LEFT | SIDE_RIGHT,
  SIDE_SURROUND_51      = SIDE_SURROUND_50 | LOW_FREQUENCY,
#endif
};
constexpr SpeakerArrangement speaker_arrangement_channels_mask { ~size_t (SpeakerArrangement::AUX) };
uint8              speaker_arrangement_count_channels (SpeakerArrangement spa);
SpeakerArrangement speaker_arrangement_channels       (SpeakerArrangement spa);
bool               speaker_arrangement_is_aux         (SpeakerArrangement spa);
const char*        speaker_arrangement_bit_name       (SpeakerArrangement spa);
std::string        speaker_arrangement_desc           (SpeakerArrangement spa);

/// Detailed information and common properties of AudioProcessor subclasses.
struct AudioProcessorInfo {
  CString uri;          ///< Unique identifier for de-/serialization.
  CString version;      ///< Version identifier for de-/serialization.
  CString label;        ///< Preferred user interface name.
  CString category;     ///< Category to allow grouping for processors of similar function.
  CString blurb;        ///< Short description for overviews.
  CString description;  ///< Elaborate description for help dialogs.
  CString website_url;  ///< Website of/about this AudioProcessor.
  CString creator_name; ///< Name of the creator.
  CString creator_url;  ///< Internet contact of the creator.
};

/// An in-memory icon representation.
struct IconStr : std::string {
};

/// One possible choice for selection parameters.
struct ChoiceDetails {
  const CString ident;          ///< Identifier used for serialization (can be derived from label).
  const CString label;          ///< Preferred user interface name.
  const CString subject;        ///< Subject line, a brief one liner or elaborate title.
  const IconStr icon;           ///< Stringified icon, SVG and PNG should be supported (64x64 pixels recommended).
  bool    operator== (const ChoiceDetails &o) const     { return ident == o.ident; }
  bool    operator!= (const ChoiceDetails &o) const     { return !operator== (o); }
  ChoiceDetails (CString label, CString subject = "");
  ChoiceDetails (IconStr icon, CString label, CString subject = "");
};

/// List of choices for ParamInfo.set_choices().
struct ChoiceEntries : std::vector<ChoiceDetails> {
  ChoiceEntries& operator+= (const ChoiceDetails &ce);
  using base_t = std::vector<ChoiceDetails>;
  ChoiceEntries (std::initializer_list<base_t::value_type> __l) : base_t (__l) {}
  ChoiceEntries () {}
};

/// Detailed information and common properties of parameters.
struct ParamInfo {
  CString    ident;        ///< Identifier used for serialization.
  CString    label;        ///< Preferred user interface name.
  CString    nick;         ///< Abbreviated user interface name, usually not more than 6 characters.
  CString    unit;         ///< Units of the values within range.
  CString    hints;        ///< Hints for parameter handling.
  GroupId    group;        ///< Group for parameters of similar function.
  CString    blurb;        ///< Short description for user interface tooltips.
  CString    description;  ///< Elaborate description for help dialogs.
  using MinMax = std::pair<double,double>;
  void       clear       ();
  MinMax     get_minmax  () const;
  double     get_stepping() const;
  double     get_initial () const;
  void       get_range   (double &fmin, double &fmax, double &fstep) const;
  void       set_range   (double fmin, double fmax, double fstep = 0);
  void       set_choices (const ChoiceEntries &centries);
  void       set_choices (ChoiceEntries &&centries);
  ChoiceEntries
  const&     get_choices () const;
  void       copy_fields (const ParamInfo &src);
  /*ctor*/   ParamInfo   (ParamId pid = ParamId (0), uint porder = 0);
  virtual   ~ParamInfo   ();
  const uint order;
private:
  uint       union_tag = 0;
  union {
    struct { double fmin, fmax, fstep; };
    uint64_t mem[sizeof (ChoiceEntries) / sizeof (uint64_t)];
    ChoiceEntries* centries() const { return (ChoiceEntries*) mem; }
  } u;
  double     initial_ = 0;
  /*copy*/   ParamInfo   (const ParamInfo&) = delete;
  void       release     ();
  std::weak_ptr<Property> bprop_;
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
  friend class AudioEngine;
  struct OConnection {
    AudioProcessor *proc = nullptr; IBusId ibusid = {};
    bool operator== (const OConnection &o) const { return proc == o.proc && ibusid == o.ibusid; }
  };
  using OBRange = std::pair<FloatBuffer*,FloatBuffer*>;
protected:
#ifndef DOXYGEN
  // Inherit `AudioSignal` concepts in derived classes from other namespaces
  using MinMax = std::pair<double,double>;
#endif
  enum { INITIALIZED   = 1 << 0,
         PARAMCHANGE   = 1 << 3,
         BUSCONNECT    = 1 << 4,
         BUSDISCONNECT = 1 << 5,
         INSERTION     = 1 << 6,
         REMOVAL       = 1 << 7, };
  std::atomic<uint32>      flags_ = 0;
private:
  uint32                   output_offset_ = 0;
  FloatBuffer             *fbuffers_ = nullptr;
  std::vector<PBus>        iobuses_;
  std::vector<PParam>      params_;
  std::vector<OConnection> outputs_;
  EventStreams            *estreams_ = nullptr;
  uint64_t                 done_frames_ = 0;
  static void        registry_init      ();
  const PParam*      find_pparam        (Id32 paramid) const;
  const PParam*      find_pparam_       (ParamId paramid) const;
  void               assign_iobufs      ();
  void               release_iobufs     ();
  void               reconfigure        (IBusId ibus, SpeakerArrangement ipatch, OBusId obus, SpeakerArrangement opatch);
  void               ensure_initialized ();
  const FloatBuffer& float_buffer       (IBusId busid, uint channelindex) const;
  FloatBuffer&       float_buffer       (OBusId busid, uint channelindex, bool resetptr = false);
  static
  const FloatBuffer& zero_buffer        ();
  void               render_block       ();
  void               reset_state        ();
  void               enqueue_deps       ();
  /*copy*/           AudioProcessor     (const AudioProcessor&) = delete;
  virtual void       render             (uint n_frames) = 0;
  virtual void       reset              () = 0;
  PropertyP          access_property    (ParamId id) const;
protected:
  AudioEngine  &engine_;
  explicit      AudioProcessor    ();
  virtual      ~AudioProcessor    ();
  virtual void  initialize        ();
  virtual void  configure         (uint n_ibuses, const SpeakerArrangement *ibuses,
                                   uint n_obuses, const SpeakerArrangement *obuses) = 0;
  void          enqueue_notify_mt (uint32 pushmask);
  virtual DeviceImplP device_impl () const;
  // Parameters
  virtual void  adjust_param      (Id32 tag) {}
  virtual void  enqueue_children  () {}
  ParamId       nextid            () const;
  ParamId       add_param         (Id32 id, const ParamInfo &infotmpl, double value);
  ParamId       add_param         (Id32 id, const std::string &clabel, const std::string &nickname,
                                   double pmin, double pmax, double value,
                                   const std::string &unit = "", std::string hints = "",
                                   const std::string &blurb = "", const std::string &description = "");
  ParamId       add_param         (Id32 id, const std::string &clabel, const std::string &nickname,
                                   ChoiceEntries &&centries, double value, std::string hints = "",
                                   const std::string &blurb = "", const std::string &description = "");
  ParamId       add_param         (Id32 id, const std::string &clabel, const std::string &nickname,
                                   bool boolvalue, std::string hints = "",
                                   const std::string &blurb = "", const std::string &description = "");
  void          start_group       (const std::string &groupname) const;
  ParamId       add_param         (const std::string &clabel, const std::string &nickname,
                                   double pmin, double pmax, double value,
                                   const std::string &unit = "", std::string hints = "",
                                   const std::string &blurb = "", const std::string &description = "");
  ParamId       add_param         (const std::string &clabel, const std::string &nickname,
                                   ChoiceEntries &&centries, double value, std::string hints = "",
                                   const std::string &blurb = "", const std::string &description = "");
  ParamId       add_param         (const std::string &clabel, const std::string &nickname,
                                   bool boolvalue, std::string hints = "",
                                   const std::string &blurb = "", const std::string &description = "");
  double        peek_param_mt     (Id32 paramid) const;
  // Buses
  IBusId        add_input_bus     (CString uilabel, SpeakerArrangement speakerarrangement,
                                   const std::string &hints = "", const std::string &blurb = "");
  OBusId        add_output_bus    (CString uilabel, SpeakerArrangement speakerarrangement,
                                   const std::string &hints = "", const std::string &blurb = "");
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
public:
  using RegistryList = std::vector<AudioProcessorInfo>;
  using MakeProcessor = AudioProcessorP (*) (const std::any*);
  using ParamInfoPVec = std::vector<ParamInfoP>;
  using MaybeParamId = std::pair<ParamId,bool>;
  static const std::string STANDARD; ///< ":G:S:r:w:" - GUI STORAGE READABLE WRITABLE
  AudioEngine&  engine            () const;
  uint          sample_rate       () const ASE_CONST;
  double        nyquist           () const ASE_CONST;
  double        inyquist          () const ASE_CONST;
  virtual void  query_info        (AudioProcessorInfo &info) const = 0;
  String        debug_name        () const;
  // Parameters
  double              get_param             (Id32 paramid);
  void                set_param             (Id32 paramid, double value);
  ParamInfoP          param_info            (Id32 paramid) const;
  MaybeParamId        find_param            (const String &identifier) const;
  MinMax              param_range           (Id32 paramid) const;
  bool                check_dirty           (Id32 paramid) const;
  void                adjust_params         (bool include_nondirty = false);
  virtual std::string param_value_to_text   (Id32 paramid, double value) const;
  virtual double      param_value_from_text (Id32 paramid, const std::string &text) const;
  virtual double      value_to_normalized   (Id32 paramid, double value) const;
  virtual double      value_from_normalized (Id32 paramid, double normalized) const;
  double              get_normalized        (Id32 paramid);
  void                set_normalized        (Id32 paramid, double normalized);
  bool                is_initialized    () const;
  // Buses
  IBusId        find_ibus         (const std::string &name) const;
  OBusId        find_obus         (const std::string &name) const;
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
  bool          has_event_input   ();
  bool          has_event_output  ();
  void          connect_event_input    (AudioProcessor &oproc);
  void          disconnect_event_input ();
  DeviceImplP   access_processor () const;
  // MT-Safe accessors
  static double          param_peek_mt   (const AudioProcessorP proc, Id32 paramid);
  // Registration and factory
  static RegistryList    registry_list   ();
  static AudioProcessorP registry_create (AudioEngine &engine, const String &uuiduri);
  static AudioProcessorP registry_create (AudioEngine &engine, RegistryId rid, const std::any &any);
private:
  static RegistryId      registry_enroll (MakeProcessor, const char*, int);
  template<class> friend RegistryId register_audio_processor (const char*, int);
  static bool   has_notifies_e    ();
  static void   call_notifies_e   ();
  std::atomic<AudioProcessor*> nqueue_next_ { nullptr }; ///< No notifications queued while == nullptr
  AudioProcessorP              nqueue_guard_;            ///< Only used while nqueue_next_ != nullptr
  std::weak_ptr<DeviceImpl> device_;
  static constexpr uint32 NOTIFYMASK = PARAMCHANGE | BUSCONNECT | BUSDISCONNECT | INSERTION | REMOVAL;
  static __thread uint64  tls_timestamp;
};

/// Timing information around AudioSignal processing.
struct AudioTiming {
  double bpm = 0;                       ///< Current tempo in beats per minute.
  uint64 frame_stamp = ~uint64 (0);     ///< Number of sample frames processed since playback start.
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
  static auto pm_remove_all_buses  (AudioProcessor &p)       { return p.remove_all_buses(); }
  static auto pm_disconnect_ibuses (AudioProcessor &p)       { return p.disconnect_ibuses(); }
  static auto pm_disconnect_obuses (AudioProcessor &p)       { return p.disconnect_obuses(); }
  static auto pm_connect           (AudioProcessor &p, IBusId i, AudioProcessor &d, OBusId o)
                                   { return p.connect (i, d, o); }
  static auto pm_connect_events    (AudioProcessor &oproc, AudioProcessor &iproc)
                                   { return iproc.connect_event_input (oproc); }
  static auto pm_reconfigure       (AudioProcessor &p, IBusId i, SpeakerArrangement ip, OBusId o, SpeakerArrangement op)
                                   { return p.reconfigure (i, ip, o, op); }
};

// == Inlined Internals ==
struct AudioProcessor::IBus : BusInfo {
  AudioProcessor *proc = {};
  OBusId     obusid = {};
  explicit IBus (const std::string &ident, const std::string &label, SpeakerArrangement sa);
};
struct AudioProcessor::OBus : BusInfo {
  uint fbuffer_concounter = 0;
  uint fbuffer_count = 0;
  uint fbuffer_index = ~0;
  explicit OBus (const std::string &ident, const std::string &label, SpeakerArrangement sa);
};
// AudioProcessor internal input/output bus book keeping
union AudioProcessor::PBus {
  IBus    ibus;
  OBus    obus;
  BusInfo pbus;
  explicit PBus (const std::string &ident, const std::string &label, SpeakerArrangement sa);
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
AudioProcessor::has_event_input()
{
  return estreams_ && estreams_->has_event_input;
}

/// Returns `true` if this AudioProcessor has an event output stream.
inline bool
AudioProcessor::has_event_output()
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
template<typename T> extern inline RegistryId
register_audio_processor (const char *bfile, int bline)
{
  AudioProcessor::MakeProcessor makeasp = nullptr;
  if constexpr (std::is_constructible<T, const std::any&>::value)
    {
      makeasp = [] (const std::any *any) -> AudioProcessorP {
        return any ? std::make_shared<T> (*any) : nullptr;
      };
    }
  else if constexpr (std::is_constructible<T>::value)
    {
      makeasp = [] (const std::any*) -> AudioProcessorP {
        return std::make_shared<T>();
      };
    }
  else
    static_assert (sizeof (T) < 0, "type `T` must be constructible from void or any");
  return AudioProcessor::registry_enroll (makeasp, bfile, bline);
}

class DeviceImpl : public GadgetImpl, public virtual Device {
  AudioProcessorP proc_;
public:
  explicit        DeviceImpl             (AudioProcessor &proc);
  DeviceInfo      device_info            () override;
  StringS         list_properties        () override;
  PropertyP       access_property        (String ident) override;
  PropertyS       access_properties      () override;
  AudioProcessorP audio_signal_processor () const       { return proc_; }
};

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
