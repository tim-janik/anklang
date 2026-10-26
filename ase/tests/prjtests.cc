// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <ase/project.hh>
#include <ase/clip.hh>
#include <ase/track.hh>
#include <ase/path.hh>
#include <ase/platform.hh>
#include <ase/storage.hh>
#include <ase/testing.hh>
#include <unistd.h>
#include <glib.h>

namespace { // Anon
using namespace Ase;

struct TestEnvironment {
  String key, previous;
  bool existed;
  TestEnvironment (const String &name, const String &value) :
    key (name), previous (getenv (name.c_str()) ?: ""), existed (getenv (name.c_str()))
  {
    TASSERT (setenv (key.c_str(), value.c_str(), 1) == 0);
  }
  ~TestEnvironment()
  {
    TASSERT ((existed ? setenv (key.c_str(), previous.c_str(), 1) : unsetenv (key.c_str())) == 0);
  }
};

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

  // Test BPM notify/undo/redo
  project->bpm (130.0);
  TASSERT (std::abs (project->bpm() - 130.0) < 0.001);
  uint64_t last_bpm_notifications = bpm_notifications;
  project->bpm (123.5);
  TASSERT (std::abs (project->bpm() - 123.5) < 0.001);
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
  TASSERT (std::abs (project->bpm() - 123.5) < 0.001);
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
project_dir_lifetime()
{
  ProjectImplP project = ProjectImpl::create ("ProjectDir");
  TASSERT (project->project_dir (false).empty());
  String dir = project->project_dir (true);
  if (dir.empty())
    {
      TASSERT (project->project_dir (false).empty());
      dir = project->project_dir (true);
    }
  TASSERT (Path::check (Path::join (dir, ".anklang.project"), "f"));
  TASSERT (project->project_dir (false) == dir);
  const String snapshot = Path::join (dir, "autosave/snapshot");
  TASSERT (Path::stringwrite (snapshot, "unsaved state", true));
  project->discard();
  project.reset();
  TASSERT (Path::stringread (snapshot) == "unsaved state");
  TASSERT (Path::check (Path::join (dir, ".anklang.project"), "f"));
}
TEST_ADD (project_dir_lifetime);

static void
project_dir_exit_child()
{
  ProjectImplP project = ProjectImpl::create ("Exit");
  const String dir = project->project_dir (true);
  TASSERT (!dir.empty());
  TASSERT (Path::stringwrite (Path::join (dir, "autosave/snapshot"), "exit state", true));
  project->discard();
}
TEST_ADD (project_dir_exit_child);

static void
project_dir_exit()
{
  const String root = anklang_cachedir_create();
  TASSERT (!root.empty());
  const String projects = Path::join (root, "projects");
  StringS args = { "env", "ANKLANG_PROJECTS_DIR=" + projects, executable_path(),
                   "--norc", "--no-devices", "--test=project_dir_exit_child" };
  std::vector<char*> argv;
  for (String &arg : args)
    argv.push_back (arg.data());
  argv.push_back (nullptr);
  int status = -1;
  TASSERT (g_spawn_sync (nullptr, argv.data(), nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, nullptr, &status, nullptr));
  TASSERT (status == 0);
  TASSERT (Path::stringread (Path::join (projects, "Exit/autosave/snapshot")) == "exit state");
  anklang_cachedir_cleanup (root);
}
TEST_ADD (project_dir_exit);

static void
project_dir_roots()
{
  const String root = anklang_cachedir_create();
  TASSERT (!root.empty());
  PropertyP setting = Server::instance().access_preference ("anklang-projects-dir");
  TASSERT (setting);
  const Value previous = setting->value();
  {
    TestEnvironment projects ("ANKLANG_PROJECTS_DIR", Path::join (root, "env"));
    setting->value (Path::join (root, "setting"));
    const StringS expected = { "env", "setting" };
    for (size_t i = 0; i < expected.size(); i++)
      {
        ProjectImplP project = ProjectImpl::create ("Roots");
        TASSERT (project->project_dir (false).empty());
        const String dir = project->project_dir (true);
        TASSERT (dir == Path::join (root, expected[i], "Roots"));
        TASSERT (setenv ("ANKLANG_PROJECTS_DIR", "", 1) == 0);
        project->name ("Renamed");
        TASSERT (project->project_dir (true) == dir);
        project->discard();
      }
  }
  setting->value (previous);
  anklang_cachedir_cleanup (root);
}
TEST_ADD (project_dir_roots);

static void
project_dir_names()
{
  const String root = anklang_cachedir_create();
  TASSERT (!root.empty());
  TestEnvironment projects ("ANKLANG_PROJECTS_DIR", root);
  for (const String &suffix : StringS { "", "(1)", "(2)" })
    {
      ProjectImplP project = ProjectImpl::create ("Same Name");
      TASSERT (project->project_dir (true) == Path::join (root, "Same Name" + suffix));
      project->discard();
    }
  const String blocked = Path::join (root, "Blocked");
  TASSERT (Path::stringwrite (blocked, "keep this file"));
  ProjectImplP project = ProjectImpl::create ("Blocked");
  TASSERT (project->project_dir (true) == blocked + "(1)");
  TASSERT (Path::stringread (blocked) == "keep this file");
  project->discard();
  for (const String &name : StringS { "", ".", "..", "../escape", "a/b\\c" })
    {
      ProjectImplP project = ProjectImpl::create (name);
      TASSERT (Path::dircontains (root, project->project_dir (true)));
      project->discard();
    }
  anklang_cachedir_cleanup (root);
}
TEST_ADD (project_dir_names);

static void
project_tempo_values()
{
  ProjectP project = ProjectImpl::create ("TempoTest");
  TASSERT (project->bpm() == 120.0);
  TASSERT (project->numerator() == 4);
  TASSERT (project->denominator() == 4);

  project->bpm (1.0);
  TASSERT (project->bpm() == 20.0);
  project->bpm (2000.0);
  TASSERT (project->bpm() == 300.0);

  for (bool numerator : { true, false })
    {
      const auto get = [&] { return numerator ? project->numerator() : project->denominator(); };
      const auto set = [&] (int value) {
        if (numerator)
          project->numerator (value);
        else
          project->denominator (value);
      };
      uint notifications = 0;
      auto connection = project->on_event (numerator ? "notify:numerator" : "notify:denominator",
                                           [&] (const Event&) { notifications++; });
      const int initial = get(), value = numerator ? 7 : 8;
      project->ungroup_undo();
      set (value);
      TASSERT (get() == value);
      TASSERT (notifications > 0);
      TASSERT (project->can_undo());

      notifications = 0;
      set (value);
      TASSERT (notifications == 0);
      project->undo();
      TASSERT (get() == initial);
      TASSERT (notifications > 0);
      TASSERT (project->can_redo());

      notifications = 0;
      project->redo();
      TASSERT (get() == value);
      TASSERT (notifications > 0);
    }

  project->discard();
}
TEST_ADD (project_tempo_values);

static void
project_length()
{
  ProjectP project = ProjectImpl::create ("LengthTest");
  TrackP track = project->create_track();
  TCMP (project->length(), ==, 0);
  uint notifications = 0;
  auto connection = project->on_event ("notify:length", [&] (const Event&) { notifications++; });
  auto expect_length = [&] (double expected) {
    TCMP (project->length(), ==, expected);
    TASSERT (notifications > 0);
    notifications = 0;
  };
  auto check_change = [&] (auto change, double expected) {
    const double before = project->length();
    project->ungroup_undo();
    change();
    expect_length (expected);
    project->undo();
    expect_length (before);
    project->redo();
    expect_length (expected);
  };

  project->ungroup_undo();
  ClipP midi = track->create_midi_clip ("LengthClip", 1, 3);
  expect_length (4);
  check_change ([&] { project->bpm (60); }, 8);
  check_change ([&] { midi->position ({ { 2, 12 }, 0 }); }, 12);
  check_change ([&] { midi->remove_self(); }, 0);
  check_change ([&] { track->create_audio_clip ("LastClip", 20, 4); }, 24);
  check_change ([&] { track->remove_self(); }, 0);

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

  uint64_t track_notifications = 0;
  auto track_connection = project->on_event ("notify:all_tracks", [&track_notifications] (const Event &event) { track_notifications++; });
  new_track->remove_self();
  TASSERT (project->all_tracks().size() == initial_count);
  TASSERT (track_notifications > 0);

  project->discard();
}
TEST_ADD (project_track_management);

static void
track_outlives_project()
{
  TrackS tracks;
  // one project passes even without the fix, freed meters must get reused
  for (size_t i = 0; i < 40; i++)
    {
      auto project = ProjectImpl::create ("TrackLifetimeTest");
      tracks.push_back (project->create_track());
      TASSERT (tracks.back());
      project->discard();
    }
  for (const auto &track : tracks)
    TASSERT (track->name().empty());
}
TEST_ADD (track_outlives_project);

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
  TASSERT (!track->muted());
  TASSERT (!track->solo());

