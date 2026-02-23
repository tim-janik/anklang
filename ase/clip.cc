// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first

#include "clip.hh"
#include "track.hh"
#include "project.hh"
#include "serialize.hh"
#include "internal.hh"

namespace te = tracktion::engine;

namespace Ase {

// == Clip ==
Clip::Clip () :
  all_notes (this, "all_notes"),
  end_tick (this, "end_tick")
{}

// == ClipNote ==
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

// == ClipStateListener ==
class ClipImpl::ClipStateListener : public juce::ValueTree::Listener {
  ClipImpl &aseclip_;
  juce::ValueTree clip_state_;
public:
  ClipStateListener (ClipImpl &aseclip) :
    aseclip_ (aseclip), clip_state_ (aseclip_.clip_->state)
  {
    clip_state_.addListener (this);
  }
  ~ClipStateListener() override
  {
    clip_state_.removeListener (this);
  }
  void
  valueTreePropertyChanged (juce::ValueTree &tree, const juce::Identifier &property) override
  {
    assert_return (tree == clip_state_);
    if (property == tracktion::engine::IDs::name)
      aseclip_.emit_notify ("name");
    else if (property == tracktion::engine::IDs::start ||
             property == tracktion::engine::IDs::length ||
             property == tracktion::engine::IDs::offset)
      {
        aseclip_.emit_notify ("start_tick");
        aseclip_.emit_notify ("stop_tick");
        aseclip_.emit_notify ("end_tick");
      }
    else
        aseclip_.emit_notify ("notes"); // Simplistic change detection for notes within state
  }
  void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
  void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
  void valueTreeParentChanged (juce::ValueTree&) override {}
  void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
};


// == ClipImpl ==
ClipImpl::ClipImpl (TrackImpl &parent)
{
  // Fallback for non-tracktion clips
  track_ = &parent;
}

ClipImpl::ClipImpl (tracktion::Clip &clip) :
  clip_ (&clip)
{
  state_listener_ = std::make_unique<ClipStateListener> (*this);
  if (auto timpl = SelectableHandle::find_selectable_handle<TrackImpl> (*clip.getTrack()))
    track_ = timpl;
}

ClipImplP
ClipImpl::from_trkn (tracktion::Clip &c)
{
  ClipImpl *clip = SelectableHandle::find_selectable_handle<ClipImpl> (c);
  if (clip)
    return shared_ptr_cast<ClipImpl> (clip);
  ClipImplP clipp = ClipImpl::make_shared (c);
  return clipp;
}

ClipImpl::~ClipImpl()
{
  state_listener_ = nullptr;
}

ProjectImpl*
ClipImpl::project () const
{
  return track_ ? track_->project() : nullptr;
}

bool
ClipImpl::needs_serialize() const
{
  return false;
}

void
ClipImpl::serialize (WritNode &xs)
{
  GadgetImpl::serialize (xs);
}

ssize_t
ClipImpl::clip_index () const
{
  return track_ ? track_->clip_index (*this) : -1;
}

void
ClipImpl::assign_range (int64 starttick, int64 stoptick)
{
  assert_return (clip_.get());
  auto &ts = clip_->edit.tempoSequence;
  double start_beats = double (starttick) / TRANSPORT_PPQN;
  double end_beats = double (stoptick) / TRANSPORT_PPQN;
  double duration_beats = end_beats - start_beats;

  if (duration_beats > 0)
    {
      auto start_time = ts.toTime (tracktion::BeatPosition::fromBeats (start_beats));
      auto duration_time = ts.toTime (tracktion::BeatPosition::fromBeats (duration_beats));
      // Note: duration in time might depend on tempo changes *during* the clip if we map strictly.
      // But setLength expects TimeDuration.
      // If we want to preserve BEAT duration, we might need setLength(BeatDuration)? No, setLength takes TimeDuration.
      // But MidiClip is usually beat based.
      // However, for assigning range on timeline, we convert beats to time.

      clip_->setStart (start_time, false, true);
      clip_->setLength (tracktion::TimeDuration::fromSeconds(duration_time.inSeconds()), true);
      // Simplistic conversion. Proper way: find time of end beat - time of start beat.
      auto end_time = ts.toTime (tracktion::BeatPosition::fromBeats (end_beats));
      clip_->setLength (end_time - start_time, true);
    }
}

ClipNoteS
ClipImpl::list_all_notes ()
{
  return get_all_notes();
}

void
ClipImpl::set_all_notes (const ClipNoteS &notes)
{
  ClipNoteS current = get_all_notes();
  // Mark all for deletion
  for (auto &n : current) n.duration = 0;

  ClipNoteS batch = current;
  batch.insert (batch.end(), notes.begin(), notes.end());
  change_batch (batch, "Set All Notes");
}

ClipNoteS
ClipImpl::get_all_notes () const
{
  ClipNoteS notes;
  if (!clip_.get()) return notes;
  auto mclip = dynamic_cast<te::MidiClip*> (clip_.get());
  if (!mclip) return notes;

  // Assuming single channel clip
  int channel = mclip->getMidiChannel().getChannelNumber() - 1;

  for (auto n : mclip->getSequence().getNotes())
    {
      ClipNote cn;
      cn.id = 1;
      cn.channel = channel;
      cn.key = n->getNoteNumber();
      cn.velocity = n->getVelocity() / 127.0f;
      cn.tick = n->getStartBeat().inBeats() * TRANSPORT_PPQN;
      cn.duration = n->getLengthBeats().inBeats() * TRANSPORT_PPQN;
      cn.selected = false;
      cn.fine_tune = 0;
      notes.push_back (cn);
    }
  return notes;
}

int64
ClipImpl::get_end_tick () const
{
  if (!clip_.get()) return 0;
  return stop_tick();
}

void
ClipImpl::set_end_tick (int64 etick)
{
  if (!clip_.get()) return;
  assign_range (start_tick(), etick);
}

int64
ClipImpl::start_tick () const
{
  if (!clip_.get()) return 0;
  auto &ts = clip_->edit.tempoSequence;
  return ts.toBeats (clip_->getPosition().getStart()).inBeats() * TRANSPORT_PPQN;
}

int64
ClipImpl::stop_tick () const
{
  if (!clip_.get()) return 0;
  auto &ts = clip_->edit.tempoSequence;
  return ts.toBeats (clip_->getPosition().getEnd()).inBeats() * TRANSPORT_PPQN;
}

/// Retrieve const vector with all notes ordered by tick.
ClipImpl::OrderedEventsP
ClipImpl::tick_events () const
{
  ClipNoteS notes = get_all_notes();
  return std::make_shared<const OrderedEventsV> (notes);
}

int32
ClipImpl::change_batch (const ClipNoteS &batch, const String &undogroup)
{
  if (!clip_.get()) return -1;
  auto mclip = dynamic_cast<te::MidiClip*> (clip_.get());
  assert_return (mclip, -1);

  auto &um = clip_->edit.getUndoManager();
  um.beginNewTransaction (juce::String (undogroup.empty() ? "Change Notes" : undogroup));

  for (const auto &note : batch)
    {
      if (note.duration == 0) // Delete
        {
          auto &seq = mclip->getSequence();
          for (auto n : seq.getNotes())
            {
              if (std::abs(n->getStartBeat().inBeats() * TRANSPORT_PPQN - note.tick) < 1 &&
                  n->getNoteNumber() == note.key)
                {
                  seq.removeNote (*n, &um);
                  break;
                }
            }
        }
      else if (note.id <= 0) // Insert
        {
          mclip->getSequence().addNote (note.key,
                                        tracktion::BeatPosition::fromBeats (double (note.tick) / TRANSPORT_PPQN),
                                        tracktion::BeatDuration::fromBeats (double (note.duration) / TRANSPORT_PPQN),
                                        note.velocity * 127,
                                        note.channel,
                                        &um);
        }
      else // Modify
        {
          auto &seq = mclip->getSequence();
          for (auto n : seq.getNotes())
            {
              if (std::abs(n->getStartBeat().inBeats() * TRANSPORT_PPQN - note.tick) < 1 &&
                  n->getNoteNumber() == note.key)
                {
                  n->setStartAndLength (tracktion::BeatPosition::fromBeats (double (note.tick) / TRANSPORT_PPQN),
                                        tracktion::BeatDuration::fromBeats (double (note.duration) / TRANSPORT_PPQN),
                                        &um);
                  n->setVelocity (note.velocity * 127, &um);
                  n->setNoteNumber (note.key, &um);
                  break;
                }
            }
        }
    }
  emit_notify ("notes");
  all_notes.notify();
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
