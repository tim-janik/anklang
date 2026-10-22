// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first

#include "clip.hh"
#include "track.hh"
#include "project.hh"
#include "internal.hh"

namespace te = tracktion::engine;

namespace Ase {

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
    else if (property == tracktion::engine::IDs::mute)
      aseclip_.emit_notify ("muted");
    else if (property == tracktion::engine::IDs::volDb || property == tracktion::engine::IDs::gain)
      aseclip_.emit_notify ("volume");
    else if (property == tracktion::engine::IDs::pan)
      aseclip_.emit_notify ("pan");
    else
        aseclip_.emit_notify ("notes"); // Simplistic change detection for notes within state
  }
  void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
  void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
  void valueTreeParentChanged (juce::ValueTree&) override {}
  void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
};


// == ClipImpl ==
ClipImpl::ClipImpl (tracktion::Clip &clip) :
  clip_ (&clip)
{
  register_ase_obj (this, clip);
  state_listener_ = std::make_unique<ClipStateListener> (*this);
}

ClipImplP
ClipImpl::from_trkn (tracktion::Clip &c)
{
  ClipImpl *clip = find_ase_obj<ClipImpl> (c);
  if (clip)
    return shared_ptr_cast<ClipImpl> (clip);
  ClipImplP clipp = ClipImpl::make_shared (c);
  return clipp;
}

ClipImpl::~ClipImpl()
{
  unregister_ase_obj (this, clip_.get());
  state_listener_ = nullptr;
}

ssize_t
ClipImpl::clip_index () const
{
  if (auto c = clip_.get())
    if (auto timpl = find_ase_obj<TrackImpl> (c->getTrack()))
      return timpl->clip_index (*this);
  return -1;
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
  return all_notes();
}

void
ClipImpl::all_notes (const ClipNoteS &notes)
{
  ClipNoteS current = all_notes();
  // Mark all for deletion
  for (auto &n : current) n.duration = 0;

  ClipNoteS batch = current;
  batch.insert (batch.end(), notes.begin(), notes.end());
  change_batch (batch, "Set All Notes");
}

ClipNoteS
ClipImpl::all_notes () const
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
ClipImpl::end_tick () const
{
  if (!clip_.get()) return 0;
  return stop_tick();
}

void
ClipImpl::end_tick (int64 etick)
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

bool
ClipImpl::is_muted () const
{
  if (!clip_.get()) return false;
  return clip_->isMuted();
}

void
ClipImpl::set_muted (bool muted)
{
  if (!clip_.get()) return;
  auto &um = clip_->edit.getUndoManager();
  um.beginNewTransaction ("Set Clip Muted");
  clip_->setMuted (muted);
}

double
ClipImpl::volume () const
{
  if (!clip_.get()) return 0.0;
  auto mclip = dynamic_cast<te::MidiClip*> (clip_.get());
  if (mclip)
    return mclip->getVolumeDb();
  auto aclip = dynamic_cast<te::AudioClipBase*> (clip_.get());
  if (aclip)
    return aclip->getGainDB();
  return 0.0;
}

void
ClipImpl::volume (double db)
{
  if (!clip_.get()) return;
  auto &um = clip_->edit.getUndoManager();
  um.beginNewTransaction ("Set Clip Volume");
  auto mclip = dynamic_cast<te::MidiClip*> (clip_.get());
  if (mclip)
    mclip->setVolumeDb (float (db));
  else
    {
      auto aclip = dynamic_cast<te::AudioClipBase*> (clip_.get());
      if (aclip)
        aclip->setGainDB (float (db));
    }
}

double
ClipImpl::pan () const
{
  if (!clip_.get()) return 0.0;
  auto aclip = dynamic_cast<te::AudioClipBase*> (clip_.get());
  if (aclip)
    return aclip->getPan();
  return 0.0;
}

void
ClipImpl::pan (double panval)
{
  if (!clip_.get()) return;
  auto &um = clip_->edit.getUndoManager();
  um.beginNewTransaction ("Set Clip Pan");
  auto aclip = dynamic_cast<te::AudioClipBase*> (clip_.get());
  if (aclip)
    aclip->setPan (float (panval));
}

TelemetryFieldS
ClipImpl::telemetry () const
{
  TelemetryFieldS v;
  return v;
}

void
ClipImpl::update_telemetry ()
{
}

void
ClipImpl::remove_self ()
{
  auto *clip = clip_.get();
  if (clip) {
    // Remove from te::Track's clip collection
    clip->removeFromParent();
    // Clear references
    clip_ = SelectableWeakref<tracktion::Clip>{};
    state_listener_ = nullptr;
  }
  GadgetImpl::remove_self();
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
  emit_notify ("all_notes");
  return 0;
}

} // Ase
