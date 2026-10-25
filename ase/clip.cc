// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first

#include "clip.hh"
#include "track.hh"
#include "project.hh"
#include "internal.hh"
#include <map>

namespace te = tracktion::engine;

namespace Ase {

// == ClipStateListener ==
class ClipImpl::ClipStateListener : public juce::ValueTree::Listener {
  ClipImpl &aseclip_;
  juce::ValueTree clip_state_, tempo_state_;
  // Handles refer to the Edit note trees, including notes held by undo.
  std::map<int32, juce::ValueTree> note_states_;
  int32 next_note_id_ = 0;
public:
  ClipStateListener (ClipImpl &aseclip) :
    aseclip_ (aseclip), clip_state_ (aseclip_.clip_->state), tempo_state_ (aseclip_.clip_->edit.tempoSequence.getState())
  {
    clip_state_.addListener (this);
    tempo_state_.addListener (this);
  }
  ~ClipStateListener() override
  {
    tempo_state_.removeListener (this);
    clip_state_.removeListener (this);
  }
  void
  prune_notes ()
  {
    std::erase_if (note_states_, [] (const auto &entry) { return entry.second.getReferenceCount() == 1; });
  }
  ClipNote
  describe_note (const te::MidiNote &note)
  {
    int32 id = 0;
    for (const auto &entry : note_states_)
      if (entry.second == note.state)
        {
          id = entry.first;
          break;
        }
    if (!id)
      {
        id = ++next_note_id_;
        note_states_.emplace (id, note.state);
      }
    return { id, note.getNoteNumber(), note.getStartBeat().inBeats(), note.getLengthBeats().inBeats(),
             note.getVelocity(), note.getColour(), note.isMute() };
  }
  te::MidiNote*
  find_note (te::MidiList &sequence, int32 id)
  {
    auto entry = note_states_.find (id);
    return entry == note_states_.end() ? nullptr : sequence.getNoteFor (entry->second);
  }
  void
  notify_notes (const juce::ValueTree &tree)
  {
    if (tree == clip_state_ || tree.isAChildOf (clip_state_))
      {
        aseclip_.emit_notify ("notes");
        aseclip_.emit_notify ("all_notes");
      }
  }
  void
  valueTreePropertyChanged (juce::ValueTree &tree, const juce::Identifier &property) override
  {
    if (tree.hasType (te::IDs::NOTE))
      notify_notes (tree);
    if (tree == tempo_state_ || tree.isAChildOf (tempo_state_))
      {
        // Tempo and time signature changes can move clip beats
        aseclip_.emit_notify ("start_beat");
        aseclip_.emit_notify ("end_beat");
      }
    return_unless (tree == clip_state_);
    if (property == tracktion::engine::IDs::name)
      aseclip_.emit_notify ("name");
    else if (property == tracktion::engine::IDs::start ||
             property == tracktion::engine::IDs::length ||
             property == tracktion::engine::IDs::offset)
      {
        aseclip_.emit_notify ("position");
        aseclip_.emit_notify ("start_beat");
        aseclip_.emit_notify ("end_beat");
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
  void valueTreeChildAdded (juce::ValueTree &parent, juce::ValueTree&) override        { notify_notes (parent); }
  void valueTreeChildRemoved (juce::ValueTree &parent, juce::ValueTree&, int) override { notify_notes (parent); }
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

String
ClipImpl::name () const
{
  return clip_.get() ? clip_->getName().toStdString() : "";
}

void
ClipImpl::name (const std::string &n)
{
  return_unless (clip_.get());
  clip_->edit.getUndoManager().beginNewTransaction ("Set Clip Name");
  clip_->setName (juce::String (n));
}

ClipPosition
ClipImpl::position () const
{
  return_unless (clip_.get(), {});
  const auto p = clip_->getPosition();
  return { { p.time.getStart().inSeconds(), p.time.getEnd().inSeconds() }, p.offset.inSeconds() };
}

void
ClipImpl::position (const ClipPosition &position)
{
  assert_return (clip_.get());
  clip_->edit.getUndoManager().beginNewTransaction ("Set Clip Position");
  clip_->setPosition ({ { tracktion::TimePosition::fromSeconds (position.time.start),
                         tracktion::TimePosition::fromSeconds (position.time.end) },
                       tracktion::TimeDuration::fromSeconds (position.offset) });
}

ClipNoteS
ClipImpl::list_all_notes ()
{
  return all_notes();
}

void
ClipImpl::all_notes (const ClipNoteS &notes)
{
  return_unless (dynamic_cast<te::MidiClip*> (clip_.get()));
  ClipNoteS batch = all_notes();
  // Mark all for deletion.
  for (auto &note : batch)
    note.length_beats = 0;
  for (auto note : notes)
    {
      note.id = 0;
      batch.push_back (note);
    }
  change_batch (batch, "Set All Notes");
}

ClipNoteS
ClipImpl::all_notes () const
{
  ClipNoteS notes;
  auto mclip = dynamic_cast<te::MidiClip*> (clip_.get());
  return_unless (mclip, notes);
  state_listener_->prune_notes();
  for (auto note : mclip->getSequence().getNotes())
    notes.push_back (state_listener_->describe_note (*note));
  return notes;
}

double
ClipImpl::start_beat () const
{
  return clip_.get() ? clip_->getStartBeat().inBeats() : 0;
}

void
ClipImpl::start_beat (double)
{
  warning ("Clip.start_beat is read-only; assign Clip.position");
}

double
ClipImpl::end_beat () const
{
  return clip_.get() ? clip_->getEndBeat().inBeats() : 0;
}

void
ClipImpl::end_beat (double)
{
  warning ("Clip.end_beat is read-only; assign Clip.position");
}

bool
ClipImpl::muted () const
{
  return_unless (clip_.get(), false);
  return clip_->isMuted();
}

void
ClipImpl::muted (bool muted)
{
  return_unless (clip_.get());
  auto &um = clip_->edit.getUndoManager();
  um.beginNewTransaction ("Set Clip Muted");
  clip_->setMuted (muted);
}

float
ClipImpl::volume () const
{
  return_unless (clip_.get(), 0.0);
  auto mclip = dynamic_cast<te::MidiClip*> (clip_.get());
  if (mclip)
    return mclip->getVolumeDb();
  auto aclip = dynamic_cast<te::AudioClipBase*> (clip_.get());
  if (aclip)
    return aclip->getGainDB();
  return 0.0;
}

void
ClipImpl::volume (float db)
{
  return_unless (clip_.get());
  auto &um = clip_->edit.getUndoManager();
  um.beginNewTransaction ("Set Clip Volume");
  auto mclip = dynamic_cast<te::MidiClip*> (clip_.get());
  if (mclip)
    mclip->setVolumeDb (db);
  else
    {
      auto aclip = dynamic_cast<te::AudioClipBase*> (clip_.get());
      if (aclip)
        aclip->setGainDB (db);
    }
}

float
ClipImpl::pan () const
{
  return_unless (clip_.get(), 0.0);
  auto aclip = dynamic_cast<te::AudioClipBase*> (clip_.get());
  if (aclip)
    return aclip->getPan();
  return 0.0;
}

void
ClipImpl::pan (float panval)
{
  auto aclip = dynamic_cast<te::AudioClipBase*> (clip_.get());
  return_unless (aclip); // MIDI clips have no pan
  clip_->edit.getUndoManager().beginNewTransaction ("Set Clip Pan");
  aclip->setPan (panval);
}

StringS
ClipImpl::read_only_properties () const
{
  StringS props = { "end_beat", "start_beat" };
  if (!dynamic_cast<te::AudioClipBase*> (clip_.get()))
    props.push_back ("pan");
  if (!dynamic_cast<te::MidiClip*> (clip_.get()))
    props.push_back ("all_notes");
  return props;
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

ClipNoteS
ClipImpl::change_batch (const ClipNoteS &batch, const String &undogroup)
{
  return_unless (clip_.get(), {});
  auto mclip = dynamic_cast<te::MidiClip*> (clip_.get());
  assert_return (mclip, {});
  auto &sequence = mclip->getSequence();
  auto &um = clip_->edit.getUndoManager();
  // Group deletions, changes and insertions into one undo step.
  um.beginNewTransaction (juce::String (undogroup.empty() ? "Change Notes" : undogroup));
  ClipNoteS changed (batch.size());
  state_listener_->prune_notes();
  // Delete existing notes before modifications and insertions.
  for (size_t i = 0; i < batch.size(); i++)
    {
      const auto &note = batch[i];
      if (note.id > 0 && note.length_beats == 0)
        if (auto n = state_listener_->find_note (sequence, note.id))
          {
            changed[i] = note;
            sequence.removeNote (*n, &um);
          }
    }
  // Modify existing notes.
  for (size_t i = 0; i < batch.size(); i++)
    {
      const auto &note = batch[i];
      if (note.id > 0 && note.length_beats != 0)
        if (auto n = state_listener_->find_note (sequence, note.id))
          {
            n->setStartAndLength (tracktion::BeatPosition::fromBeats (note.start_beat),
                                 tracktion::BeatDuration::fromBeats (note.length_beats), &um);
            n->setNoteNumber (note.note_number, &um);
            n->setVelocity (note.velocity, &um);
            n->setColour (note.colour, &um);
            n->setMute (note.mute, &um);
            changed[i] = state_listener_->describe_note (*n);
          }
    }
  // Insert new notes.
  for (size_t i = 0; i < batch.size(); i++)
    {
      const auto &note = batch[i];
      if (note.id <= 0 && note.length_beats != 0)
        if (auto n = sequence.addNote (note.note_number, tracktion::BeatPosition::fromBeats (note.start_beat),
                                      tracktion::BeatDuration::fromBeats (note.length_beats), note.velocity, note.colour, &um))
          {
            n->setMute (note.mute, &um);
            changed[i] = state_listener_->describe_note (*n);
          }
    }
  emit_notify ("notes");
  emit_notify ("all_notes");
  return changed;
}

} // Ase