  // Test mute notifications
  uint64_t muted_notifications = 0;
  auto muted_connection = track->on_event ("notify:muted", [&muted_notifications] (const Event &event) { muted_notifications++; });

  uint64_t solo_notifications = 0;
  auto solo_connection = track->on_event ("notify:solo", [&solo_notifications] (const Event &event) { solo_notifications++; });

  // Test mute with notification
  uint64_t last_muted_notifications = muted_notifications;
  track->muted (true);
  TASSERT (track->muted());
  TASSERT (muted_notifications > last_muted_notifications);

  // Reset mute
  last_muted_notifications = muted_notifications;
  track->muted (false);
  TASSERT (!track->muted());
  TASSERT (muted_notifications > last_muted_notifications);

  // Test solo with notification
  uint64_t last_solo_notifications = solo_notifications;
  track->solo (true);
  TASSERT (track->solo());
  TASSERT (solo_notifications > last_solo_notifications);

  // Reset solo
  last_solo_notifications = solo_notifications;
  track->solo (false);
  TASSERT (!track->solo());
  TASSERT (solo_notifications > last_solo_notifications);

  project->discard();
}
TEST_ADD (track_mute_solo);

static void
track_control_classification()
{
  ProjectImplP project = ProjectImpl::create ("ControlTrackTest");
  TASSERT (project);

  // Fresh projects have the default control tracks plus one audio track
  TrackS tracks = project->all_tracks();
  bool have_master = false, have_control = false, have_editable = false;
  for (auto &t : tracks)
    {
      have_master |= t->is_master();
      have_control |= t->is_control_track();
      have_editable |= !t->is_control_track();
    }
  TASSERT (have_master);
  TASSERT (have_control);
  TASSERT (have_editable); // default audio track is editable

  // New tracks are editable, not control tracks
  TrackP new_track = project->create_track();
  TASSERT (new_track);
  TASSERT (!new_track->is_control_track());
  TASSERT (!new_track->is_master());

  project->discard();
}
TEST_ADD (track_control_classification);

