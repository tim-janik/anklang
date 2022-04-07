// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clip.hh"
#include "track.hh"
#include "jsonipc/jsonipc.hh"
#include "project.hh"
#include "serialize.hh"
#include "internal.hh"
#include <atomic>

namespace Ase {

bool
ClipNote::operator== (const ClipNote &o) const
{
  return (tick      == o.tick &&
          id        == o.id &&
          channel   == o.channel &&
          key       == o.key &&
          selected  == o.selected &&
          duration  == o.duration &&
          velocity  == o.velocity &&
          fine_tune == o.fine_tune);
}

// == ClipImpl ==
JSONIPC_INHERIT (ClipImpl, Clip);

ClipImpl::ClipImpl (TrackImpl &parent)
{
  track_ = &parent;
  notifytrack_ = on_event ("notify", [this] (const Event &event) {
    if (track_)
      track_->update_clips();
  });
}

ClipImpl::~ClipImpl()
{}

ProjectImpl*
ClipImpl::project () const
{
  return track_ ? track_->project() : nullptr;
}

bool
ClipImpl::needs_serialize() const
{
  return notes_.size() > 0;
}

static std::atomic<uint> next_noteid { MIDI_NOTE_ID_FIRST };

void
ClipImpl::serialize (WritNode &xs)
{
  GadgetImpl::serialize (xs);

  // save notes, along with their quantization
  if (xs.in_save())
    {
      xs["ppq"] & TRANSPORT_PPQN;
      OrderedEventsP event_vector = notes_.ordered_events<OrderedEventsV>();
      for (ClipNote cnote : *event_vector)
        {
          WritNode xn = xs["notes"].push();
          xn & cnote;
          xn.value().purge_r ([] (const ValueField &field) {
            if (field.name == "id" || field.name == "selected")
              return true;
            return false;
          });
        }
    }
  // load notes, re-quantize, re-assign ids
  if (xs.in_load())
    {
      int64 ppq = TRANSPORT_PPQN;
      xs["ppq"] & ppq;
      std::vector<ClipNote> cnotes;
      xs["notes"] & cnotes;
      long double ppqfactor = TRANSPORT_PPQN / (long double) ppq;
      for (const auto &cnote : cnotes)
        {
          ClipNote note = cnote;
          note.id = next_noteid++; // automatic id allocation for new notes
          note.tick = llrintl (note.tick * ppqfactor);
          note.duration = llrintl (note.duration * ppqfactor);
          note.selected = false;
          notes_.insert (note);
        }
      emit_event ("notify", "notes");
      // TODO: serialize range
    }
}

ssize_t
ClipImpl::clip_index () const
{
  return track_ ? track_->clip_index (*this) : -1;
}

void
ClipImpl::assign_range (int64 starttick, int64 stoptick)
{
  assert_return (starttick >= 0);
  assert_return (stoptick >= starttick);
  const auto last_starttick_ = starttick_;
  const auto last_stoptick_ = stoptick_;
  const auto last_endtick_ = endtick_;
  starttick_ = starttick;
  stoptick_ = stoptick;
  endtick_ = std::max (starttick_, stoptick_);
  if (last_endtick_ != endtick_)
    emit_event ("notify", "end_tick");
  if (last_stoptick_ != stoptick_)
    emit_event ("notify", "stop_tick");
  if (last_starttick_ != starttick_)
    emit_event ("notify", "start_tick");
}

ClipNoteS
ClipImpl::list_all_notes ()
{
  ClipNoteS cnotes;
  auto events = tick_events();
  cnotes.assign (events->begin(), events->end());
  return cnotes;
}

/// Retrieve const vector with all notes ordered by tick.
ClipImpl::OrderedEventsP
ClipImpl::tick_events () const
{
  return const_cast<ClipImpl*> (this)->notes_.ordered_events<OrderedEventsV> ();
}

static bool
find_same_note (const EventList<ClipNote,ClipImpl::CmpNoteIds> &notes, ClipNote &ev)
{
  for (const auto &e : notes)
    if (e.key == ev.key && e.tick == ev.tick && e.selected == ev.selected)
      {
        ev = e;
        return true;
      }
  return false;
}

/// Change note `id`, or delete (`duration=0`) or create (`id=-1`) it.
int32
ClipImpl::change_note (int32 id, int64 tick, int64 duration, int32 key, int32 fine_tune, double velocity, bool selected)
{
  assert_return (duration >= 0, 0);
  if (tick < 0)
    return -1;
  if (id < 0 && duration > 0)
    id = next_noteid++; // automatic id allocation for new notes
  const uint uid = id;
  assert_return (uid >= MIDI_NOTE_ID_FIRST && uid <= MIDI_NOTE_ID_LAST, 0);
  ClipNote ev;
  ev.tick = tick;
  ev.key = key;
  ev.selected = selected;
  ev.id = 0;
  if (find_same_note (notes_, ev) && ev.id != id)
    notes_.remove (ev);
  ev.id = id;
  ev.channel = 0;
  ev.duration = duration;
  ev.fine_tune = fine_tune;
  ev.velocity = velocity;
  int ret = ev.id;
  if (duration > 0)
    notes_.insert (ev);
  else
    ret = notes_.remove (ev) ? 0 : -1;
  emit_event ("notify", "notes");
  return ret;
}

bool
ClipImpl::toggle_note (int32 id, bool selected)
{
  bool was_selected = false;
  ClipNote ev;
  ev.id = id;
  ev.selected = !selected;
  const ClipNote *ce = notes_.lookup (ev);
  if (ce)
    {
      was_selected = ce->selected;
      ev = *ce;
      ev.selected = selected;   // look for merge candidate
      if (find_same_note (notes_, ev) && ev.id != id)
        notes_.remove (ev);     // remove candidate
      ev = *ce;
      notes_.remove (ev);
      ev.selected = selected;
      notes_.insert (ev);
      emit_event ("notify", "notes");
    }
  return was_selected;
}

// == ClipImpl::Generator ==
/// Create generator from clip.
void
ClipImpl::Generator::setup (const ClipImpl &clip)
{
  ProjectImpl *p = clip.project();
  TickSignature tsig;
  if (p)
    tsig = p->signature();
  events_ = clip.tick_events();
  muted_ = false;
  start_offset_ = 0;
  loop_start_ = 0;
  loop_end_ = tsig.bar_ticks() * 2;
  const int LOOPS = 2;
  last_ = loop_end_ - start_offset_ + LOOPS * (loop_end_ - loop_start_);
  if (true) // keep looping
    last_ = M52MAX;
}

/// Assign new play_position() (and clip_position()), preserves all other state.
void
ClipImpl::Generator::jumpto (int64 target_tick)
{
  // negative ticks indicate delay
  if (target_tick < 0)
    {
      xtick_ = target_tick;
      itick_ = xtick_;
      return;
    }
  // external position
  xtick_ = std::min (target_tick, play_length());
  // advance internal position by externally observable ticks
  itick_ = start_offset_;
  return_unless (xtick_ > 0);
  // beyond loop end
  if (itick_ >= loop_end_)
    {
      itick_ = xtick_;
      return;
    }
  // until loop end
  int64 delta = xtick_;
  const int64 frag = std::min (delta, loop_end_ - itick_);
  delta -= frag;
  itick_ += frag;
  if (itick_ == loop_end_)
    {
      itick_ = loop_start_;
      // within loop (loop count is discarded)
      if (delta)
        {
          const int64 frac = delta % (loop_end_ - loop_start_);
          itick_ += frac;
        }
    }
}

/// Advance tick and call `receiver` for generated events.
int64
ClipImpl::Generator::generate (int64 target_tick, const Receiver &receiver)
{
  if (0)
    printerr ("generate: %d < %d (%+d) && %d > %d (%+d) (loop: %d %d) i=%d\n", xtick_, last_, xtick_ < last_,
              target_tick, xtick_, target_tick > xtick_,
              loop_start_, loop_end_, itick_);
  const int64 old_xtick = xtick_;
  return_unless (xtick_ < last_ && target_tick > xtick_, xtick_ - old_xtick);
  int64 ticks = std::min (target_tick, last_) - xtick_;
  // consume delay
  if (xtick_ < 0)
    {
      const int64 delta = std::min (ticks, -xtick_);
      ticks -= delta;
      xtick_ += delta;
      itick_ += delta;
      if (itick_ == 0)
        itick_ = start_offset_;
    }
  // here: ticks == 0 || xtick_ >= 0
  while (ticks > 0)
    {
      // advance
      const int64 delta = itick_ < loop_end_ ? std::min (ticks, loop_end_ - itick_) : ticks;
      ticks -= delta;
      const int64 x = xtick_;
      xtick_ += delta;
      const int64 a = itick_;
      itick_ += delta;
      const int64 b = itick_;
      if (itick_ == loop_end_)
        itick_ = loop_start_;
      // generate notes within [a,b)
      if (receiver && !muted_)
        {
          ClipNote index = { .tick = a, .id = 0, .key = 0, };
          const ClipNote *event = events_->lookup_after (index);
          while (event && event->tick < b)
            {
              MidiEvent midievent = make_note_on (event->channel, event->key, event->velocity, event->fine_tune, event->id);
              const int64 noteon_tick = x + event->tick - a;
              receiver (noteon_tick, midievent);
              midievent.type = MidiEvent::NOTE_OFF;
              receiver (noteon_tick + event->duration, midievent);
              event++;
              if (event == &*events_->end())
                break;
            }
        }
    }
  return xtick_ - old_xtick;
}

} // Ase
