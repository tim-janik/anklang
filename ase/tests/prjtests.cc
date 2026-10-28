// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <ase/project.hh>
#include <ase/main.hh>
#include <ase/clip.hh>
#include <ase/track.hh>
#include <ase/path.hh>
#include <ase/platform.hh>
#include <ase/storage.hh>
#include <ase/unicode.hh>
#include <ase/websocket.hh>
#include <ase/logging.hh>
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
  TASSERT (Path::stringread (Path::join (projects, "Unnamed-1/autosave/snapshot")) == "exit state");
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
        TASSERT (dir == Path::join (root, expected[i], "Unnamed-1"));
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
      TASSERT (project->project_dir (true) == Path::join (root, "Unnamed-1") && project->unnamed());
      TASSERT (project->save_project ("") == Error::INVALID_PROPERTY);
      TASSERT (!project->save_project ("Same Name"));
      TASSERT (project->project_dir (false) == Path::join (root, "Same Name" + suffix));
      project->discard();
    }
  const String blocked = Path::join (root, "Blocked");
  TASSERT (Path::stringwrite (blocked, "keep this file"));
  ProjectImplP project = ProjectImpl::create ("Blocked");
  TASSERT (!project->save_project ("Blocked"));
  TASSERT (project->project_dir (false) == blocked + "(1)");
  TASSERT (Path::stringread (blocked) == "keep this file");
  project->discard();
  for (const String &name : StringS { "", ".", "..", "../escape", "a/b\\c" })
    {
      ProjectImplP project = ProjectImpl::create (name);
      TASSERT (Path::dircontains (root, project->project_dir (true)));
      if (name.empty() || name == "." || name == "..")
        TASSERT (project->save_project (name) == Error::INVALID_PROPERTY);
      else
        {
          TASSERT (!project->save_project (name));
          TASSERT (Path::dircontains (root, project->project_dir (false)));
        }
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
project_grep_serialized()
{
  ProjectP project = ProjectImpl::create ("GrepTest");
  TASSERT (project->grep_serialized ("<TEMPO\\b[^>]*\\bbpm=\"([^\"]*)\"", 1) == "120.0");
  TASSERT (project->grep_serialized ("<TRACK\\b[^>]*\\bname=\"([^\"]*)\"", 1) == "Track 1");
  TrackP track = project->create_track();
  track->name ("Master2");
  project->bpm (133);
  ClipP clip = track->create_midi_clip ("Grep Clip", 0, 4);
  clip->all_notes ({ ClipNote { .note_number = 61, .length_beats = 1, .velocity = 99 } });
  TASSERT (project->grep_serialized ("<TRACK\\b[^>]*\\bname=\"(Master2)\"", 1) == "Master2");
  TASSERT (project->grep_serialized ("<TEMPO\\b[^>]*\\bbpm=\"([^\"]*)\"", 1) == "133.0");
  TASSERT (project->grep_serialized ("<NOTE\\b[^>]*\\bp=\"([^\"]*)\"", 1) == "61");
  // Long start tags stay on one line, with `\n` line ends
  TASSERT (project->grep_serialized ("<MIDICLIP\\b[^>\\n]*>") != "");
  TASSERT (project->grep_serialized ("\r") == "");
  project->discard();
}
TEST_ADD (project_grep_serialized);

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
track_insert_wave_clip()
{
  ProjectImplP project = ProjectImpl::create ("WaveClip");
  TrackP track = project->create_track();
  const String tmpdir = anklang_cachedir_create(), sample = Path::join (tmpdir, "source/piano.ogg");
  TASSERT (Path::mkdirs (Path::dirname (sample)));
  TASSERT (Path::copy_file (anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg"), sample));
  ClipP clip = track->insert_wave_clip ("Piano", sample, 1);
  TASSERT (clip && clip->position().time.start == 1 && clip->position().time.end > 1);
  TASSERT (project->unnamed());
  const String samples = Path::join (project->project_dir (false), "samples");
  auto links = [&] { StringS files; Path::glob (Path::join (samples, "*"), files); return files.size(); };
  const String link = Path::join (samples, Path::basename (sample));
  TASSERT (Path::check (link, "L") && Path::equals (link, sample) && links() == 1);
  TASSERT (clip->original_file() == link);
  // A link to the same file is reused
  TASSERT (track->insert_wave_clip ("Again", sample, 5)->original_file() == link && links() == 1);
  // A name clash adds a counter
  const String copy = Path::join (tmpdir, "copy", Path::basename (sample));
  TASSERT (Path::mkdirs (Path::dirname (copy)));
  const String link1 = Path::join (samples, "piano(1).ogg");
  TASSERT (Path::copy_file (sample, copy));
  TASSERT (track->insert_wave_clip ("Copy", copy, 9)->original_file() == link1 && links() == 2);
  TASSERT (Path::equals (link1, copy));
  // Files in the project dir need no link
  const String inside = Path::join (project->project_dir (false), "inside.ogg");
  TASSERT (Path::copy_file (sample, inside));
  TASSERT (track->insert_wave_clip ("Inside", inside, 13)->original_file() == inside && links() == 2);
  const String correct = Path::join (tmpdir, "real/piano.ogg"), decoy = Path::join (tmpdir, "piano.ogg");
  const String child = Path::join (tmpdir, "real/child"), shortcut = Path::join (tmpdir, "shortcut");
  TASSERT (Path::mkdirs (child));
  TASSERT (Path::copy_file (sample, correct) && Path::copy_file (sample, decoy));
  TASSERT (symlink (child.c_str(), shortcut.c_str()) == 0);
  ClipP through_link = track->insert_wave_clip ("Parent", Path::join (shortcut, "../piano.ogg"), 15);
  TASSERT (through_link && Path::equals (through_link->original_file(), correct));
  TASSERT (!Path::equals (through_link->original_file(), decoy));
  for (const String &dir : StringS { tmpdir, project->project_dir (false) })
    {
      const String native = Path::join (dir, "piano\x80.ogg");
      TASSERT (Path::copy_file (sample, native));
      ClipP bytes = track->insert_wave_clip ("Bytes", encodefs (native), 17);
      TASSERT (bytes && Path::equals (bytes->original_file(), native));
      TASSERT (WebSocketServer::utf8_validate (bytes->original_file()));
      ClipP again = track->insert_wave_clip ("BytesAgain", encodefs (native), 19);
      TASSERT (again && again->original_file() == bytes->original_file());
    }
  // Clips without a file
  TASSERT (track->create_midi_clip ("Midi", 20, 1)->original_file().empty());
  TASSERT (track->create_audio_clip ("Audio", 22, 1)->original_file().empty());
  project->discard();
  TASSERT (Path::check (sample, "f") && Path::check (copy, "f"));
  anklang_cachedir_cleanup (tmpdir);
}
TEST_ADD (track_insert_wave_clip);

static void
track_insert_wave_clip_loop_info()
{
  const String root = anklang_cachedir_create(), library = Path::join (root, "library");
  TestEnvironment samples ("ANKLANG_SAMPLE_DIRS", library), projects ("ANKLANG_PROJECTS_DIR", Path::join (root, "projects"));
  TASSERT (Path::mkdirs (library));
  // Half a second of mono PCM, with an ACID root note and four beats.
  String wav;
  auto u16 = [&] (uint16_t value) { wav += char (value); wav += char (value >> 8); };
  auto u32 = [&] (uint32_t value) { u16 (value); u16 (value >> 16); };
  constexpr uint32_t rate = 8000;
  wav += "RIFF"; u32 (rate + 68); wav += "WAVEfmt "; u32 (16);
  u16 (1); u16 (1); u32 (rate); u32 (rate * 2); u16 (2); u16 (16);
  wav += "acid"; u32 (24); u32 (2); u16 (60); u16 (0); u32 (0);
  u32 (4); u16 (4); u16 (4); u32 (0);
  wav += "data"; u32 (rate); wav.append (rate, '\0');
  ProjectP project = Server::instance().create_project ("Loop Metadata");
  project->bpm (120);
  TrackP track = project->create_track();
  double start = 3;
  for (const String &dir : StringS { root, library })
    {
      const String file = Path::join (dir, "acid-loop.wav");
      TASSERT (Path::stringwrite (file, wav));
      ClipP clip = track->insert_wave_clip ("Loop", file, start);
      TASSERT (clip && Path::equals (clip->original_file(), file));
      TCMP (clip->position().time.start, ==, start);
      TCMP (clip->position().time.end, ==, start + 2);
      start += 3;
    }
  TASSERT (project->grep_serialized ("autoTempo=\"1\"") == "autoTempo=\"1\"");
  TASSERT (project->grep_serialized ("autoPitch=\"1\"") == "autoPitch=\"1\"");
  TASSERT (project->grep_serialized ("<LOOPINFO[^>]*rootNote=\"60\"[^>]*") != "");
  project->discard();
  anklang_cachedir_cleanup (root);
}
TEST_ADD (track_insert_wave_clip_loop_info);

static void
track_insert_wave_clip_symlink_project_dir()
{
  const String root = anklang_cachedir_create();
  TASSERT (!root.empty());
  const String real = Path::join (root, "real"), link = Path::join (root, "link");
  TASSERT (Path::mkdirs (real));
  TASSERT (symlink (real.c_str(), link.c_str()) == 0);
  TestEnvironment projects ("ANKLANG_PROJECTS_DIR", Path::join (link, "projects"));
  track_insert_wave_clip();
  anklang_cachedir_cleanup (root);
}
TEST_ADD (track_insert_wave_clip_symlink_project_dir);

static void
track_insert_wave_clip_invalid_file()
{
  ProjectImplP project = ProjectImpl::create ("InvalidWaveClip");
  TrackP track = project->create_track();
  const String dir = project->project_dir (true), samples = Path::join (dir, "samples");
  const String root = anklang_cachedir_create();
  TASSERT (!dir.empty() && !root.empty());
  const String invalid = Path::join (root, "invalid.ogg"), existing = Path::join (samples, "existing.ogg");
  const String source = Path::join (root, "existing.ogg"), internal = Path::join (dir, "internal.ogg");
  TASSERT (Path::stringwrite (invalid, "not audio"));
  TASSERT (Path::stringwrite (source, "not audio") && Path::stringwrite (internal, "not audio"));
  TASSERT (Path::mkdirs (samples));
  TASSERT (symlink (source.c_str(), existing.c_str()) == 0);
  const bool fatal_warnings = logging_fatal_warnings;
  for (const String &file : StringS { "", Path::join (root, "missing.ogg"), root, invalid, source, existing, internal })
    {
      logging_fatal_warnings = false;
      ClipP clip = track->insert_wave_clip ("Invalid", file, 0);
      logging_fatal_warnings = fatal_warnings;
      TASSERT (!clip && track->all_clips().empty());
      StringS files;
      Path::glob (Path::join (samples, "*"), files);
      TASSERT (files == StringS { existing });
    }
  TASSERT (Path::check (existing, "L") && Path::check (invalid, "f") && Path::check (internal, "f"));
  const String sample = anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg");
  const String valid = Path::join (root, "valid/invalid.ogg");
  TASSERT (Path::mkdirs (Path::dirname (valid)) && Path::copy_file (sample, valid));
  ClipP clip = track->insert_wave_clip ("Valid", valid, 0);
  TASSERT (clip && clip->original_file() == Path::join (samples, "invalid.ogg"));
  project->discard();
  anklang_cachedir_cleanup (root);
}
TEST_ADD (track_insert_wave_clip_invalid_file);

static ProjectP
project_with_assets (const StringS &references, const String &dir = "")
{
  String xml = "<EDIT><TRACK id=\"1001\" name=\"Assets\">";
  for (size_t i = 0; i < references.size(); i++)
    xml += string_format ("<AUDIOCLIP id=\"%zu\" name=\"Asset\" start=\"%zu\" length=\"1\" source=\"%s\"/>",
                          2001 + i, i, references[i]);
  xml += "</TRACK></EDIT>";
  auto project = ProjectImpl::from_xml (xml, dir);
  TASSERT (project);
  return project;
}

static ClipS
asset_clips (ProjectP project)
{
  for (const auto &track : project->all_tracks())
    if (track->name() == "Assets")
      return track->all_clips();
  TASSERT (false);
  return {};
}

static void
project_unresolved_assets_notify()
{
  const String root = anklang_cachedir_create(), file = Path::join (root, "missing-notify.ogg");
  TestEnvironment projects ("ANKLANG_PROJECTS_DIR", Path::join (root, "projects"));
  TASSERT (Path::copy_file (anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg"), file));
  ProjectP project = Server::instance().create_project ("Asset Notifications");
  TrackP track = project->create_track();
  track->name ("Assets");
  TASSERT (track->insert_wave_clip ("Missing", file, 0));
  TASSERT (unlink (file.c_str()) == 0);
  const String reference = "samples/missing-notify.ogg";
  TASSERT (project->unresolved_assets() == StringS { reference });
  uint notifications = 0;
  auto connection = project->on_event ("notify:unresolved_assets", [&] (const Event&) { notifications++; });
  auto expect_assets = [&] (const StringS &expected) {
    TASSERT (project->unresolved_assets() == expected);
    TASSERT (notifications > 0);
    notifications = 0;
  };
  auto check_remove = [&] (auto remove) {
    project->ungroup_undo();
    remove();
    expect_assets ({});
    project->undo();
    expect_assets ({ reference });
    project->redo();
    expect_assets ({});
    project->undo();
    expect_assets ({ reference });
  };
  check_remove ([&] { asset_clips (project)[0]->remove_self(); });
  check_remove ([&] {
    for (const auto &track : project->all_tracks())
      if (track->name() == "Assets")
        {
          track->remove_self();
          break;
        }
  });
  project->discard();
  anklang_cachedir_cleanup (root);
}
TEST_ADD (project_unresolved_assets_notify);

static void
project_asset_references()
{
  const String root = anklang_cachedir_create();
  TestEnvironment projects ("ANKLANG_PROJECTS_DIR", Path::join (root, "projects"));
  const String dir = Path::join (root, "local");
  const String sample = anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg");
  TASSERT (Path::mkdirs (dir));
  const String absolute = Path::join (root, "x.wav");
  TASSERT (Path::copy_file (sample, absolute));
  const StringS references { absolute, "../x.wav", "a/../x.wav", "C:\\x.wav", "\\\\host\\x.wav",
                             "/abs/x.wav", "C:/x.wav", "C:x.wav", "a\\x.wav", "..", "a/..", "1/2", "missing.wav" };
  TASSERT (Path::mkdirs (Path::join (dir, "a")));
  TASSERT (Path::mkdirs (Path::join (dir, "1")));
  TASSERT (Path::copy_file (sample, Path::join (dir, "x.wav")));
  TASSERT (Path::copy_file (sample, Path::join (dir, "1/2")));
  for (const String &reference : StringS { "C:\\x.wav", "\\\\host\\x.wav", "C:/x.wav", "C:x.wav", "a\\x.wav" })
    {
      const String file = Path::join (dir, reference);
      TASSERT (Path::mkdirs (Path::dirname (file)) && Path::copy_file (sample, file));
    }
  auto project = project_with_assets (references, dir);
  StringS expected = references;
  std::sort (expected.begin(), expected.end());
  TASSERT (project->unresolved_assets() == expected);
  auto check = [&] (ProjectP p) {
    for (const auto &clip : asset_clips (p))
      TASSERT (clip->original_file().empty());
    const String xml = p->grep_serialized ("[\\s\\S]*");
    for (const String &reference : references)
      TASSERT (xml.find ("source=\"" + reference + "\"") != String::npos);
  };
  check (project);
  TASSERT (!project->save_project (""));
  project->discard();
  TASSERT (!Server::instance().open_project (dir));
  auto opened = Server::instance().last_project();
  check (opened);
  TASSERT (opened->unresolved_assets() == expected);
  opened->discard();
  anklang_cachedir_cleanup (root);
}
TEST_ADD (project_asset_references);

static void
project_asset_libraries()
{
  const String root = anklang_cachedir_create();
  const String first = Path::join (root, "first"), second = Path::join (root, "second");
  const String setting = Path::join (root, "setting"), sfz = Path::join (root, "sfz"), sf2 = Path::join (root, "sf2");
  TestEnvironment projects ("ANKLANG_PROJECTS_DIR", Path::join (root, "projects"));
  TestEnvironment samples ("ANKLANG_SAMPLE_DIRS", Path::searchpath_join (first, second));
  TestEnvironment sfz_path ("SFZ_PATH", sfz), sf2_path ("SF2_PATH", sf2);
  auto preference = Server::instance().access_preference ("anklang-sample-dirs");
  TASSERT (preference);
  const Value previous = preference->value();
  preference->value (setting);
  const String factory_ref = "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg";
  const String factory = anklang_runpath (RPath::SAMPLEDIR, factory_ref);
  const StringS roots { first, second, setting, sfz, sf2 };
  for (const String &dir : roots)
    {
      TASSERT (Path::mkdirs (dir));
      TASSERT (Path::copy_file (factory, Path::join (dir, "shared.ogg")));
      TASSERT (Path::copy_file (factory, Path::join (dir, Path::basename (dir) + ".ogg")));
      TASSERT (Path::mkdirs (Path::dirname (Path::join (dir, factory_ref))));
      TASSERT (Path::copy_file (factory, Path::join (dir, factory_ref)));
    }
  const String escape = Path::join (first, "escape.ogg"), sibling = first + "-sibling";
  TASSERT (Path::mkdirs (sibling) && Path::copy_file (factory, Path::join (sibling, "outside.ogg")));
  TASSERT (symlink (Path::join (sibling, "outside.ogg").c_str(), escape.c_str()) == 0);
  TASSERT (symlink (Path::join (first, "first.ogg").c_str(), Path::join (first, "inside.ogg").c_str()) == 0);
  auto project = project_with_assets ({ factory_ref, "shared.ogg", "escape.ogg", "inside.ogg" });
  auto clips = asset_clips (project);
  TCMP (clips[0]->original_file(), ==, factory);
  TCMP (clips[1]->original_file(), ==, Path::join (first, "shared.ogg"));
  TASSERT (clips[2]->original_file().empty() && Path::equals (clips[3]->original_file(), Path::join (first, "first.ogg")));
  TASSERT (project->unresolved_assets() == StringS { "escape.ogg" });
  for (const String &dir : roots)
    {
      TASSERT (clips[1]->original_file() == Path::join (dir, "shared.ogg"));
      TASSERT (unlink (Path::join (dir, "shared.ogg").c_str()) == 0);
    }
  TASSERT (clips[1]->original_file().empty());
  TrackP track = project->create_track();
  TASSERT (track->insert_wave_clip ("Factory", factory, 0)->original_file() == factory);
  for (const String &dir : roots)
    {
      const String reference = Path::basename (dir) + ".ogg", file = Path::join (dir, reference);
      TASSERT (track->insert_wave_clip ("Library", file, 0)->original_file() == file);
      TASSERT (project->grep_serialized ("source=\"" + reference + "\"") == "source=\"" + reference + "\"");
    }
  TASSERT (project->project_dir().empty());
  TASSERT (!project->save_project ("Libraries"));
  const String local = project->project_dir();
  TASSERT (!Path::check (Path::join (local, "samples"), "e"));
  TASSERT (Path::mkdirs (Path::dirname (Path::join (local, factory_ref))));
  TASSERT (symlink (factory.c_str(), Path::join (local, factory_ref).c_str()) == 0);
  TASSERT (clips[0]->original_file() == Path::join (local, factory_ref));
  TASSERT (symlink (factory.c_str(), Path::join (local, "escape.ogg").c_str()) == 0);
  TASSERT (Path::equals (clips[2]->original_file(), factory));
  TASSERT (project->unresolved_assets() == StringS { "shared.ogg" });
  {
    TestEnvironment filesystem_root ("ANKLANG_SAMPLE_DIRS", "/");
    auto from_root = project_with_assets ({ factory.substr (1) });
    TASSERT (asset_clips (from_root)[0]->original_file() == factory);
    from_root->discard();
  }
  const String chosen = Path::join (second, "clash.ogg");
  TASSERT (Path::copy_file (factory, chosen) && Path::copy_file (factory, Path::join (first, "clash.ogg")));
  auto clash = track->insert_wave_clip ("Picked", chosen, 0);
  TASSERT (clash && Path::equals (clash->original_file(), chosen));
  project->discard();
  preference->value (previous);
  anklang_cachedir_cleanup (root);
}
TEST_ADD (project_asset_libraries);

static CoTaskVoid
project_asset_preferences()
{
  const String root = anklang_cachedir_create(), first = Path::join (root, "first"), second = Path::join (root, "second");
  TestEnvironment samples ("ANKLANG_SAMPLE_DIRS", ""), sfz ("SFZ_PATH", ""), sf2 ("SF2_PATH", "");
  auto preference = Server::instance().access_preference ("anklang-sample-dirs");
  TASSERT (preference);
  const Value previous = preference->value();
  Emittable::Connection preference_connection;
  auto change_preference = [&] (PropertyP property, const Value &value) {
    auto changed = main_loop->make_promise ([&] (std::function<void()> resolve) {
      preference_connection = property->on_event ("notify:" + property->ident(), [resolve] (const Event&) { resolve(); });
    });
    property->value (value);
    return changed;
  };
  co_await change_preference (preference, "");
  const String reference = "pref-library.ogg";
  const String sample = anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg");
  for (const String &dir : StringS { first, second })
    TASSERT (Path::mkdirs (dir) && Path::copy_file (sample, Path::join (dir, reference)));
  auto project = project_with_assets ({ reference }), other = project_with_assets ({ reference });
  auto clip = asset_clips (project)[0], other_clip = asset_clips (other)[0];
  TASSERT (project->unresolved_assets() == StringS { reference });
  TASSERT (other->unresolved_assets() == StringS { reference });
  uint notifications = 0, other_notifications = 0, originals = 0;
  auto connection = project->on_event ("notify:unresolved_assets", [&] (const Event&) { notifications++; });
  auto other_connection = other->on_event ("notify:unresolved_assets", [&] (const Event&) { other_notifications++; });
  auto clip_connection = clip->on_event ("notify:original_file", [&] (const Event&) { originals++; });
  co_await change_preference (preference, first);
  TASSERT (project->unresolved_assets().empty() && other->unresolved_assets().empty());
  TASSERT (notifications > 0 && other_notifications > 0 && originals > 0);
  TASSERT (clip->original_file() == Path::join (first, reference));
  TASSERT (other_clip->original_file() == Path::join (first, reference));
  const uint before_switch = notifications, original_before_switch = originals;
  co_await change_preference (preference, second);
  TASSERT (notifications > before_switch && originals > original_before_switch);
  TASSERT (clip->original_file() == Path::join (second, reference));
  TASSERT (other_clip->original_file() == Path::join (second, reference));
  const uint before_unrelated = notifications;
  auto unrelated = Server::instance().access_preference ("anklang-projects-dir");
  TASSERT (unrelated);
  co_await change_preference (unrelated, unrelated->value());
  TASSERT (notifications == before_unrelated);
  co_await change_preference (preference, "");
  TASSERT (notifications > before_unrelated);
  TASSERT (project->unresolved_assets() == StringS { reference });
  TASSERT (other->unresolved_assets() == StringS { reference });
  TASSERT (clip->original_file().empty() && other_clip->original_file().empty());
  std::weak_ptr<Project> weak = project;
  project->discard();
  project.reset();
  TASSERT (weak.expired());
  const uint after_destroy = notifications, other_before_change = other_notifications;
  co_await change_preference (preference, first);
  TASSERT (notifications == after_destroy && other_notifications > other_before_change);
  TASSERT (other->unresolved_assets().empty());
  other->discard();
  other.reset();
  co_await change_preference (preference, previous);
  preference_connection.disconnect();
  anklang_cachedir_cleanup (root);
}
TEST_ADD (project_asset_preferences);

static void
project_asset_reappears()
{
  const String root = anklang_cachedir_create();
  TestEnvironment projects ("ANKLANG_PROJECTS_DIR", root);
  TestEnvironment samples ("ANKLANG_SAMPLE_DIRS", Path::join (root, "library"));
  auto project = project_with_assets ({ "returned.ogg" });
  TASSERT (project->unresolved_assets() == StringS { "returned.ogg" });
  TASSERT (!project->save_project ("Reappears"));
  const String dir = project->project_dir();
  project->discard();
  const String file = Path::join (root, "library/returned.ogg");
  TASSERT (Path::mkdirs (Path::dirname (file)));
  TASSERT (Path::copy_file (anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg"), file));
  TASSERT (!Server::instance().open_project (dir));
  auto opened = Server::instance().last_project();
  TASSERT (opened->unresolved_assets().empty() && asset_clips (opened)[0]->original_file() == file);
  TASSERT (opened->grep_serialized ("source=\"returned.ogg\"") == "source=\"returned.ogg\"");
  opened->discard();
  anklang_cachedir_cleanup (root);
}
TEST_ADD (project_asset_reappears);

static void
project_replace_asset()
{
  const String root = anklang_cachedir_create();
  TestEnvironment projects ("ANKLANG_PROJECTS_DIR", Path::join (root, "projects"));
  auto project = project_with_assets ({ "../missing.ogg", "../missing.ogg", "other.ogg" });
  auto clips = asset_clips (project);
  uint notifications = 0, originals = 0;
  auto connection = project->on_event ("notify:unresolved_assets", [&] (const Event&) { notifications++; });
  auto clip_connection = clips[0]->on_event ("notify:original_file", [&] (const Event&) { originals++; });
  const String source = anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg");
  const String file = Path::join (root, "replacement.ogg");
  TASSERT (Path::copy_file (source, file));
  TASSERT (project->replace_asset ("absent.ogg", file) == Error::INVALID_PROPERTY);
  TASSERT (project->replace_asset ("../missing.ogg", "") == Error::FILE_NOT_FOUND);
  TASSERT (!project->can_undo());
  TASSERT (!project->replace_asset ("../missing.ogg", file));
  const String link = Path::join (project->project_dir(), "samples/replacement.ogg");
  TASSERT (Path::check (link, "L") && Path::equals (link, file));
  TASSERT (clips[0]->original_file() == link && clips[1]->original_file() == link && clips[2]->original_file().empty());
  TASSERT (project->unresolved_assets() == StringS { "other.ogg" });
  TASSERT (notifications > 0 && originals > 0);
  const uint before_undo = notifications, original_before_undo = originals;
  project->undo();
  TASSERT (!project->can_undo());
  TASSERT (clips[0]->original_file().empty() && clips[1]->original_file().empty());
  TASSERT (project->unresolved_assets() == StringS ({ "../missing.ogg", "other.ogg" }));
  TASSERT (project->grep_serialized ("source=\"[.][.]/missing.ogg\"") == "source=\"../missing.ogg\"");
  TASSERT (notifications > before_undo && originals > original_before_undo);
  project->redo();
  TASSERT (project->unresolved_assets() == StringS { "other.ogg" });
  TASSERT (clips[0]->original_file() == link && clips[1]->original_file() == link);
  TASSERT (!project->replace_asset ("samples/replacement.ogg", source));
  TASSERT (clips[0]->original_file() == source && clips[1]->original_file() == source);
  project->undo();
  TASSERT (clips[0]->original_file() == link && clips[1]->original_file() == link);
  TASSERT (!project->save_project ("Replaced"));
  const String dir = project->project_dir();
  project->discard();
  TASSERT (!Server::instance().open_project (dir));
  auto opened = Server::instance().last_project();
  TASSERT (opened->unresolved_assets() == StringS { "other.ogg" });
  TASSERT (Path::equals (asset_clips (opened)[0]->original_file(), file));
  opened->discard();
  anklang_cachedir_cleanup (root);
}
TEST_ADD (project_replace_asset);

static void
project_replace_asset_invalid_file()
{
  const String root = anklang_cachedir_create(), dir = Path::join (root, "project");
  TASSERT (Path::mkdirs (dir));
  auto project = project_with_assets ({ "missing.ogg", "missing.ogg" }, dir);
  const auto clips = asset_clips (project);
  const String invalid = Path::join (root, "invalid.ogg"), link = Path::join (dir, "samples/invalid.ogg");
  TASSERT (Path::stringwrite (invalid, "not audio"));
  TASSERT (project->replace_asset ("missing.ogg", invalid) == Error::FORMAT_INVALID);
  TASSERT (!Path::check (link, "L") && !Path::check (link, "e"));
  TASSERT (!project->can_undo());
  TASSERT (clips[0]->original_file().empty() && clips[1]->original_file().empty());
  TASSERT (project->unresolved_assets() == StringS { "missing.ogg" });
  TASSERT (project->grep_serialized ("source=\"missing.ogg\"") == "source=\"missing.ogg\"");
  TASSERT (symlink (invalid.c_str(), link.c_str()) == 0);
  TASSERT (project->replace_asset ("missing.ogg", invalid) == Error::FORMAT_INVALID);
  TASSERT (Path::check (link, "L") && !project->can_undo());
  TASSERT (project->unresolved_assets() == StringS { "missing.ogg" });
  const String native = Path::join (root, "piano\x80.ogg");
  TASSERT (Path::copy_file (anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg"), native));
  TASSERT (!project->replace_asset ("missing.ogg", encodefs (native)));
  TASSERT (Path::equals (clips[0]->original_file(), native) && Path::equals (clips[1]->original_file(), native));
  project->undo();
  TASSERT (project->unresolved_assets() == StringS { "missing.ogg" } && !project->can_undo());
  project->discard();
  anklang_cachedir_cleanup (root);
}
TEST_ADD (project_replace_asset_invalid_file);

static void
track_all_clips()
{
  ProjectImplP project = ProjectImpl::create ("TrackClips");
  TrackP track = project->create_track();
  uint notifications = 0;
  auto connection = track->on_event ("notify:all_clips", [&] (const Event&) { notifications++; });
  ClipP midi = track->create_midi_clip ("Midi", 0, 2);
  ClipP audio = track->create_audio_clip ("Audio", 2, 2);
  TASSERT (track->all_clips() == ClipS ({ midi, audio }) && notifications >= 2);
  TASSERT (track->launcher_clips() == ClipS ({ midi }));
  notifications = 0;
  audio->remove_self();
  TASSERT (track->all_clips() == ClipS ({ midi }) && notifications > 0);
  project->discard();
}
TEST_ADD (track_all_clips);

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