static void
track_hidden()
{
  ProjectImplP project = ProjectImpl::create ("TrackHiddenTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  // Test initial hidden state
  TASSERT (!track->hidden());

  // Test hidden notifications
  uint64_t hidden_notifications = 0;
  auto hidden_connection = track->on_event ("notify:hidden", [&hidden_notifications] (const Event &event) { hidden_notifications++; });

  // Test hide with notification
  uint64_t last_hidden_notifications = hidden_notifications;
  project->ungroup_undo();
  track->hidden (true);
  TASSERT (track->hidden());
  TASSERT (hidden_notifications > last_hidden_notifications);

  TASSERT (project->can_undo());
  last_hidden_notifications = hidden_notifications;
  project->undo();
  TASSERT (!track->hidden());
  TASSERT (hidden_notifications > last_hidden_notifications);

  TASSERT (project->can_redo());
  last_hidden_notifications = hidden_notifications;
  project->redo();
  TASSERT (track->hidden());
  TASSERT (hidden_notifications > last_hidden_notifications);

  project->discard();
}
TEST_ADD (track_hidden);

static void
check_track_parameter_values (bool master)
{
  struct ParameterTest {
    const char *name;
    float (Track::*get)() const;
    void (Track::*set) (float);
    float value;
  };
  const std::array<ParameterTest,3> parameters = {{
    { "volume_db", &Track::volume_db, &Track::volume_db, -6.0f },
    { "slider_pos", &Track::slider_pos, &Track::slider_pos, 0.25f },
    { "pan", &Track::pan, &Track::pan, 0.5f },
  }};
  for (size_t property = 0; property < parameters.size(); property++) {
    ProjectP project = ProjectImpl::create ("TrackParameterTest");
    TASSERT (project);
    TrackP track = master ? project->master_track() : project->create_track();
    TrackP other_track = master ? project->create_track() : project->master_track();
    TASSERT (track);
    TASSERT (other_track);
    using Values = std::array<float,3>;
    auto read_values = [&parameters] (const TrackP &t) {
      Values values;
      for (size_t i = 0; i < parameters.size(); i++)
        values[i] = (t.get()->*parameters[i].get)();
      return values;
    };
    const Values other_values = read_values (other_track);
    std::array<uint64_t,3> notifications = {};
    Values notified_values = {};
    std::array<Emittable::Connection,3> connections;
    for (size_t i = 0; i < parameters.size(); i++)
      connections[i] = track->on_event (String ("notify:") + parameters[i].name, [&, i] (const Event&) {
        notifications[i]++;
        notified_values[i] = (track.get()->*parameters[i].get)();
      });
    auto verify = [&] (const Values &expected, const Values &before, const auto &previous_notifications) {
      const Values actual = read_values (track);
      for (size_t i = 0; i < parameters.size(); i++) {
        TASSERT (std::abs (actual[i] - expected[i]) < 0.001f);
        if (expected[i] != before[i]) {
          TASSERT (notifications[i] > previous_notifications[i]);
          TASSERT (std::abs (notified_values[i] - expected[i]) < 0.001f);
        }
      }
      TASSERT (read_values (other_track) == other_values);
    };
    const auto &parameter = parameters[property];
    const Values initial = read_values (track);
    auto previous_notifications = notifications;
    project->group_undo (parameter.name);
    (track.get()->*parameter.set) (parameter.value);
    project->ungroup_undo();
    const Values changed = read_values (track);
    TASSERT (std::abs (changed[property] - parameter.value) < 0.001f);
    TASSERT (changed[property] != initial[property]);
    if (property < 2) {
      TASSERT (changed[0] != initial[0]);
      TASSERT (changed[1] != initial[1]);
    }
    verify (changed, initial, previous_notifications);
    TASSERT (project->can_undo());
    TASSERT (!project->can_redo());

    for (int repeat = 0; repeat < 2; repeat++) {
      previous_notifications = notifications;
      project->undo();
      verify (initial, changed, previous_notifications);
      TASSERT (project->can_redo());

      previous_notifications = notifications;
      project->redo();
      verify (changed, initial, previous_notifications);
      TASSERT (!project->can_redo());
    }

    project->discard();
  }
}

static void
track_parameter_values()
{
  check_track_parameter_values (false);
}
TEST_ADD (track_parameter_values);

static void
master_parameter_values()
{
  check_track_parameter_values (true);
}
TEST_ADD (master_parameter_values);

static void
track_volume_pan()
{
  ProjectImplP project = ProjectImpl::create ("TrackVolumePanTest");
  TASSERT (project);

  TrackP track = project->create_track();
  TASSERT (track);

  // Test initial volume
  double initial_vol = track->volume_db();
  TASSERT (initial_vol >= -100.0 && initial_vol <= 20.0);

  // Test volume notifications
  uint64_t volume_notifications = 0;
  auto volume_connection = track->on_event ("notify:volume_db", [&volume_notifications] (const Event &event) { volume_notifications++; });

  uint64_t pan_notifications = 0;
  auto pan_connection = track->on_event ("notify:pan", [&pan_notifications] (const Event &event) { pan_notifications++; });

  // Test setting volume with notification
  uint64_t last_volume_notifications = volume_notifications;
  track->volume_db (-6.0);
  TASSERT (std::abs (track->volume_db() - (-6.0)) < 0.01);
  TASSERT (volume_notifications > last_volume_notifications);

  // Reset volume
  last_volume_notifications = volume_notifications;
  track->volume_db (0.0);
  TASSERT (std::abs (track->volume_db()) < 0.01);
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

  TrackP first = project->all_tracks().back();
  TASSERT (first->name() == "Track 1" && project->master_track()->name() == "Master");
  TrackP track = project->create_track();
  TASSERT (track && track->name() == "Track 2");

  // Set and get track name
  track->name ("MyTrack");
  TASSERT (track->name() == "MyTrack");

  track->name ("AnotherName");
  TASSERT (track->name() == "AnotherName");

  // Empty names show Tracktion's default
  track->name ("");
  TASSERT (track->name() == "Track 1");
  track->name ("Track 2");

  // New tracks get a free number, in the same undo step as their creation
  first->remove_self();
  TASSERT (track->name() == "Track 2");
  project->ungroup_undo();
  const size_t ntracks = project->all_tracks().size();
  TrackP third = project->create_track();
  TASSERT (third->name() == "Track 3");
  project->undo();
  TASSERT (project->all_tracks().size() == ntracks);

  project->discard();
}
TEST_ADD (track_name);

static void
track_fixed_names()
{
  ProjectP project = Server::instance().create_project ("FixedNamesTest");
  TrackS tracks = project->all_tracks();
  tracks.push_back (project->master_track());
  for (const TrackP &track : tracks)
    if (track->is_control_track() || track->is_master())
      {
        const String initial = track->name();
        if (initial == "Chord")
          continue;
        const StringS read_only = track->read_only_properties();
        TASSERT (std::count (read_only.begin(), read_only.end(), "name"));
        uint notifications = 0;
        auto connection = track->on_event ("notify:name", [&] (const Event&) { notifications++; });
        track->name ("Ignored Name");
        TASSERT (track->name() == initial && notifications == 0);
      }
  project->discard();
}
TEST_ADD (track_fixed_names);

static void
plugin_name()
{
  ProjectP project = Server::instance().create_project ("PluginNameTest");
  TrackP track = project->create_track();
  for (const String &type : { "text", "insert" })
    {
      PluginP plugin = track->create_plugin (type);
      TASSERT (plugin);
      const String initial = plugin->name();
      const StringS read_only = plugin->read_only_properties();
      TASSERT (!std::count (read_only.begin(), read_only.end(), "name"));
      uint notifications = 0;
      auto connection = plugin->on_event ("notify:name", [&] (const Event&) { notifications++; });
      const String project_name = project->name();
      project->ungroup_undo();
      project->name (project_name + " marker");
      plugin->name ("");
      TASSERT (plugin->name() == initial && notifications == 0);
      project->undo();
      TASSERT (project->name() == project_name);
      project->redo();
      TASSERT (project->name() == project_name + " marker");
      plugin->name ("New Name");
      TASSERT (plugin->name() == "New Name" && notifications > 0);
      notifications = 0;
      plugin->name ("");
      TASSERT (plugin->name() == initial && notifications > 0);
    }
  PluginP fixed = track->create_plugin ("4osc");
  TASSERT (fixed);
  const String initial = fixed->name();
  const StringS read_only = fixed->read_only_properties();
  TASSERT (std::count (read_only.begin(), read_only.end(), "name"));
  fixed->name ("Ignored Name");
  TASSERT (fixed->name() == initial);
  project->discard();
}
TEST_ADD (plugin_name);

static void
project_name_notifications()
{
  ProjectP project = Server::instance().create_project ("ProjectNameNotifyTest");
  TrackP track = project->create_track();
  GadgetS children = {
    track,
    track->create_midi_clip ("Midi", 0, 1),
    track->create_audio_clip ("Audio", 2, 1),
    track->create_plugin ("text"),
    track->create_plugin ("insert"),
  };
  uint notifications = 0;
  auto connection = project->on_event ("notify:name", [&] (const Event&) { notifications++; });
  for (const auto &child : children)
    {
      TASSERT (child);
      project->ungroup_undo();
      child->name ("Child Name");
      TASSERT (project->name() == "ProjectNameNotifyTest" && notifications == 0);
      project->undo();
      TASSERT (notifications == 0);
      project->redo();
      TASSERT (notifications == 0);
    }
  project->name ("Project Name");
  TASSERT (notifications > 0);
  project->discard();
}
TEST_ADD (project_name_notifications);

static void
name_undo_redo()
{
  ProjectP project = Server::instance().create_project ("NameUndoTest");
  TrackP track = project->create_track();
  TrackP chord;
  for (const TrackP &control : project->all_tracks())
    if (control->is_control_track() && control->name() == "Chord")
      chord = control;
  TASSERT (chord);
  const StringS read_only = chord->read_only_properties();
  TASSERT (!std::count (read_only.begin(), read_only.end(), "name"));
  const std::vector<std::pair<GadgetP,String>> writable = {
    { project, "" },
    { track, "Track 1" },
    { chord, "" },
    { track->create_midi_clip ("Midi", 0, 1), "" },
    { track->create_audio_clip ("Audio", 2, 1), "" },
    { track->create_plugin ("text"), "Text Plugin" },
    { track->create_plugin ("insert"), "Insert Plugin" },
  };
  for (const auto &[gadget, default_name] : writable)
    {
      TASSERT (gadget);
      const String initial = gadget->name();
      uint notifications = 0;
      auto connection = gadget->on_event ("notify:name", [&] (const Event&) { notifications++; });
      auto check = [&] (const String &expected) {
        TASSERT (gadget->name() == expected && notifications > 0);
        notifications = 0;
      };
      gadget->name ("First Name");
      check ("First Name");
      gadget->name ("Second Name");
      check ("Second Name");
      gadget->name ("Second Name");
      TASSERT (notifications == 0);
      project->undo();
      check ("First Name");
      project->undo();
      check (initial);
      project->redo();
      check ("First Name");
      project->redo();
      check ("Second Name");
      gadget->name ("");
      check (default_name);
      project->undo();
      check ("Second Name");
      project->redo();
      check (default_name);
    }
  project->discard();
}
TEST_ADD (name_undo_redo);

static void
server_name()
{
  Server &server = Server::instance();
  TASSERT (server.name().empty() && server.read_only_properties() == StringS { "name" });
  uint notifications = 0;
  auto connection = server.on_event ("notify:name", [&] (const Event&) { notifications++; });
  for (const String &name : { "Renamed Server", "" })
    {
      server.name (name);
      TASSERT (server.name().empty() && notifications == 0);
    }
}
TEST_ADD (server_name);

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
  TASSERT (clip && clip->name() == "TestClip");
  TASSERT (track->create_audio_clip ("TestAudio", 4.0, 4.0)->name() == "TestAudio");

  project->discard();
}
TEST_ADD (clip_creation);

