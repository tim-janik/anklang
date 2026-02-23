// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <ase/project.hh>
#include <ase/testing.hh>

namespace { // Anon
using namespace Ase;

static void
project_creation()
{
  // Create Project
  ProjectImplP project = ProjectImpl::create ("TestProject");
  TASSERT (project);
  project->_activate();

  // Verify basic properties (mapping to te::Edit)
  // Verify basic properties (mapping to te::Edit)
  project->bpm.set (130.0);
  TASSERT (std::abs (project->bpm.get() - 130.0) < 0.001);
  TASSERT (project->name() == "TestProject");

  // Clean up
  project->_deactivate();
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
project_master_volume()
{
  ProjectImplP project = ProjectImpl::create ("VolumeTest");
  TASSERT (project);
  project->_activate();

  // Test initial master volume (should be around 0dB)
  double initial_vol = project->get_master_volume();
  TASSERT (initial_vol >= -100.0 && initial_vol <= 20.0);

  // Test setting master volume
  project->set_master_volume (-6.0);
  TASSERT (std::abs (project->get_master_volume() - (-6.0)) < 0.01);

  project->set_master_volume (0.0);
  TASSERT (std::abs (project->get_master_volume()) < 0.01);

  project->set_master_volume (3.5);
  TASSERT (std::abs (project->get_master_volume() - 3.5) < 0.01);

  project->_deactivate();
  project->discard();
}
TEST_ADD (project_master_volume);

static void
project_length()
{
  ProjectImplP project = ProjectImpl::create ("LengthTest");
  TASSERT (project);
  project->_activate();

  double len = project->get_length();
  TASSERT (len >= 0.0);

  project->_deactivate();
  project->discard();
}
TEST_ADD (project_length);

static void
project_track_management()
{
  ProjectImplP project = ProjectImpl::create ("TrackTest");
  TASSERT (project);
  project->_activate();

  TrackS tracks = project->all_tracks();
  const size_t initial_count = tracks.size();
  TASSERT (initial_count >= 1);

  TrackP new_track = project->create_track();
  TASSERT (new_track);

  tracks = project->all_tracks();
  TASSERT (tracks.size() == initial_count + 1);

  project->_deactivate();
  project->discard();
}
TEST_ADD (project_track_management);

static void
project_undo_redo()
{
  ProjectImplP project = ProjectImpl::create ("UndoTest");
  TASSERT (project);
  project->_activate();

  // Initially, no undo/redo should be available
  TASSERT (!project->can_undo());
  TASSERT (!project->can_redo());

  // Perform an undoable operation using undo_scope
  {
    auto scope = project->undo_scope ("Change BPM");
    project->bpm.set (140.0);
  }

  // Now undo should be available
  TASSERT (project->can_undo());
  TASSERT (!project->can_redo());

  // Perform undo
  project->undo();
  TASSERT (!project->can_undo());
  TASSERT (project->can_redo());

  // Perform redo
  project->redo();
  TASSERT (project->can_undo());
  TASSERT (!project->can_redo());

  project->_deactivate();
  project->discard();
}
TEST_ADD (project_undo_redo);

static void
project_playback_state()
{
  ProjectImplP project = ProjectImpl::create ("PlaybackTest");
  TASSERT (project);
  project->_activate();

  // Initially not playing
  TASSERT (!project->is_playing());

  project->_deactivate();
  project->discard();
}
TEST_ADD (project_playback_state);

static void
track_mute_solo()
{
  ProjectImplP project = ProjectImpl::create ("TrackMuteSoloTest");
  TASSERT (project);
  project->_activate();

  TrackP track = project->create_track();
  TASSERT (track);

  // Test initial mute/solo state
  TASSERT (!track->is_muted());
  TASSERT (!track->is_solo());

  // Test mute
  track->set_muted (true);
  TASSERT (track->is_muted());
  track->set_muted (false);
  TASSERT (!track->is_muted());

  // Test solo
  track->set_solo (true);
  TASSERT (track->is_solo());
  track->set_solo (false);
  TASSERT (!track->is_solo());

  project->_deactivate();
  project->discard();
}
TEST_ADD (track_mute_solo);

static void
track_volume_pan()
{
  ProjectImplP project = ProjectImpl::create ("TrackVolumePanTest");
  TASSERT (project);
  project->_activate();

  TrackP track = project->create_track();
  TASSERT (track);

  // Test initial volume
  double initial_vol = track->get_volume();
  TASSERT (initial_vol >= -100.0 && initial_vol <= 20.0);

  // Test setting volume
  track->set_volume (-6.0);
  TASSERT (std::abs (track->get_volume() - (-6.0)) < 0.01);

  track->set_volume (0.0);
  TASSERT (std::abs (track->get_volume()) < 0.01);

  // Test pan
  double initial_pan = track->get_pan();
  TASSERT (initial_pan >= -1.0 && initial_pan <= 1.0);

  track->set_pan (0.5);
  TASSERT (std::abs (track->get_pan() - 0.5) < 0.01);

  track->set_pan (-0.5);
  TASSERT (std::abs (track->get_pan() - (-0.5)) < 0.01);

  project->_deactivate();
  project->discard();
}
TEST_ADD (track_volume_pan);

static void
track_name()
{
  ProjectImplP project = ProjectImpl::create ("TrackNameTest");
  TASSERT (project);
  project->_activate();

  TrackP track = project->create_track();
  TASSERT (track);

  // Set and get track name
  track->set_name ("MyTrack");
  TASSERT (track->name() == "MyTrack");

  track->set_name ("AnotherName");
  TASSERT (track->name() == "AnotherName");

  project->_deactivate();
  project->discard();
}
TEST_ADD (track_name);

} // Anon
