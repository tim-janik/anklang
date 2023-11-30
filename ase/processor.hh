// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PROCESSOR_HH__
#define __ASE_PROCESSOR_HH__

#include <ase/gadget.hh>
#include <ase/parameter.hh>
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

/// Structure providing supplementary information about input/output buses.
struct BusInfo {
  CString            ident;     ///< Identifier used for serialization.
  CString            label;     ///< Preferred user interface name.
  CString            hints;     ///< Hints for parameter handling.
  CString            blurb;     ///< Short description for user interface tooltips.
  SpeakerArrangement speakers = SpeakerArrangement::NONE; ///< Channel to speaker arrangement.
  uint               n_channels () const;
};

/// Audio parameter handling, internal to AudioProcessor.
struct AudioParams final {
  using Map = std::map<uint32_t,ParameterC>;
  const uint32_t          *ids = nullptr;
  double                  *values = nullptr;
  std::atomic<uint64_t>   *bits = nullptr;
  const ParameterC        *parameters = nullptr;
  std::weak_ptr<Property> *wprops = nullptr;
  uint32                   count = 0;
  bool                     changed = false;
  void          clear   ();
  void          install (const Map &params);
  ssize_t       index   (uint32_t id) const;
  double        value   (uint32_t id) const;
  double        value   (uint32_t id, double newval);
  bool          dirty   (uint32_t id) const     { return dirty_index (index (id)); }
  void          dirty   (uint32_t id, bool d)   { return dirty_index (index (id), d); }
  /*dtor*/     ~AudioParams();
  bool      dirty_index (uint32_t idx) const;
  void      dirty_index (uint32_t idx, bool d);
};

/// Audio signal AudioProcessor base class, implemented by all effects and instruments.
class AudioProcessor : public std::enable_shared_from_this<AudioProcessor>, public FastMemory::NewDeleteBase {
  struct IBus;
  struct OBus;
  struct EventStreams;
  struct RenderContext;
  union  PBus;
  class FloatBuffer;
  friend class ProcessorManager;
  friend class DeviceImpl;
  friend class NativeDeviceImpl;
  friend class LV2DeviceImpl; /* FIXME: is this the right way to do it */
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
  using MidiEventInput = MidiEventReader<2>;
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
  AudioParams              params_;
  std::vector<OConnection> outputs_;
  EventStreams            *estreams_ = nullptr;
  AtomicBits              *atomic_bits_ = nullptr;
  uint64_t                 render_stamp_ = 0;
  using MidiEventVector = std::vector<MidiEvent>;
  using MidiEventVectorAP = std::atomic<MidiEventVector*>;
  MidiEventVectorAP        t0events_ = nullptr;
  RenderContext           *render_context_ = nullptr;
  template<class F> void modify_t0events (const F&);
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
  PropertyS          access_properties  () const;
public:
  struct ProcessorSetup {
    CString      aseid;
    AudioEngine &engine;
  };
protected:
  AudioEngine  &engine_;
  CString       aseid_;
  explicit      AudioProcessor    (const ProcessorSetup&);
  virtual      ~AudioProcessor    ();
  virtual void  initialize        (SpeakerArrangement busses) = 0;
  void          enotify_enqueue_mt (uint32 pushmask);
  uint          schedule_processor ();
  void          reschedule        ();
  virtual uint  schedule_children () { return 0; }
  static uint   schedule_processor (AudioProcessor &p)  { return p.schedule_processor(); }
  // Parameters
  void          install_params    (const AudioParams::Map &params);
  void          apply_event       (const MidiEvent &event);
  void          apply_input_events ();
  virtual void  adjust_param      (uint32_t paramid) {}
  void          set_param_from_render (Id32 paramid, const double value);
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
  void             prepare_event_output ();
  MidiEventInput   midi_event_input     ();
  MidiEventOutput& midi_event_output    ();
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
  bool                send_param            (Id32 paramid, double value);
  ParameterC          parameter             (Id32 paramid) const;
  MaybeParamId        find_param            (const String &identifier) const;
  MinMax              param_range           (Id32 paramid) const;
  bool                check_dirty           (Id32 paramid) const;
  void                adjust_all_params     ();
  virtual String      param_value_to_text   (uint32_t paramid, double value) const;
  virtual double      param_value_from_text (uint32_t paramid, const String &text) const;
  virtual double      value_to_normalized   (Id32 paramid, double value) const;
  virtual double      value_from_normalized (Id32 paramid, double normalized) const;
  double              get_normalized        (Id32 paramid);
  bool                set_normalized        (Id32 paramid, double normalized);
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
  using MakeProcessorP = AudioProcessorP (*) (CString,AudioEngine&);
  static void    registry_add     (CString aseid, StaticInfo, MakeProcessorP);
  static DeviceP registry_create  (CString aseid, AudioEngine &engine, const MakeDeviceP&);
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
  AudioProcessor *oproc = nullptr;
  MidiEventOutput midi_event_output;
  bool            has_event_input = false;
  bool            has_event_output = false;
};