static void
clip_audio_notes_read_only()
{
  ProjectImplP project = ProjectImpl::create ("AudioNotesReadOnly");
  ClipP clip = project->create_track()->create_audio_clip ("Audio", 0, 4);
  const StringS read_only = clip->read_only_properties();
  TASSERT (std::count (read_only.begin(), read_only.end(), "all_notes") == 1);
  uint notifications = 0;
  auto connection = clip->on_event ("notify", [&] (const Event&) { notifications++; });
  project->clear_undo();
  ClipNote note { .note_number = 60, .length_beats = 1, .velocity = 90 };
  clip->all_notes ({ note });
  TASSERT (clip->all_notes().empty());
  TASSERT (!project->can_undo() && notifications == 0);
  project->discard();
}
TEST_ADD (clip_audio_notes_read_only);

static void
clip_notes()
{
  ProjectP project = ProjectImpl::create ("ClipNotesTest");
  TrackP track = project->create_track();
  project->denominator (8);
  ClipP clip = track->create_midi_clip ("NotesClip", 0.0, 4.0);
  TASSERT (clip->all_notes().empty());

  ClipNote first;
  first.note_number = 60;
  first.start_beat = 0.125;
  first.length_beats = 0.375;
  first.velocity = 0;
  first.colour = 127;
  first.mute = true;
  ClipNote second = first;
  second.velocity = 127;
  second.colour = 3;
  second.mute = false;
  ClipNoteS added = clip->change_batch ({ first, second }, "Add Notes");
  TASSERT (added.size() == 2);
  TASSERT (added[0].id > 0 && added[1].id > 0 && added[0].id != added[1].id);
  first.id = added[0].id;
  second.id = added[1].id;
  TASSERT (added[0] == first && added[1] == second);
  TASSERT (clip->list_all_notes() == added);
  project->undo();
  TASSERT (clip->all_notes().empty());
  project->redo();
  TASSERT (clip->all_notes() == added);

  ClipNote moved = second;
  moved.note_number = 67;
  moved.start_beat = 1.0 / 3;
  moved.length_beats = 2.0 / 3;
  moved.velocity = 64;
  moved.colour = 12;
  moved.mute = true;
  TASSERT (clip->change_batch ({ moved }, "Move Note") == ClipNoteS { moved });
  TASSERT ((clip->all_notes() == ClipNoteS { first, moved }));
  project->undo();
  TASSERT (clip->all_notes() == added);
  project->redo();
  TASSERT ((clip->all_notes() == ClipNoteS { first, moved }));

  ClipNote removed = first;
  removed.length_beats = 0;
  clip->change_batch ({ removed }, "Delete Note");
  TASSERT (clip->all_notes() == ClipNoteS { moved });
  project->undo();
  TASSERT ((clip->all_notes() == ClipNoteS { first, moved }));
  project->redo();
  TASSERT (clip->all_notes() == ClipNoteS { moved });

  clip->all_notes ({ second });
  const auto replaced = clip->all_notes();
  TASSERT (replaced.size() == 1 && replaced[0].id != second.id);
  second.id = replaced[0].id;
  TASSERT (replaced == ClipNoteS { second });
  project->undo();
  TASSERT (clip->all_notes() == ClipNoteS { moved });
  project->redo();
  TASSERT (clip->all_notes() == replaced);

  ClipNote fresh = second;
  fresh.id = 0;
  const auto mixed = clip->change_batch ({ first, fresh }, "Skip Deleted Handle");
  TASSERT (mixed.size() == 2 && mixed[0].id == 0 && mixed[1].id > 0);
  TASSERT (clip->all_notes().size() == 2);
  const auto before_batch = clip->all_notes();
  ClipNote modified = before_batch[0], deleted = before_batch[0];
  modified.velocity = 42;
  deleted.length_beats = 0;
  const auto ordered = clip->change_batch ({ modified, fresh, deleted }, "Delete Before Modify");
  TASSERT (ordered.size() == 3 && ordered[0].id == 0 && ordered[1].id > 0 && ordered[2].id == deleted.id);
  TASSERT ((clip->all_notes() == ClipNoteS { before_batch[1], ordered[1] }));
  project->undo();
  TASSERT (clip->all_notes() == before_batch);
  project->redo();
  TASSERT ((clip->all_notes() == ClipNoteS { before_batch[1], ordered[1] }));
  clip->remove_self();
  TASSERT (clip->change_batch ({ fresh }, "Edit Removed Clip").empty());
  TASSERT (clip->position() == ClipPosition{});
  project->discard();
}
TEST_ADD (clip_notes);

