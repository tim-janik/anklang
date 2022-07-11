// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CLIP_HH__
#define __ASE_CLIP_HH__

#include <ase/project.hh>
#include <ase/eventlist.hh>
#include <ase/midievent.hh>

namespace Ase {

/// First (internal) MIDI note event ID (lower IDs are reserved for external notes).
constexpr const uint MIDI_NOTE_ID_FIRST = 0x10000001;
/// Last valid (internal) MIDI note event ID.
constexpr const uint MIDI_NOTE_ID_LAST = 0xfffffffe;

class ClipImpl : public GadgetImpl, public virtual Clip {
public:
  struct CmpNoteTicks { int operator() (const ClipNote &a, const ClipNote &b) const; };
  struct CmpNoteIds   { int operator() (const ClipNote &a, const ClipNote &b) const; };
  using EventsById = EventList<ClipNote,CmpNoteIds>;
private:
  int64 starttick_ = 0, stoptick_ = 0, endtick_ = 0;
  EventsById notes_;
  Connection notifytrack_;
  using OrderedEventsV = OrderedEventList<ClipNote,CmpNoteTicks>;
  struct EventImage {
    String cbuffer;
    static_assert (std::is_trivially_copyable<ClipNoteS::value_type>::value);
    EventImage (const ClipNoteS &cnotes);
    ~EventImage();
  };
  using EventImageP = std::shared_ptr<EventImage>;
  void        apply_undo     (const EventImage &image, const String &undogroup);
  static void collapse_notes (EventsById &notes, bool preserve_selected);
public:
  class Generator;
protected:
  TrackImpl *track_ = nullptr;
  Connection ontrackchange_;
  explicit ClipImpl         (TrackImpl &parent);
  virtual ~ClipImpl         ();
  void     serialize        (WritNode &xs) override;
  ssize_t  clip_index       () const;
public:
  using OrderedEventsP = OrderedEventsV::ConstP;
  OrderedEventsP tick_events    () const;
  ProjectImpl*   project        () const;
  void           push_undo      (const ClipNoteS &cnotes, const String &undogroup);
  UndoScope      undo_scope     (const String &scopename) { return project()->undo_scope (scopename); }
  int64          start_tick     () const override { return starttick_; }
  int64          stop_tick      () const override { return stoptick_; }
  int64          end_tick       () const override { return endtick_; }
  void           assign_range   (int64 starttick, int64 stoptick) override;
  ClipNoteS      list_all_notes () override;
  bool           needs_serialize() const;
  int32          change_batch   (const ClipNoteS &notes, const String &undogroup) override;
  ASE_DEFINE_MAKE_SHARED (ClipImpl);
};

/// Generator for MIDI events.
class ClipImpl::Generator {
  using EventS = ClipImpl::OrderedEventsP;
  int64  last_ = I63MAX;
  int64  xtick_ = 0; // external tick (API)
  int64  itick_ = 0; // internal tick (clip position)
  int64  start_offset_ = 0;
  int64  loop_start_ = 0;
  int64  loop_end_ = I63MAX;
  EventS events_;
  bool   muted_ = false;
  friend class ClipImpl;
public:
  /// Handler for generated MIDI events.
  using Receiver = std::function<void (int64 tick, MidiEvent &event)>;
  explicit Generator  () = default;
  void  setup         (const ClipImpl &clip);
  void  jumpto        (int64 target_tick);
  int64 generate      (int64 target_tick, const Receiver &receiver);
  /// Mute MIDI note generation.
  bool  muted         () const { return muted_; }
  /// Assign muted state.
  void  muted         (bool b) { muted_ = b; }
  /// Initial offset in ticks.
  int64 start_offset  () const { return start_offset_; }
  /// Loop start in ticks.
  int64 loop_start    () const { return loop_start_; }
  /// Loop end in ticks.
  int64 loop_end      () const { return loop_end_; }
  /// Maximum amount of ticks during playback
  int64 play_length   () const { return last_; }
  /// Current playback position in ticks.
  int64 play_position () const { return xtick_; }
  /// Check if playback is done.
  bool  done          () const { return play_position() >= play_length(); }
  /// Position within clip as tick.
  int64 clip_position () const { return itick_; }
};
using ClipImplGeneratorS = std::vector<ClipImpl::Generator>;


// == Implementation Details ==
inline int
ClipImpl::CmpNoteIds::operator () (const ClipNote &a, const ClipNote &b) const
{
  return Aux::compare_lesser (a.id, b.id);
}

inline int
ClipImpl::CmpNoteTicks::operator() (const ClipNote &a, const ClipNote &b) const
{
  // tick is neccessary primary key for playback
  const int tcmp = Aux::compare_lesser (a.tick, b.tick);
  if (ASE_ISLIKELY (tcmp))
    return tcmp;
  const int kcmp = Aux::compare_lesser (a.key, b.key);
  if (ASE_ISLIKELY (kcmp))
    return kcmp;
  const int ccmp = Aux::compare_lesser (a.channel, b.channel);
  if (ASE_ISLIKELY (ccmp))
    return ccmp;
  // allow selected to "override" a previous unselected element
  const int scmp = Aux::compare_lesser (a.selected, b.selected);
  if (ASE_UNLIKELY (scmp))
    return scmp;
  return Aux::compare_lesser (a.id, b.id);
}

String stringify_clip_note (const ClipNote &n);

} // Ase

#endif // __ASE_CLIP_HH__