/// Find index of parameter identified by `id` or return -1.
inline ssize_t
AudioParams::index (const uint32_t id) const
{
  if (id - 1 < count && ids[id - 1] == id) [[likely]]
    return id - 1;      // fast path for consecutive IDs
  if (id < count && ids[id] == id) [[likely]]
    return id - 1;      // fast handling of 0-based IDs
  auto [it,found] = Aux::binary_lookup_insertion_pos (ids, ids + count,
                                                      [] (auto a, auto b) { return a < b ? -1 : a > b; },
                                                      id);
  return found ? it - ids : -1;
}

/// Read current value of parameter identified by `id`.
inline double
AudioParams::value (uint32_t id) const
{
  const auto idx = index (id);
  return idx < 0 ? 0.0 : values[idx];
}

/// Write new value into parameter identified by `id`, return old value.
inline double
AudioParams::value (uint32_t id, double newval)
{
  const auto idx = index (id);
  if (idx < 0) return 0.0;
  const double old = values[idx];
  values[idx] = newval;
  if (!wprops[idx].expired()) {
    dirty_index (idx, true);
    changed = true;
  }
  return old;
}

/// Check if parameter is dirty (changed).
inline bool
AudioParams::dirty_index (uint32_t idx) const
{
  return bits[idx >> 6] & uint64_t (1) << (idx & (64-1));
}

/// Set or clear parameter dirty flag.
inline void
AudioParams::dirty_index (uint32_t idx, bool d)
{
  if (d)
    bits[idx >> 6] |= uint64_t (1) << (idx & (64-1));
  else
    bits[idx >> 6] &= ~(uint64_t (1) << (idx & (64-1)));
}

/// Handle atomic swapping of t0events around modifications by `F` [main-thread].
template<class F> void
AudioProcessor::modify_t0events (const F &mod)
{
  ASE_ASSERT_RETURN (this_thread_is_ase());
  MidiEventVector *t0events = nullptr;
  t0events = t0events_.exchange (t0events);
  if (!t0events)
    t0events = new std::vector<MidiEvent>;
  mod (*t0events);
  if (!t0events->empty())
    t0events = t0events_.exchange (t0events);
  delete t0events;
}

/// Assign MidiEvent::PARAM_VALUE event values to parameters.
inline void
AudioProcessor::apply_event (const MidiEvent &event)
{
  switch (event.type)
    {
    case MidiEvent::PARAM_VALUE:
      params_.value (event.param, event.pvalue);
      break;
    default: ;
    }
}

/// Process all input events via apply_event() and adjust_param().
/// This applies all incoming parameter changes,
/// events like NOTE_ON are not handled.
inline void
AudioProcessor::apply_input_events()
{
  MidiEventInput evinput = midi_event_input();
  for (const auto &event : evinput)
    switch (event.type) // event.message()
      {
      case MidiEvent::PARAM_VALUE:
        apply_event (event);
        adjust_param (event.param);
        break;
      default: ;
      }
}

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

// Call adjust_param() for all parameters.
inline void
AudioProcessor::adjust_all_params()
{
  for (size_t idx = 0; idx < params_.count; idx++)
    adjust_param (params_.ids[idx]);
}

/// Fetch `value` of parameter `id`.
inline double
AudioProcessor::get_param (Id32 paramid)
{
  const ssize_t idx = params_.index (paramid.id);
  if (idx < 0) [[unlikely]]
    return 0;
  return params_.values[idx];
}

/// Check if the parameter `dirty` flag is set.
/// Return `true` if the parameter value changed during render().
inline bool
AudioProcessor::check_dirty (Id32 paramid) const
{
  const ssize_t idx = params_.index (paramid.id);
  if (idx < 0) [[unlikely]]
    return false;
  return params_.dirty_index (idx);
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
  if constexpr (std::is_constructible<T, const AudioProcessor::ProcessorSetup&>::value)
    {
      auto make_shared = [] (CString aseid, AudioEngine &engine) -> AudioProcessorP {
        const AudioProcessor::ProcessorSetup psetup { aseid, engine };
        return std::make_shared<T> (psetup);
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
  String aseid = typeid_name<AudioProc>();
  const ProcessorSetup psetup { aseid, engine };
  std::shared_ptr<AudioProc> proc = std::make_shared<AudioProc> (psetup, args...);
  if (proc) {
    AudioProcessorP aproc = proc;
    aproc->ensure_initialized (nullptr);
  }
  return proc;
}

} // Ase

#endif // __ASE_PROCESSOR_HH__