static void
clip_note_notifications()
{
  ProjectImplP project = ProjectImpl::create ("NoteNotifications");
  ClipP clip = project->create_track()->create_midi_clip ("Notes", 0, 4);
  std::map<String,ClipNoteS> notified;
  auto connection = clip->on_event ("notify", [&] (const Event &event) {
    if (event.detail() == "notes" || event.detail() == "all_notes")
      notified[event.detail()] = clip->all_notes();
  });
  auto require_notifies = [&] (const ClipNoteS &notes) {
    TASSERT (notified.count ("notes") && notified.at ("notes") == notes);
    TASSERT (notified.count ("all_notes") && notified.at ("all_notes") == notes);
    notified.clear();
  };
  for (int step = 0; step < 3; step++)
    {
      const ClipNoteS before = clip->all_notes();
      project->clear_undo();
      ClipNote note { .note_number = 60, .length_beats = 1, .velocity = 90 };
      if (step > 0)
        {
          note = before.at (0);
          if (step == 1)
            note.velocity = 45;
          else
            note.length_beats = 0;
        }
      clip->change_batch ({ note }, "Change Note");
      const ClipNoteS after = clip->all_notes();
      TASSERT (before != after);
      require_notifies (after);
      project->undo();
      TASSERT (clip->all_notes() == before);
      require_notifies (before);
      project->redo();
      TASSERT (clip->all_notes() == after);
      require_notifies (after);
    }
  project->discard();
}
TEST_ADD (clip_note_notifications);

