// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_CLIP_HH__
#define __ASE_CLIP_HH__

#include <ase/gadget.hh>
#include <ase/eventlist.hh>

namespace Ase {

/// First (internal) MIDI note event ID (lower IDs are reserved for external notes).
constexpr const uint MIDI_NOTE_ID_FIRST = 0x10000001;
/// Last valid (internal) MIDI note event ID.
constexpr const uint MIDI_NOTE_ID_LAST = 0xfffffffe;

class ClipImpl : public GadgetImpl, public virtual Clip {
  int32 starttick_ = 0, stoptick_ = 0, endtick_ = 0;
  struct CmpNoteTicks { int operator() (const ClipNote &a, const ClipNote &b) const; };
  struct CmpNoteIds   { int operator() (const ClipNote &a, const ClipNote &b) const; };
  EventList<ClipNote,CmpNoteIds> notes_;
  using OrderedEventsV = OrderedEventList<ClipNote,CmpNoteTicks>;
protected:
  TrackImpl *track_ = nullptr;
  Connection ontrackchange_;
  explicit ClipImpl         (TrackImpl &parent);
  virtual ~ClipImpl         ();
  ssize_t  clip_index       () const;
  bool     find_key_at_tick (ClipNote &ev);
public:
  using OrderedEventsP = OrderedEventsV::ConstP;
  OrderedEventsP tick_events    ();
  int32          start_tick     () const override { return starttick_; }
  int32          stop_tick      () const override { return stoptick_; }
  int32          end_tick       () const override { return endtick_; }
  void           assign_range   (int32 starttick, int32 stoptick) override;
  ClipNoteS      list_all_notes () override;
  int32          change_note    (int32 id, int32 tick, int32 duration, int32 key, int32 fine_tune, double velocity) override;
  ASE_DEFINE_MAKE_SHARED (ClipImpl);
};

// == Implementation Details ==
inline int
ClipImpl::CmpNoteIds::operator() (const ClipNote &a, const ClipNote &b) const
{
  return Aux::compare_lesser (a.id, b.id);
}

inline int
ClipImpl::CmpNoteTicks::operator() (const ClipNote &a, const ClipNote &b) const
{
  const int tcmp = Aux::compare_lesser (a.tick, b.tick);
  if (ASE_ISLIKELY (tcmp))
    return tcmp;
  const int kcmp = Aux::compare_lesser (a.key, b.key);
  if (ASE_ISLIKELY (kcmp))
    return kcmp;
  return Aux::compare_lesser (a.id, b.id);
}

} // Ase

#endif // __ASE_CLIP_HH__
