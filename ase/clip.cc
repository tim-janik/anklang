// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "clip.hh"
#include "track.hh"
#include "jsonipc/jsonipc.hh"
#include "project.hh"
#include "serialize.hh"
#include "platform.hh"
#include "compress.hh"
#include "path.hh"
#include "internal.hh"
#include <atomic>

#define CDEBUG(...)     Ase::debug ("ClipNote", __VA_ARGS__)
#define UDEBUG(...)     Ase::debug ("undo", __VA_ARGS__)

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
      xs["ppq"] << TRANSPORT_PPQN;
      OrderedEventsP event_vector = notes_.ordered_events<OrderedEventsV>();
      for (ClipNote cnote : *event_vector)
        {
          WritNode xn = xs["notes"].push();
          xn & cnote;
          xn.value().filter ([] (const ValueField &field) {
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
      xs["ppq"] >> ppq;
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
      emit_notify ("notes");
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

ClipImpl::EventImage::EventImage (const ClipNoteS &clipnotes)
{
  const size_t clipnotes_bytes = clipnotes.size() * sizeof (clipnotes[0]);
  cbuffer = zstd_compress (clipnotes.data(), clipnotes_bytes, 4);
  assert_return (cbuffer.size() > 0);
  ProjectImpl::undo_mem_counter += sizeof (*this) + cbuffer.size();
  UDEBUG ("ClipImpl: store undo (notes=%d): %d->%d (%f%%)", clipnotes.size(),
          clipnotes_bytes, cbuffer.size(), cbuffer.size() * 100.0 / clipnotes_bytes);
}

ClipImpl::EventImage::~EventImage()
{
  ProjectImpl::undo_mem_counter -= sizeof (*this) + cbuffer.size();
  UDEBUG ("ClipImpl: free undo mem: %d\n", sizeof (*this) + cbuffer.size());
}

void
ClipImpl::push_undo (const ClipNoteS &clipnotes, const String &undogroup)
{
  auto thisp = shared_ptr_from (this);
  EventImageP imagep = std::make_shared<EventImage> (clipnotes);
  undo_scope (undogroup) += [thisp, imagep, undogroup] () { thisp->apply_undo (*imagep, undogroup); };
}

void
ClipImpl::apply_undo (const EventImage &image, const String &undogroup)
{
  push_undo (notes_.copy(), undogroup);
  ClipNoteS onotes;
  const ssize_t osize = zstd_target_size (image.cbuffer);
  assert_return (osize >= 0 && osize == sizeof (onotes[0]) * (osize / sizeof (onotes[0])));
  onotes.resize (osize / sizeof (onotes[0]));
  const ssize_t rsize = zstd_uncompress (image.cbuffer, onotes.data(), osize);
  assert_return (rsize == osize);
  notes_.clear_silently();
  for (const ClipNote &note : onotes)
    notes_.insert (note);
  emit_notify ("notes");
}

size_t
ClipImpl::collapse_notes (EventsById &inotes, const bool preserve_selected)
{
  ClipNoteS copies = inotes.copy();
  size_t collapsed = 0;
  // sort notes by tick, keep order; delete from lhs, preserve newer entries on rhs
  std::stable_sort (copies.begin(), copies.end(), [] (const ClipNote &a, const ClipNote &b) {
    return a.tick < b.tick;
  });
  // remove duplicates at the same tick
  for (size_t i = 0; i < copies.size(); i++) {
    const ClipNote &note = copies[i];
    for (size_t j = i + 1; j < copies.size(); j++) {
      if (note.tick != copies[j].tick)
        break;
      if (note.key == copies[j].key && note.channel == copies[j].channel) {
        if (note.selected != copies[j].selected && preserve_selected)
          continue;
        // note has a successor at same tick, with same key, channel
        collapsed += inotes.remove (note);
      }
    }
  }
  return collapsed;
}

int32
ClipImpl::change_batch (const ClipNoteS &batch, const String &undogroup)
{
  bool changes = false, selections = false;
  // save undo image
  const ClipNoteS orig_notes = notes_.copy();
  // delete existing notes
  for (const auto &note : batch)
    if (note.id > 0 && (note.duration == 0 || note.channel < 0)) {
      changes |= notes_.remove (note);
      CDEBUG ("%s: delete notes: %d\n", __func__, note.id);
    }
  // modify *existing* notes
  for (const auto &note : batch)
    if (note.id > 0 && note.duration > 0 && note.channel >= 0) {
      ClipNote replaced;
      if (notes_.replace (note, &replaced) && !(note == replaced)) {
        replaced.selected = !replaced.selected;
        if (note == replaced)
          selections = true; // only selection changed
        else
          changes = true;
        CDEBUG ("%s: %s %d: new=%s old=%s\n", __func__, note == replaced ? "toggle" : "replace", note.id,
                stringify_clip_note (note), stringify_clip_note (replaced));
      }
    }
  // insert new notes
  for (const auto &note : batch)
    if (note.id <= 0 && note.duration > 0 && note.channel >= 0) {
      ClipNote ev = note;
      ev.id = next_noteid++;    // automatic id allocation for new notes
      assert_warn (ev.id >= MIDI_NOTE_ID_FIRST && ev.id <= MIDI_NOTE_ID_LAST);
      const bool replaced = notes_.insert (ev);
      changes |= !replaced;
      CDEBUG ("%s: insert: %s%s\n", __func__, stringify_clip_note (ev), replaced ? " (REPLACED?)" : "");
    }
  // collapse overlapping notes
  if (changes || selections) {
    const size_t collapsed = collapse_notes (notes_, true);
    changes = changes || collapsed;
    if (collapsed) CDEBUG ("%s: collapsed=%d\n", __func__, collapsed);
  }
  // queue undo
  if (!notes_.equals (orig_notes)) {
    if (changes)
      push_undo (orig_notes, undogroup.empty() ? "Change Notes" : undogroup);
    if (changes) CDEBUG ("%s: notes=%d undo_size: %fMB\n", __func__, notes_.size(), project()->undo_size_guess() / (1024. * 1024));
    emit_notify ("notes");
  }
  return 0;
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
          ClipNote index = { .tick = a };
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

String
stringify_clip_note (const ClipNote &n)
{
  return string_format ("{%d,%d,%d,%s,%d,%d,%f,%f}",
                        n.id, n.channel, n.key,
                        n.selected ? "true" : "false",
                        n.tick, n.duration, n.velocity, n.fine_tune);
}

} // Ase