static void
clip_range()
{
  ProjectP project = ProjectImpl::create ("ClipRangeTest");
  TrackP track = project->create_track();
  project->bpm (120);
  project->numerator (6);
  for (int denominator : { 4, 8, 16 })
    {
      project->denominator (denominator);
      for (ClipP clip : { track->create_midi_clip ("MidiRange", 0.5, 1.25),
                          track->create_audio_clip ("AudioRange", 0.5, 1.25) })
        {
          const ClipPosition initial { { 0.5, 1.75 }, 0 };
          const double beats_per_second = denominator / 2.0;
          TASSERT (clip->position() == initial);
          TASSERT (clip->start_beat() == initial.time.start * beats_per_second);
          TASSERT (clip->end_beat() == initial.time.end * beats_per_second);
          uint position_notifications = 0, start_notifications = 0, end_notifications = 0;
          auto position_connection = clip->on_event ("notify:position", [&] (const Event&) { position_notifications++; });
          auto start_connection = clip->on_event ("notify:start_beat", [&] (const Event&) { start_notifications++; });
          auto end_connection = clip->on_event ("notify:end_beat", [&] (const Event&) { end_notifications++; });
          auto check_position = [&] (const ClipPosition &expected)
            {
              TASSERT (clip->position() == expected);
              TASSERT (clip->start_beat() == expected.time.start * beats_per_second);
              TASSERT (clip->end_beat() == expected.time.end * beats_per_second);
              TASSERT (position_notifications > 0 && start_notifications > 0 && end_notifications > 0);
              position_notifications = start_notifications = end_notifications = 0;
            };
          const ClipPosition changed { { 0.625, 2.125 }, 0.375 };
          clip->position (changed);
          check_position (changed);
          project->undo();
          check_position (initial);
          project->redo();
          check_position (changed);
          const ClipPosition offset_changed { changed.time, 0.875 };
          clip->position (offset_changed);
          check_position (offset_changed);
          project->undo();
          check_position (changed);
          project->redo();
          check_position (offset_changed);
        }
    }
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
  TASSERT (!mclip->muted());

  // Test mute notifications
  uint64_t muted_notifications = 0;
  auto muted_connection = mclip->on_event ("notify:muted", [&muted_notifications] (const Event &event) { muted_notifications++; });

  // Test mute with notification
  uint64_t last_muted_notifications = muted_notifications;
  mclip->muted (true);
  TASSERT (mclip->muted());
  TASSERT (muted_notifications > last_muted_notifications);

  last_muted_notifications = muted_notifications;
  project->undo();
  TASSERT (!mclip->muted() && muted_notifications > last_muted_notifications);
  last_muted_notifications = muted_notifications;
  project->redo();
  TASSERT (mclip->muted() && muted_notifications > last_muted_notifications);

  // Reset mute
  last_muted_notifications = muted_notifications;
  mclip->muted (false);
  TASSERT (!mclip->muted());
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
  mclip->muted (true);
  TASSERT (mclip->muted());
  TASSERT (muted_notifications > last_muted_notifications);

  // Undo mute
  TASSERT (project->can_undo());
  project->undo();
  TASSERT (!mclip->muted());

  // Redo mute
  TASSERT (project->can_redo());
  project->redo();
  TASSERT (mclip->muted());

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
  note.note_number = 60;
  note.start_beat = 0;
  note.length_beats = 0.375;
  note.velocity = 102;
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
  const ClipPosition initial_position = mclip->position();
  ClipPosition changed_position = initial_position;
  changed_position.time.start += 0.375;
  changed_position.time.end += 0.375;

  // Assign new range
  uint64_t range_notifications = 0;
  auto range_connection = mclip->on_event ("notify:position", [&range_notifications] (const Event &event) { range_notifications++; });

  uint64_t last_range_notifications = range_notifications;
  mclip->position (changed_position);
  TASSERT (mclip->position() == changed_position);
  TASSERT (range_notifications > last_range_notifications);

  // Undo range
  TASSERT (project->can_undo());
  project->undo();
  TASSERT (mclip->position() == initial_position);

  // Redo range
  TASSERT (project->can_redo());
  project->redo();
  TASSERT (mclip->position() == changed_position);

  project->discard();
}
TEST_ADD (clip_undo_redo);

