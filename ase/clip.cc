// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clip.hh"
#include "track.hh"
#include "jsonipc/jsonipc.hh"
#include "transport.hh"
#include "serialize.hh"
#include "internal.hh"
#include <atomic>

namespace Ase {

// == ClipImpl ==
JSONIPC_INHERIT (ClipImpl, Clip);

ClipImpl::ClipImpl (TrackImpl &parent)
{
  track_ = &parent;
}

ClipImpl::~ClipImpl()
{}

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
ClipImpl::tick_events ()
{
  return notes_.ordered_events<OrderedEventsV> ();
}

/// Change note `id`, or delete (`duration=0`) or create (`id=-1`) it.
int32
ClipImpl::change_note (int32 id, int64 tick, int64 duration, int32 key, int32 fine_tune, double velocity)
{
  if (id < 0 && duration > 0)
    id = next_noteid++; // automatic id allocation for new notes
  const uint uid = id;
  assert_return (uid >= MIDI_NOTE_ID_FIRST && uid <= MIDI_NOTE_ID_LAST, 0);
  assert_return (duration >= 0, 0);
  if (tick < 0)
    return -1;
  ClipNote ev;
  ev.tick = tick;
  ev.key = key;
  ev.id = 0;
  if (find_key_at_tick (ev) && ev.id != id)
    notes_.remove (ev);
  ev.id = id;
  ev.channel = 0;
  ev.duration = duration;
  ev.fine_tune = fine_tune;
  ev.velocity = velocity;
  ev.selected = false;
  int ret = ev.id;
  if (duration > 0)
    notes_.insert (ev);
  else
    ret = notes_.remove (ev) ? 0 : -1;
  emit_event ("notify", "notes");
  return ret;
}

bool
ClipImpl::find_key_at_tick (ClipNote &ev)
{
  for (const auto &e : notes_)
    if (e.key == ev.key && e.tick == ev.tick)
      {
        ev = e;
        return true;
      }
  return false;
}

} // Ase
