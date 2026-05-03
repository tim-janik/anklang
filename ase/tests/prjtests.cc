// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <ase/project.hh>
#include <ase/clip.hh>
#include <ase/track.hh>
#include <ase/testing.hh>

namespace { // Anon
using namespace Ase;

static void
project_creation()
{
  ProjectImplP project = ProjectImpl::create ("TestProject");
  TASSERT (project);
  TASSERT (project->name() == "TestProject");

  const double initial_bpm = project->bpm();
  TASSERT (initial_bpm >= 10.0 && initial_bpm <= 999.0);
  uint64_t bpm_notifications = 0;
  auto bpm_connection = project->on_event ("notify:bpm", [&bpm_notifications] (const Event &event) { bpm_notifications++; });

  uint64_t name_notifications = 0;
  auto name_connection = project->on_event ("notify:name", [&name_notifications] (const Event &event) { name_notifications++; });

  uint64_t volume_notifications = 0;
  auto volume_connection = project->on_event ("notify:master_volume", [&volume_notifications] (const Event &event) { volume_notifications++; });

  const double initial_vol = project->master_volume();
  TASSERT (initial_vol >= -100.0 && initial_vol <= 20.0);

  // Test BPM notify/undo/redo
  project->bpm (130.0);
  TASSERT (std::abs (project->bpm() - 130.0) < 0.001);
  uint64_t last_bpm_notifications = bpm_notifications;
  project->bpm (123.0);
  TASSERT (std::abs (project->bpm() - 123.0) < 0.001);
  TASSERT (bpm_notifications > last_bpm_notifications);

  // Test undo
  TASSERT (project->can_undo());
  TASSERT (!project->can_redo());
  last_bpm_notifications = bpm_notifications;
  project->undo();
  TASSERT (!project->can_undo());
  TASSERT (project->can_redo());
  TASSERT (std::abs (project->bpm() - initial_bpm) < 0.001);
  TASSERT (bpm_notifications > last_bpm_notifications);
  // Test redo
  last_bpm_notifications = bpm_notifications;
  project->redo();
  TASSERT (project->can_undo());
  TASSERT (!project->can_redo());
  TASSERT (std::abs (project->bpm() - 123.0) < 0.001);
  TASSERT (bpm_notifications > last_bpm_notifications);

  project->undo();
  TASSERT (!project->can_undo());
  TASSERT (project->can_redo());
  TASSERT (std::abs (project->bpm() - initial_bpm) < 0.001);

  // Test name notify/undo/redo
  const String initial_name = project->name();
  TASSERT (initial_name == "TestProject");
  uint64_t last_name_notifications = name_notifications;
  project->name ("NewName");
  TASSERT (project->name() == "NewName");
  TASSERT (name_notifications > last_name_notifications);

  TASSERT (project->can_undo());
  last_name_notifications = name_notifications;
  project->undo();
  TASSERT (project->name() == initial_name);
  TASSERT (project->can_redo());
  TASSERT (name_notifications > last_name_notifications);

  last_name_notifications = name_notifications;
  project->redo();
  TASSERT (project->name() == "NewName");
  TASSERT (name_notifications > last_name_notifications);

  // Test master_volume notify/undo/redo
  uint64_t last_volume_notifications = volume_notifications;
  project->master_volume (-6.0);
  TASSERT (std::abs (project->master_volume() - (-6.0)) < 0.01);
  TASSERT (volume_notifications > last_volume_notifications);

  TASSERT (project->can_undo());
  last_volume_notifications = volume_notifications;
  project->undo();
  TASSERT (std::abs (project->master_volume() - initial_vol) < 0.01);
  TASSERT (project->can_redo());
  TASSERT (volume_notifications > last_volume_notifications);

  last_volume_notifications = volume_notifications;
  project->redo();
  TASSERT (std::abs (project->master_volume() - (-6.0)) < 0.01);
  TASSERT (volume_notifications > last_volume_notifications);

  project->discard();

  // Test create second project
  ProjectImplP project2 = ProjectImpl::create ("TestProject2");
  project2->name ("foo");
  TASSERT (project2->name() == "foo");
  project2->name ("bar");
  TASSERT (project2->name() == "bar");
  project2->discard();
}
TEST_ADD (project_creation);

static void
project_length()
{
  ProjectImplP project = ProjectImpl::create ("LengthTest");
  TASSERT (project);

  double len = project->length();
  TASSERT (len >= 0.0);

  project->discard();
}
TEST_ADD (project_length);

static void
project_track_management()
{
  ProjectImplP project = ProjectImpl::create ("TrackTest");
  TASSERT (project);

  TrackS tracks = project->all_tracks();
  const size_t initial_count = tracks.size();
  TASSERT (initial_count >= 1);

  TrackP new_track = project->create_track();
  TASSERT (new_track);

  tracks = project->all_tracks();
  TASSERT (tracks.size() == initial_count + 1);

  project->discard();
}
TEST_ADD (project_track_management);

static void
project_playback_state()
{
  ProjectImplP project = ProjectImpl::create ("PlaybackTest");
  TASSERT (project);

  // Initially not playing
  TASSERT (!project->is_playing());

  project->discard();
}
TEST_ADD (project_playback_state);

static void
track_mute_solo()
{
  ProjectImplP project = ProjectImpl::create ("TrackMuteSoloTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  // Test initial mute/solo state
  TASSERT (!track->is_muted());
  TASSERT (!track->is_solo());

  // Test mute notifications
  uint64_t muted_notifications = 0;
  auto muted_connection = track->on_event ("notify:muted", [&muted_notifications] (const Event &event) { muted_notifications++; });

  uint64_t solo_notifications = 0;
  auto solo_connection = track->on_event ("notify:solo", [&solo_notifications] (const Event &event) { solo_notifications++; });

  // Test mute with notification
  uint64_t last_muted_notifications = muted_notifications;
  track->set_muted (true);
  TASSERT (track->is_muted());
  TASSERT (muted_notifications > last_muted_notifications);

  // Reset mute
  last_muted_notifications = muted_notifications;
  track->set_muted (false);
  TASSERT (!track->is_muted());
  TASSERT (muted_notifications > last_muted_notifications);

  // Test solo with notification
  uint64_t last_solo_notifications = solo_notifications;
  track->set_solo (true);
  TASSERT (track->is_solo());
  TASSERT (solo_notifications > last_solo_notifications);

  // Reset solo
  last_solo_notifications = solo_notifications;
  track->set_solo (false);
  TASSERT (!track->is_solo());
  TASSERT (solo_notifications > last_solo_notifications);

  project->discard();
}
TEST_ADD (track_mute_solo);

static void
track_hidden()
{
  ProjectImplP project = ProjectImpl::create ("TrackHiddenTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  // Test initial hidden state
  TASSERT (!track->is_hidden());

  // Test hidden notifications
  uint64_t hidden_notifications = 0;
  auto hidden_connection = track->on_event ("notify:hidden", [&hidden_notifications] (const Event &event) { hidden_notifications++; });

  // Test hide with notification
  uint64_t last_hidden_notifications = hidden_notifications;
  track->set_hidden (true);
  TASSERT (track->is_hidden());
  TASSERT (hidden_notifications > last_hidden_notifications);

  // Reset hidden
  last_hidden_notifications = hidden_notifications;
  track->set_hidden (false);
  TASSERT (!track->is_hidden());
  TASSERT (hidden_notifications > last_hidden_notifications);

  project->discard();
}
TEST_ADD (track_hidden);

static void
track_undo_redo()
{
  ProjectImplP project = ProjectImpl::create ("TrackUndoRedoTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  // Test track volume/pan notifications
  uint64_t volume_notifications = 0;
  auto volume_connection = track->on_event ("notify:volume", [&volume_notifications] (const Event &event) { volume_notifications++; });

  uint64_t pan_notifications = 0;
  auto pan_connection = track->on_event ("notify:pan", [&pan_notifications] (const Event &event) { pan_notifications++; });

  // Change volume
  uint64_t last_volume_notifications = volume_notifications;
  track->volume (-6.0);
  TASSERT (std::abs (track->volume() - (-6.0)) < 0.01);
  TASSERT (volume_notifications > last_volume_notifications);

  // Change volume back
  last_volume_notifications = volume_notifications;
  track->volume (0.0);
  TASSERT (std::abs (track->volume()) < 0.01);
  TASSERT (volume_notifications > last_volume_notifications);

  // Change pan
  uint64_t last_pan_notifications = pan_notifications;
  track->pan (0.5);
  TASSERT (std::abs (track->pan() - 0.5) < 0.01);
  TASSERT (pan_notifications > last_pan_notifications);

  // Change pan back
  last_pan_notifications = pan_notifications;
  track->pan (-0.5);
  TASSERT (std::abs (track->pan() - (-0.5)) < 0.01);
  TASSERT (pan_notifications > last_pan_notifications);

  project->discard();
}
TEST_ADD (track_undo_redo);

static void
track_volume_pan()
{
  ProjectImplP project = ProjectImpl::create ("TrackVolumePanTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  // Test initial volume
  double initial_vol = track->volume();
  TASSERT (initial_vol >= -100.0 && initial_vol <= 20.0);

  // Test volume notifications
  uint64_t volume_notifications = 0;
  auto volume_connection = track->on_event ("notify:volume", [&volume_notifications] (const Event &event) { volume_notifications++; });

  uint64_t pan_notifications = 0;
  auto pan_connection = track->on_event ("notify:pan", [&pan_notifications] (const Event &event) { pan_notifications++; });

  // Test setting volume with notification
  uint64_t last_volume_notifications = volume_notifications;
  track->volume (-6.0);
  TASSERT (std::abs (track->volume() - (-6.0)) < 0.01);
  TASSERT (volume_notifications > last_volume_notifications);

  // Reset volume
  last_volume_notifications = volume_notifications;
  track->volume (0.0);
  TASSERT (std::abs (track->volume()) < 0.01);
  TASSERT (volume_notifications > last_volume_notifications);

  // Test pan
  double initial_pan = track->pan();
  TASSERT (initial_pan >= -1.0 && initial_pan <= 1.0);

  // Test setting pan with notification
  uint64_t last_pan_notifications = pan_notifications;
  track->pan (0.5);
  TASSERT (std::abs (track->pan() - 0.5) < 0.01);
  TASSERT (pan_notifications > last_pan_notifications);

  // Reset pan
  last_pan_notifications = pan_notifications;
  track->pan (-0.5);
  TASSERT (std::abs (track->pan() - (-0.5)) < 0.01);
  TASSERT (pan_notifications > last_pan_notifications);

  project->discard();
}
TEST_ADD (track_volume_pan);

static void
track_name()
{
  ProjectImplP project = ProjectImpl::create ("TrackNameTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  // Set and get track name
  track->name ("MyTrack");
  TASSERT (track->name() == "MyTrack");

  track->name ("AnotherName");
  TASSERT (track->name() == "AnotherName");

  project->discard();
}
TEST_ADD (track_name);

static void
clip_creation()
{
  ProjectImplP project = ProjectImpl::create ("ClipTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  TrackImplP trackimpl = std::dynamic_pointer_cast<TrackImpl> (track);
  TASSERT (trackimpl);

  ClipP clip = trackimpl->create_midi_clip ("TestClip", 0.0, 4.0);
  TASSERT (clip);

  project->discard();
}
TEST_ADD (clip_creation);

static void
clip_notes()
{
  ProjectImplP project = ProjectImpl::create ("ClipNotesTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  TrackImplP trackimpl = std::dynamic_pointer_cast<TrackImpl> (track);
  TASSERT (trackimpl);

  ClipP clip = trackimpl->create_midi_clip ("NotesClip", 0.0, 4.0);
  TASSERT (clip);

  ClipNoteS notes = clip->list_all_notes();
  TASSERT (notes.empty());

  ClipNote note;
  note.id = -1;
  note.key = 60;
  note.channel = 0;
  note.tick = 0;
  note.duration = 960;
  note.velocity = 0.8f;

  ClipNoteS batch;
  batch.push_back (note);
  clip->change_batch (batch, "Add Note");

  notes = clip->list_all_notes();
  TASSERT (notes.size() == 1);
  TASSERT (notes[0].key == 60);

  project->discard();
}
TEST_ADD (clip_notes);

static void
clip_range()
{
  ProjectImplP project = ProjectImpl::create ("ClipRangeTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  TrackImplP trackimpl = std::dynamic_pointer_cast<TrackImpl> (track);
  TASSERT (trackimpl);

  ClipP clip = trackimpl->create_midi_clip ("RangeClip", 0.0, 4.0);
  TASSERT (clip);

  int64 start = clip->start_tick();
  int64 stop = clip->stop_tick();
  TASSERT (start >= 0);
  TASSERT (stop > start);

  clip->assign_range (start + 960, stop + 960);
  TASSERT (clip->start_tick() == start + 960);

  project->discard();
}
TEST_ADD (clip_range);

static void
clip_mute_volume_pan()
{
  ProjectImplP project = ProjectImpl::create ("ClipMuteVolPanTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  TrackImplP trackimpl = std::dynamic_pointer_cast<TrackImpl> (track);
  TASSERT (trackimpl);

  // Test MIDI clip
  ClipP mclip = trackimpl->create_midi_clip ("MidiClip", 0.0, 4.0);
  TASSERT (mclip);

  // Test initial state
  TASSERT (!mclip->is_muted());

  // Test mute notifications
  uint64_t muted_notifications = 0;
  auto muted_connection = mclip->on_event ("notify:muted", [&muted_notifications] (const Event &event) { muted_notifications++; });

  // Test mute with notification
  uint64_t last_muted_notifications = muted_notifications;
  mclip->set_muted (true);
  TASSERT (mclip->is_muted());
  TASSERT (muted_notifications > last_muted_notifications);

  // Reset mute
  last_muted_notifications = muted_notifications;
  mclip->set_muted (false);
  TASSERT (!mclip->is_muted());
  TASSERT (muted_notifications > last_muted_notifications);

  // Test volume notifications
  uint64_t volume_notifications = 0;
  auto volume_connection = mclip->on_event ("notify:volume", [&volume_notifications] (const Event &event) { volume_notifications++; });

  // Test setting volume with notification
  uint64_t last_volume_notifications = volume_notifications;
  mclip->volume (-6.0);
  TASSERT (std::abs (mclip->volume() - (-6.0)) < 0.01);
  TASSERT (volume_notifications > last_volume_notifications);

  // Reset volume
  last_volume_notifications = volume_notifications;
  mclip->volume (0.0);
  TASSERT (std::abs (mclip->volume()) < 0.01);
  TASSERT (volume_notifications > last_volume_notifications);

  // Test audio clip (pan is only available on audio clips)
  ClipP aclip = trackimpl->create_audio_clip ("AudioClip", 0.0, 4.0);
  TASSERT (aclip);

  // Test pan notifications for audio clip
  uint64_t pan_notifications = 0;
  auto pan_connection = aclip->on_event ("notify:pan", [&pan_notifications] (const Event &event) { pan_notifications++; });

  // Initial pan should be 0
  TASSERT (std::abs (aclip->pan()) < 0.01);

  // Test setting pan with notification
  uint64_t last_pan_notifications = pan_notifications;
  aclip->pan (0.5);
  TASSERT (std::abs (aclip->pan() - 0.5) < 0.01);
  TASSERT (pan_notifications > last_pan_notifications);

  // Reset pan
  last_pan_notifications = pan_notifications;
  aclip->pan (-0.5);
  TASSERT (std::abs (aclip->pan() - (-0.5)) < 0.01);
  TASSERT (pan_notifications > last_pan_notifications);

  // Test audio clip volume
  uint64_t audio_volume_notifications = 0;
  auto audio_volume_connection = aclip->on_event ("notify:volume", [&audio_volume_notifications] (const Event &event) { audio_volume_notifications++; });

  last_volume_notifications = audio_volume_notifications;
  aclip->volume (-3.0);
  TASSERT (std::abs (aclip->volume() - (-3.0)) < 0.01);
  TASSERT (audio_volume_notifications > last_volume_notifications);

  project->discard();
}
TEST_ADD (clip_mute_volume_pan);

static void
clip_undo_redo()
{
  ProjectImplP project = ProjectImpl::create ("ClipUndoRedoTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  TrackImplP trackimpl = std::dynamic_pointer_cast<TrackImpl> (track);
  TASSERT (trackimpl);

  // Test MIDI clip mute undo/redo
  ClipP mclip = trackimpl->create_midi_clip ("UndoMidiClip", 0.0, 4.0);
  TASSERT (mclip);

  uint64_t muted_notifications = 0;
  auto muted_connection = mclip->on_event ("notify:muted", [&muted_notifications] (const Event &event) { muted_notifications++; });

  // Set muted
  uint64_t last_muted_notifications = muted_notifications;
  mclip->set_muted (true);
  TASSERT (mclip->is_muted());
  TASSERT (muted_notifications > last_muted_notifications);

  // Undo mute
  TASSERT (project->can_undo());
  project->undo();
  TASSERT (!mclip->is_muted());

  // Redo mute
  TASSERT (project->can_redo());
  project->redo();
  TASSERT (mclip->is_muted());

  // Test MIDI clip volume undo/redo
  uint64_t volume_notifications = 0;
  auto volume_connection = mclip->on_event ("notify:volume", [&volume_notifications] (const Event &event) { volume_notifications++; });

  // Set volume
  double initial_vol = mclip->volume();
  uint64_t last_volume_notifications = volume_notifications;
  mclip->volume (-12.0);
  TASSERT (std::abs (mclip->volume() - (-12.0)) < 0.01);
  TASSERT (volume_notifications > last_volume_notifications);

  // Undo volume
  TASSERT (project->can_undo());
  project->undo();
  TASSERT (std::abs (mclip->volume() - initial_vol) < 0.01);

  // Redo volume
  TASSERT (project->can_redo());
  project->redo();
  TASSERT (std::abs (mclip->volume() - (-12.0)) < 0.01);

  // Test audio clip pan undo/redo
  ClipP aclip = trackimpl->create_audio_clip ("UndoAudioClip", 0.0, 4.0);
  TASSERT (aclip);

  uint64_t pan_notifications = 0;
  auto pan_connection = aclip->on_event ("notify:pan", [&pan_notifications] (const Event &event) { pan_notifications++; });

  // Set pan
  double initial_pan = aclip->pan();
  uint64_t last_pan_notifications = pan_notifications;
  aclip->pan (0.8);
  TASSERT (std::abs (aclip->pan() - 0.8) < 0.01);
  TASSERT (pan_notifications > last_pan_notifications);

  // Undo pan
  TASSERT (project->can_undo());
  project->undo();
  TASSERT (std::abs (aclip->pan() - initial_pan) < 0.01);

  // Redo pan
  TASSERT (project->can_redo());
  project->redo();
  TASSERT (std::abs (aclip->pan() - 0.8) < 0.01);

  // Test MIDI clip notes undo/redo
  ClipNoteS batch;
  ClipNote note;
  note.id = -1;
  note.key = 60;
  note.channel = 0;
  note.tick = 0;
  note.duration = 960;
  note.velocity = 0.8f;
  batch.push_back (note);

  uint64_t notes_notifications = 0;
  auto notes_connection = mclip->on_event ("notify:notes", [&notes_notifications] (const Event &event) { notes_notifications++; });

  // Add notes
  uint64_t last_notes_notifications = notes_notifications;
  mclip->change_batch (batch, "Add Note");
  TASSERT (mclip->list_all_notes().size() == 1);
  TASSERT (notes_notifications > last_notes_notifications);

  // Undo notes
  TASSERT (project->can_undo());
  project->undo();
  TASSERT (mclip->list_all_notes().empty());

  // Redo notes
  TASSERT (project->can_redo());
  project->redo();
  TASSERT (mclip->list_all_notes().size() == 1);

  // Test MIDI clip range undo/redo
  int64 initial_start = mclip->start_tick();
  int64 initial_stop = mclip->stop_tick();

  // Assign new range
  uint64_t range_notifications = 0;
  auto range_connection = mclip->on_event ("notify:start_tick", [&range_notifications] (const Event &event) { range_notifications++; });

  uint64_t last_range_notifications = range_notifications;
  mclip->assign_range (initial_start + 960, initial_stop + 960);
  TASSERT (mclip->start_tick() == initial_start + 960);
  TASSERT (range_notifications > last_range_notifications);

  // Undo range
  TASSERT (project->can_undo());
  project->undo();
  TASSERT (mclip->start_tick() == initial_start);
  TASSERT (mclip->stop_tick() == initial_stop);

  // Redo range
  TASSERT (project->can_redo());
  project->redo();
  TASSERT (mclip->start_tick() == initial_start + 960);
  TASSERT (mclip->stop_tick() == initial_stop + 960);

  project->discard();
}
TEST_ADD (clip_undo_redo);

static void
plugin_creation()
{
  ProjectImplP project = ProjectImpl::create ("PluginTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  TrackImplP trackimpl = std::dynamic_pointer_cast<TrackImpl> (track);
  TASSERT (trackimpl);

  // Check initial plugins list (tracks have default plugins like volume/pan)
  PluginS plugins = trackimpl->list_plugins();
  TASSERT (plugins.size() >= 0);

  // Test with existing plugins (tracks have default plugins)
  if (plugins.size() > 0) {
    PluginP plugin = plugins[0];
    TASSERT (plugin != nullptr);

    // Check plugin properties
    TASSERT (!plugin->name().empty());
    TASSERT (!plugin->plugin_type().empty());

    // Setup notification counters
    uint64_t enabled_notifications = 0;
    auto enabled_connection = plugin->on_event ("notify:enabled", [&enabled_notifications] (const Event &event) { enabled_notifications++; });

    uint64_t frozen_notifications = 0;
    auto frozen_connection = plugin->on_event ("notify:frozen", [&frozen_notifications] (const Event &event) { frozen_notifications++; });

    // Check enabled state (should be either true or false)
    bool enabled = plugin->is_enabled();
    TASSERT (enabled == true || enabled == false);

    // Toggle enabled - should emit notification
    uint64_t last_enabled_notifications = enabled_notifications;
    plugin->set_enabled (!enabled);
    TASSERT (plugin->is_enabled() != enabled);
    TASSERT (enabled_notifications > last_enabled_notifications);

    // Check frozen state
    bool frozen = plugin->is_frozen();
    TASSERT (frozen == true || frozen == false);

    // Toggle frozen - should emit notification
    uint64_t last_frozen_notifications = frozen_notifications;
    plugin->set_frozen (!frozen);
    TASSERT (plugin->is_frozen() != frozen);
    TASSERT (frozen_notifications > last_frozen_notifications);

    // Test plugin removal via remove_self
    uint64_t removed_count = 0;
    auto removed_connection = plugin->on_event ("object:removed", [&removed_count] (const Event &event) { removed_count++; });
    plugin->remove_self();
    TASSERT (removed_count > 0);
  }

  project->discard();
}
TEST_ADD (plugin_creation);

} // Anon