static void
plugin_creation()
{
  ProjectP project = ProjectImpl::create ("PluginTest");
  TrackP track = project->create_track();
  TASSERT (track);
  PluginS plugins = track->list_plugins();
  TASSERT (!plugins.empty());
  PluginP plugin = plugins.front();
  TASSERT (plugin);
  TASSERT (!plugin->name().empty());
  TASSERT (!plugin->plugin_type().empty());

  for (bool check_enabled : { true, false })
    {
      const auto get = [&] { return check_enabled ? plugin->enabled() : plugin->frozen(); };
      const auto set = [&] (bool value) {
        if (check_enabled)
          plugin->enabled (value);
        else
          plugin->frozen (value);
      };
      uint notifications = 0;
      auto connection = plugin->on_event (check_enabled ? "notify:enabled" : "notify:frozen",
                                          [&] (const Event&) { notifications++; });
      const bool initial = get();
      project->ungroup_undo();
      set (!initial);
      TASSERT (get() != initial);
      TASSERT (notifications > 0);
      TASSERT (project->can_undo());

      notifications = 0;
      set (!initial);
      TASSERT (notifications == 0);
      project->undo();
      TASSERT (get() == initial);
      TASSERT (notifications > 0);
      TASSERT (project->can_redo());

      notifications = 0;
      project->redo();
      TASSERT (get() != initial);
      TASSERT (notifications > 0);
    }

  uint removed_count = 0;
  auto removed_connection = plugin->on_event ("object:removed", [&] (const Event&) { removed_count++; });
  plugin->remove_self();
  TASSERT (removed_count > 0);
  project->discard();
}
TEST_ADD (plugin_creation);

} // Anon
