// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_MIDI_EVENT_HH__
#define __ASE_MIDI_EVENT_HH__

#include <ase/memory.hh>
#include <ase/queuemux.hh>
#include <ase/mathutils.hh>

namespace Ase {

// == Forward Declarations ==
enum class MusicalTuning : uint8;

/// Type of MIDI Events.
enum class MidiEventType : uint8_t {
  PARAM_VALUE      = 0x70, /// Ase internal Parameter update.
  NOTE_OFF         = 0x80,
  NOTE_ON          = 0x90,
  AFTERTOUCH       = 0xA0, ///< Key Pressure, polyphonic aftertouch
  CONTROL_CHANGE   = 0xB0, ///< Control Change
  PROGRAM_CHANGE   = 0xC0,
  CHANNEL_PRESSURE = 0xD0, ///< Channel Aftertouch
  PITCH_BEND       = 0xE0,
  SYSEX            = 0xF0,
};

/// Extended type information for MidiEvent.
enum class MidiMessage : int32_t {
  NONE                          = 0,
  ALL_SOUND_OFF                 = 120,
  RESET_ALL_CONTROLLERS         = 121,
  LOCAL_CONTROL                 = 122,
  ALL_NOTES_OFF                 = 123,
  OMNI_MODE_OFF                 = 124,
  OMNI_MODE_ON                  = 125,
  MONO_MODE_ON                  = 126,
  POLY_MODE_ON                  = 127,
  PARAM_VALUE                   = 0x70,
  NOTE_OFF                      = 0x80,
  NOTE_ON                       = 0x90,
  AFTERTOUCH                    = 0xA0,
  CONTROL_CHANGE                = 0xB0,
  PROGRAM_CHANGE                = 0xC0,
  CHANNEL_PRESSURE              = 0xD0,
  PITCH_BEND                    = 0xE0,
  SYSEX                         = 0xF0,
};

/// MidiEvent data structure.
struct MidiEvent {
  using enum MidiEventType;
  static_assert (AUDIO_BLOCK_MAX_RENDER_SIZE <= 2048); // -2048…+2047 fits frame
  int       frame : 12;  ///< Offset into current block, delayed if negative
  uint      channel : 4; ///< 0…15 for standard events
  MidiEventType type;    ///< MidiEvent type, one of the MidiEventType members
  union {
    uint8   key;        ///< NOTE, KEY_PRESSURE MIDI note, 0…0x7f, 60 = middle C at 261.63 Hz.
    uint8   fragment;   ///< Flag for multi-part control change mesages.
  };
  union {
    uint    length;     ///< Data event length of byte array.
    uint    param;      ///< PROGRAM_CHANGE (program), CONTROL_CHANGE (controller):0…0x7f; PARAM_VALUE:uint32_t
    uint    noteid;     ///< NOTE, identifier for note expression handling or 0xffffffff.
  };
  union {
    char   *data;       ///< Data event byte array.
    double  pvalue;     ///< Numeric parameter value, PARAM_VALUE.
    struct {
      float value;      ///< CONTROL_CHANGE 0…+1, CHANNEL_PRESSURE, 0…+1, PITCH_BEND -1…+1
      uint  cval;       ///< CONTROL_CHANGE control value, 0…0x7f
    };
    struct {
      float velocity;   ///< NOTE, KEY_PRESSURE, CHANNEL_PRESSURE, 0…+1
      float tuning;     ///< NOTE, fine tuning in ±cents
    };
  };
  explicit    MidiEvent (MidiEventType etype = MidiEventType (0));
  /*copy*/    MidiEvent (const MidiEvent &other);
  MidiEvent&  operator= (const MidiEvent &other);
  /*des*/    ~MidiEvent ()      {}
  MidiMessage message   () const;
  std::string to_string () const;
};

MidiEvent make_note_on     (uint16 chnl, uint8 mkey, float velo, float tune = 0, uint nid = 0xffffff);
MidiEvent make_note_off    (uint16 chnl, uint8 mkey, float velo, float tune = 0, uint nid = 0xffffff);
MidiEvent make_aftertouch  (uint16 chnl, uint8 mkey, float velo, float tune = 0, uint nid = 0xffffff);
MidiEvent make_pressure    (uint16 chnl, float velo);
MidiEvent make_control     (uint16 chnl, uint prm, float val);
MidiEvent make_control8    (uint16 chnl, uint prm, uint8 cval);
MidiEvent make_program     (uint16 chnl, uint prgrm);
MidiEvent make_pitch_bend  (uint16 chnl, float val);
MidiEvent make_param_value (uint param, double pvalue);

/// A stream of writable MidiEvent structures.
class MidiEventOutput {
  std::vector<MidiEvent> events_; // TODO: use O(1) allocator
public:
  explicit         MidiEventOutput ();
  void             append          (int16_t frame, const MidiEvent &event);
  const MidiEvent* begin           () const noexcept { return &*events_.begin(); }
  const MidiEvent* end             () const noexcept { return &*events_.end(); }
  size_t           size            () const noexcept { return events_.size(); }
  bool             empty           () const noexcept { return events_.empty(); }
  void             clear           () noexcept       { events_.clear(); }
  bool             append_unsorted (int16_t frame, const MidiEvent &event);
  void             ensure_order    ();
  int64_t          last_frame      () const ASE_PURE;
  size_t           capacity        () const noexcept    { return events_.capacity(); }
  void             reserve         (size_t n)           { events_.reserve (n); }
  std::vector<MidiEvent>
  const&           vector          () { return events_; }
};

/// An in-order MidiEvent reader for multiple MidiEvent sources.
template<size_t MAXQUEUES>
class MidiEventReader : QueueMultiplexer<MAXQUEUES,std::vector<MidiEvent>::const_iterator> {
  using Base = QueueMultiplexer<MAXQUEUES,std::vector<MidiEvent>::const_iterator>;
  ASE_CLASS_NON_COPYABLE (MidiEventReader);
public:
  using iterator = Base::iterator;
  using Base::assign;
  size_t   events_pending  () const { return this->count_pending(); }
  iterator begin           ()       { return this->Base::begin(); }
  iterator end             ()       { return this->Base::end(); }
  using VectorArray = std::array<const std::vector<MidiEvent>*, MAXQUEUES>;
  /*ctor*/ MidiEventReader (const VectorArray &midi_event_vector_array = VectorArray());
};

// == MidiEventReader ==
template<size_t MAXQUEUES>
MidiEventReader<MAXQUEUES>::MidiEventReader (const VectorArray &midi_event_vector_array)
{
  assign (midi_event_vector_array);
}

inline int
QueueMultiplexer_priority (const MidiEvent &e)
{
  return e.frame;
}

/// Components of a MIDI note.
struct MidiNote {
  static constexpr const int NMIN = 0;
  static constexpr const int NMAX = 131;
  static constexpr const int NVOID = NMAX + 1;
  static constexpr const int KAMMER_NOTE = 69; // A' - Kammer Frequency
  static constexpr const int KAMMER_OCTAVE = +1;
  static float note_to_freq (MusicalTuning tuning, int note, float kammer_freq);
};

/// Convert MIDI note to Hertz for a MusicalTuning and `kammer_freq`.
extern inline float
MidiNote::note_to_freq (MusicalTuning tuning, int note, float kammer_freq)
{
  if (ASE_ISLIKELY (note >= -131) && ASE_ISLIKELY (note <= 131))
    return semitone_tables_265[uint (tuning)][note - KAMMER_NOTE] * kammer_freq;
  return 0;
}

} // Ase

#endif // __ASE_MIDI_EVENT_HH__
