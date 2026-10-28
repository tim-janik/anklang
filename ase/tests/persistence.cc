// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <ase/project.hh>
#include <ase/main.hh>
#include <ase/path.hh>
#include <ase/storage.hh>
#include <ase/testing.hh>
#include <signal.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>

namespace {
using namespace Ase;

struct ProjectFiles {
  String root = anklang_cachedir_create();
  String previous = getenv ("ANKLANG_PROJECTS_DIR") ?: "";
  bool existed = getenv ("ANKLANG_PROJECTS_DIR");
  ProjectFiles()
  {
    TASSERT (!root.empty());
    TASSERT (setenv ("ANKLANG_PROJECTS_DIR", root.c_str(), 1) == 0);
  }
  ~ProjectFiles()
  {
    TASSERT ((existed ? setenv ("ANKLANG_PROJECTS_DIR", previous.c_str(), 1) : unsetenv ("ANKLANG_PROJECTS_DIR")) == 0);
    anklang_cachedir_cleanup (root);
  }
};

static Error
with_file_size_limit (rlim_t size, const std::function<Error()> &write)
{
  struct rlimit previous = {};
  struct sigaction ignored = {}, previous_action = {};
  ignored.sa_handler = SIG_IGN;
  sigemptyset (&ignored.sa_mask);
  TASSERT (getrlimit (RLIMIT_FSIZE, &previous) == 0);
  const struct rlimit limit = { std::min (size, previous.rlim_cur), previous.rlim_max };
  TASSERT (sigaction (SIGXFSZ, &ignored, &previous_action) == 0);
  TASSERT (setrlimit (RLIMIT_FSIZE, &limit) == 0);
  const Error error = write();
  TASSERT (setrlimit (RLIMIT_FSIZE, &previous) == 0);
  TASSERT (sigaction (SIGXFSZ, &previous_action, nullptr) == 0);
  return error;
}

static String
saved_xml (const String &dir)
{
  StorageReader reader;
  TASSERT (!reader.open_for_reading (Path::join (dir, "project.anklang")));
  TASSERT (reader.stringread ("mimetype") == "application/x-anklang");
  TASSERT (reader.list_files() == StringS ({ "mimetype", "edit.tracktionedit" }));
  const String xml = reader.stringread ("edit.tracktionedit");
  TASSERT (!reader.close());
  return xml;
}

static void
write_project (const String &dir, const String &xml, const String &mimetype = "application/x-anklang")
{
  TASSERT (Path::mkdirs (dir));
  StorageWriter writer;
  TASSERT (!writer.open_with_mimetype (Path::join (dir, "project.anklang"), mimetype));
  TASSERT (!writer.store_file_data ("edit.tracktionedit", xml));
  TASSERT (!writer.close());
}

static TrackP
named_track (ProjectP project, const String &name)
{
  for (const auto &track : project->all_tracks())
    if (track->name() == name)
      return track;
  TASSERT (false);
  return nullptr;
}

static void
project_save_open()
{
  ProjectFiles files;
  const String linked_root = Path::join (files.root, "projects-root");
  TASSERT (symlink (files.root.c_str(), linked_root.c_str()) == 0);
  TASSERT (setenv ("ANKLANG_PROJECTS_DIR", linked_root.c_str(), 1) == 0);
  Server &server = Server::instance();
  ProjectP project = server.create_project ("Initial");
  TASSERT (project->unnamed() && project->project_dir().empty());
  project->bpm (137.5);
  project->numerator (7);
  project->denominator (8);
  TrackP track = project->create_track();
  track->name ("Saved Track");
  track->hidden (true);
  ClipP clip = track->create_midi_clip ("Saved Clip", 1, 3);
  clip->change_batch ({ { .id = -1, .note_number = 64, .start_beat = 0.5, .length_beats = 1.25, .velocity = 93 } }, "Notes");
  PluginP plugin = track->create_plugin ("4osc");
  TASSERT (plugin);
  plugin->enabled (false);
  const ClipPosition position = clip->position();
  ClipNoteS notes = clip->all_notes();
  for (auto &note : notes)
    note.id = 0;
  TASSERT (!project->save_project ("Saved Name"));
  const String dir = project->project_dir(), target = Path::join (dir, "project.anklang");
  TASSERT (Path::equals (dir, Path::join (linked_root, "Saved Name")));
  TASSERT (project->name() == "Saved Name" && !project->unnamed());
  const String xml = saved_xml (dir), first = Path::stringread (target);
  const String id = project->grep_serialized ("projectID=\"([^\"]*)\"", 1);
  const String serialized_notes = project->grep_serialized ("<NOTE [^>]*");
  const String serialized_plugin = project->grep_serialized ("<PLUGIN[^>]*type=\"4osc\"[^>]*");
  TASSERT (!id.empty() && !serialized_notes.empty() && !serialized_plugin.empty());
  PropertyP sync_saves = server.access_preference ("anklang-fsync-saves");
  TASSERT (sync_saves && !sync_saves->value().as_int());
  sync_saves->value (true);
  TASSERT (sync_saves->value().as_int());
  TASSERT (!project->save_project ("Ignored Name"));
  sync_saves->value (false);
  TASSERT (project->name() == "Saved Name" && project->project_dir() == dir);
  StringS backups;
  Path::glob (Path::join (Path::glob_escape (dir), "backup/*"), backups);
  TASSERT (backups.size() == 1 && Path::stringread (backups[0]) == first);
  TASSERT (server.open_project (dir) == Error::OPERATION_BUSY);
  const String alias = Path::join (files.root, "alias");
  TASSERT (symlink (dir.c_str(), alias.c_str()) == 0);
  TASSERT (server.open_project (Path::join (alias, "project.anklang")) == Error::OPERATION_BUSY);
  project->discard();
  TASSERT (!server.open_project (dir));
  ProjectP opened = server.last_project();
  TASSERT (opened && opened != project && !opened->can_undo() && !opened->can_redo());
  TASSERT (!project->remove_project_dir() && Path::check (target, "f"));
  TASSERT (opened->name() == "Saved Name" && Path::equals (opened->project_dir(), dir) && !opened->unnamed());
  TASSERT (opened->bpm() == 137.5 && opened->numerator() == 7 && opened->denominator() == 8);
  TASSERT (opened->all_tracks().size() == project->all_tracks().size());
  TrackP loaded_track = named_track (opened, "Saved Track");
  TASSERT (loaded_track->hidden());
  TASSERT (loaded_track->all_clips().size() == 1);
  ClipP loaded_clip = loaded_track->all_clips()[0];
  TASSERT (loaded_clip->name() == "Saved Clip" && loaded_clip->position() == position);
  ClipNoteS loaded_notes = loaded_clip->all_notes();
  for (auto &note : loaded_notes)
    note.id = 0;
  TASSERT (loaded_notes == notes);
  bool found_plugin = false;
  for (const auto &loaded_plugin : loaded_track->list_plugins())
    if (loaded_plugin->plugin_type() == "4osc")
      {
        TASSERT (!loaded_plugin->enabled());
        found_plugin = true;
      }
  TASSERT (found_plugin);
  TASSERT (opened->grep_serialized ("projectID=\"([^\"]*)\"", 1) == id);
  TASSERT (opened->grep_serialized ("<NOTE [^>]*") == serialized_notes);
  TASSERT (opened->grep_serialized ("<PLUGIN[^>]*type=\"4osc\"[^>]*") == serialized_plugin);
  opened->discard();
  TASSERT (!server.open_project (target));
  server.last_project()->discard();
  ProjectImplP memory = ProjectImpl::from_xml (xml);
  TASSERT (memory && memory->project_dir().empty() && !memory->can_undo());
  TASSERT (named_track (memory, "Saved Track")->all_clips().size() == 1);
  memory->discard();
}
TEST_ADD (project_save_open);

static void
project_save_backups()
{
  ProjectFiles files;
  ProjectP project = Server::instance().create_project ("");
  TASSERT (!project->save_project ("Backups [x]*?"));
  const String dir = project->project_dir(), target = Path::join (dir, "project.anklang");
  String previous;
  for (uint i = 0; i < 27; i++)
    {
      previous = Path::stringread (target);
      project->bpm (100 + i);
      TASSERT (!project->save_project (""));
      TASSERT (Path::check (target, "f"));
      StringS backups;
      Path::glob (Path::join (Path::glob_escape (dir), "backup/*"), backups);
      TASSERT (backups.size() == std::min (i + 1, 24u));
      TASSERT (std::any_of (backups.begin(), backups.end(), [&] (const String &file) { return Path::stringread (file) == previous; }));
    }
  previous = Path::stringread (target);
  Path::rmrf (Path::join (dir, "backup"));
  TASSERT (Path::stringwrite (Path::join (dir, "backup"), "blocked"));
  uint backup_notices = 0;
  auto connection = Server::instance().on_event ("usernote", [&] (const Event&) { backup_notices++; });
  project->bpm (200);
  TASSERT (!project->save_project (""));
  TASSERT (backup_notices == 1 && Path::stringread (target) != previous);
  previous = Path::stringread (target);
  if (geteuid() != 0) // Root bypasses directory write permissions.
    {
      TASSERT (chmod (dir.c_str(), 0500) == 0);
      project->bpm (201);
      const Error error = project->save_project ("");
      TASSERT (chmod (dir.c_str(), 0700) == 0);
      TASSERT (!!error && Path::stringread (target) == previous);
    }
  StringS temporary;
  Path::glob (Path::join (Path::glob_escape (dir), ".project.anklang-*"), temporary);
  TASSERT (temporary.empty());
  project->discard();
}
TEST_ADD (project_save_backups);

static void
project_save_close_errors()
{
  ProjectFiles files;
  const String archive = Path::join (files.root, "close-error.anklang");
  // Small and large central directories fail in fclose() and fwrite(), respectively.
  for (const size_t name_size : { 16, 8192 })
    {
      StorageWriter writer;
      TASSERT (!writer.open_with_mimetype (archive, "application/x-anklang"));
      TASSERT (!writer.store_file_data (String (name_size, 'x'), "data"));
      const Error error = with_file_size_limit (Path::file_size (archive), [&] { return writer.close(); });
      TASSERT (!!error && !Path::check (archive, "e"));
    }
  ProjectP project = Server::instance().create_project ("");
  TASSERT (!project->save_project ("Write Errors"));
  const String dir = project->project_dir(), target = Path::join (dir, "project.anklang");
  const String previous = Path::stringread (target);
  project->bpm (137.5);
  const Error error = with_file_size_limit (0, [&] { return project->save_project (""); });
  TASSERT (!!error && Path::stringread (target) == previous);
  StringS temporary;
  Path::glob (Path::join (Path::glob_escape (dir), ".project.anklang-*"), temporary);
  TASSERT (temporary.empty());
  TASSERT (!project->save_project (""));
  TASSERT (Path::stringread (target) != previous);
  project->discard();
}
TEST_ADD (project_save_close_errors);

static void
project_first_save_errors()
{
  ProjectFiles files;
  const String sample = anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg");
  for (bool save_as : { false, true })
    for (bool with_clip : { false, true })
      for (bool retry : { false, true })
        {
          ProjectP project = Server::instance().create_project ("Before");
          ClipP clip = with_clip ? project->create_track()->insert_wave_clip ("Sample", sample, 0) : nullptr;
          TASSERT (!with_clip || clip);
          const String old_dir = project->project_dir(), old_name = project->name();
          const String dest = Path::join (files.root, "Failed");
          uint name_notifications = 0;
          auto name_connection = project->on_event ("notify:name", [&] (const Event&) { name_notifications++; });
          const Error error = with_file_size_limit (64, [&] {
            return save_as ? project->save_project_as (dest) : project->save_project ("Failed");
          });
          TASSERT (!!error && project->unnamed());
          TASSERT (name_notifications == 0);
          TASSERT (project->project_dir() == old_dir && project->name() == old_name && !Path::check (dest, "e"));
          if (clip)
            TASSERT (Path::check (clip->original_file(), "f") && Path::equals (clip->original_file(), sample));
          if (retry)
            {
              TASSERT (!(save_as ? project->save_project_as (dest) : project->save_project ("Failed")));
              TASSERT (!project->unnamed() && project->name() == "Failed" && name_notifications == 1);
              TASSERT (saved_xml (project->project_dir()).find ("name=\"Failed\"") != String::npos);
            }
          const String discard_dir = project->project_dir();
          TASSERT (!project->remove_project_dir() && project->project_dir().empty());
          TASSERT (discard_dir.empty() || !Path::check (discard_dir, "e"));
          project->discard();
        }
}
TEST_ADD (project_first_save_errors);

static void
project_unnamed_directory()
{
  ProjectFiles files;
  ProjectImplP project = ProjectImpl::create ("");
  TASSERT (project->save_project ("") == Error::INVALID_PROPERTY);
  TASSERT (project->project_dir().empty());
  TASSERT (project->project_dir (true) == Path::join (files.root, "Unnamed-1"));
  TASSERT (project->unnamed());
  TASSERT (Path::check (Path::join (project->project_dir(), ".anklang.project"), "f"));
  TASSERT (!project->remove_project_dir() && project->project_dir().empty());
  TASSERT (!Path::check (Path::join (files.root, "Unnamed-1"), "e"));
  const String old_dir = project->project_dir (true);
  const String existing = Path::join (files.root, "Chosen");
  TASSERT (Path::mkdirs (existing));
  TASSERT (!project->can_undo());
  uint name_notifications = 0;
  auto name_connection = project->on_event ("notify:name", [&] (const Event&) { name_notifications++; });
  TASSERT (!project->save_project ("Chosen"));
  TASSERT (!project->can_undo() && name_notifications == 1);
  const String dir = project->project_dir();
  TASSERT (dir == existing + "(1)" && !project->unnamed());
  TASSERT (!Path::check (old_dir, "e"));
  TASSERT (project->name() == "Chosen");
  project->name ("Edit Name");
  TASSERT (!project->save_project ("Ignored"));
  TASSERT (project->name() == "Edit Name" && project->project_dir() == dir);
  project->discard();
  std::weak_ptr<ProjectImpl> weak = project;
  project.reset();
  TASSERT (weak.expired());
  TASSERT (Path::check (Path::join (dir, "project.anklang"), "f"));
  TASSERT (!Server::instance().open_project (dir));
  ProjectP opened = Server::instance().last_project();
  TASSERT (opened->name() == "Edit Name");
  opened->discard();
  ProjectImplP untitled = ProjectImpl::create ("Untitled");
  const String unnamed = untitled->project_dir (true);
  TASSERT (unnamed == Path::join (files.root, "Unnamed-1") && untitled->unnamed());
  untitled->discard();
  untitled.reset();
  TASSERT (Path::check (Path::join (unnamed, ".anklang.project"), "f"));
  TASSERT (Path::mkdirs (Path::join (files.root, "Direct")));
  ProjectP direct = Server::instance().create_project ("Direct");
  TASSERT (!direct->save_project ("Direct"));
  TASSERT (direct->project_dir() == Path::join (files.root, "Direct(1)"));
  TASSERT (!Path::check (Path::join (files.root, "Direct(2)"), "e"));
  direct->discard();
}
TEST_ADD (project_unnamed_directory);

static void
project_dir_clip_files()
{
  ProjectFiles files;
  ProjectP project = Server::instance().create_project ("");
  const String sample = anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg");
  ClipP clip = project->create_track()->insert_wave_clip ("Linked Piano", sample, 0);
  TASSERT (clip && project->unnamed());
  const String old_dir = project->project_dir();
  uint notifications = 0;
  auto connection = clip->on_event ("notify:original_file", [&] (const Event&) { notifications++; });
  TASSERT (!project->save_project ("Renamed Clips"));
  const String dir = project->project_dir();
  TASSERT (notifications == 1 && dir != old_dir && !Path::check (old_dir, "e"));
  TASSERT (clip->original_file() == Path::join (dir, "samples", Path::basename (sample)));
  TASSERT (Path::equals (clip->original_file(), sample));
  TASSERT (!project->remove_project_dir());
  TASSERT (notifications == 2 && clip->original_file().empty());
  project->discard();
}
TEST_ADD (project_dir_clip_files);

static void
project_save_as()
{
  ProjectFiles files;
  ProjectP project = Server::instance().create_project ("");
  const String sample = anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg");
  TrackP track = project->create_track();
  ClipP clip = track->insert_wave_clip ("Linked Piano", sample, 0);
  TASSERT (clip && project->unnamed());
  const String unnamed_dir = project->project_dir(), copy = Path::join (files.root, "elsewhere", "Copy");
  TASSERT (project->save_project_as (Path::join (files.root, "Unnamed-7")) == Error::INVALID_PROPERTY);
  TASSERT (!project->save_project_as (copy));
  TASSERT (project->project_dir() == copy && !project->unnamed() && project->name() == "Copy");
  TASSERT (Path::check (Path::join (copy, "project.anklang"), "f") && Path::check (Path::join (copy, ".anklang.project"), "f"));
  TASSERT (!Path::check (unnamed_dir, "e"));
  TASSERT (clip->original_file() == Path::join (copy, "samples", Path::basename (sample)));
  TASSERT (Path::equals (clip->original_file(), sample));
  const String hidden_sample = Path::join (copy, ".media", "take.ogg");
  TASSERT (Path::mkdirs (Path::dirname (hidden_sample)) && Path::copy_file (sample, hidden_sample));
  ClipP hidden = track->insert_wave_clip ("Hidden", hidden_sample, 1);
  TASSERT (hidden && hidden->original_file() == hidden_sample);
  for (const String &name : { ".settings", "..settings" })
    TASSERT (Path::stringwrite (Path::join (copy, name), "hidden data"));
  TASSERT (chmod (hidden_sample.c_str(), 0640) == 0);
  TASSERT (Path::stringwrite (Path::join (copy, ".media", "project.anklang"), "nested asset"));
  TASSERT (Path::stringwrite (Path::join (copy, ".media", "backup", "asset"), "nested backup", true));
  for (const String &target : StringS { ".media", "missing" })
    TASSERT (symlink (target.c_str(), Path::join (copy, target + "-link").c_str()) == 0);
  TASSERT (!project->save_project ("Ignored") && Path::check (Path::join (copy, "backup"), "d"));
  const String second = Path::join (files.root, "Second");
  TASSERT (!project->save_project_as (second));
  TASSERT (project->name() == "Copy" && project->project_dir() == second);
  TASSERT (hidden->original_file() == Path::join (second, ".media", "take.ogg"));
  TASSERT (Path::stringread (hidden->original_file()) == Path::stringread (sample));
  for (const String &name : { ".settings", "..settings" })
    TASSERT (Path::stringread (Path::join (second, name)) == "hidden data");
  struct stat copied = {};
  TASSERT (stat (hidden->original_file().c_str(), &copied) == 0 && (copied.st_mode & 0777) == 0640);
  TASSERT (Path::stringread (Path::join (second, ".media", "project.anklang")) == "nested asset");
  TASSERT (Path::stringread (Path::join (second, ".media", "backup", "asset")) == "nested backup");
  for (const String &target : StringS { ".media", "missing" })
    {
      char buffer[64];
      const ssize_t length = readlink (Path::join (second, target + "-link").c_str(), buffer, sizeof (buffer));
      TASSERT (length == ssize_t (target.size()) && String (buffer, length) == target);
    }
  TASSERT (Path::stringwrite (Path::join (second, ".settings"), "changed copy"));
  TASSERT (Path::stringread (Path::join (copy, ".settings")) == "hidden data");
  TASSERT (!Path::check (Path::join (second, "backup"), "e") && Path::check (Path::join (copy, "project.anklang"), "f"));
  TASSERT (project->save_project_as (copy) == Error::FILE_EXISTS && project->project_dir() == second);
  const size_t ntracks = project->all_tracks().size();
  project->discard();
  TASSERT (!Server::instance().open_project (copy));
  ProjectP opened = Server::instance().last_project();
  TASSERT (opened->name() == "Copy" && opened->all_tracks().size() == ntracks);
  opened->discard();
}
TEST_ADD (project_save_as);

static void
project_open_errors()
{
  ProjectFiles files;
  Server &server = Server::instance();
  const ProjectP last = server.last_project();
  const String dir = Path::join (files.root, "Errors"), target = Path::join (dir, "project.anklang");
  TASSERT (server.open_project (Path::join (files.root, "Missing")) == Error::FILE_NOT_FOUND);
  TASSERT (server.open_project (Path::join (files.root, "Missing/project.anklang")) == Error::FILE_NOT_FOUND);
  TASSERT (server.last_project() == last);
  write_project (dir, "<EDIT/>", "text/plain");
  TASSERT (server.open_project (dir) == Error::BAD_PROJECT);
  write_project (dir, "<EDIT/>");
  String data = Path::stringread (target);
  TASSERT (Path::stringwrite (target, data.substr (0, data.size() / 2)));
  TASSERT (!!server.open_project (target));
  for (const String &xml : StringS { "", "<EDIT>", "<WRONG/>" })
    {
      write_project (dir, xml);
      TASSERT (server.open_project (dir) == Error::FORMAT_INVALID);
      TASSERT (server.last_project() == last);
    }
  write_project (dir, "<EDIT name=\"No ID\"/>");
  TASSERT (!server.open_project (dir));
  ProjectP project = server.last_project();
  TASSERT (project->name() == "No ID" && !project->can_undo());
  const String unrelated = Path::join (dir, "keep.txt");
  TASSERT (Path::stringwrite (unrelated, "unrelated file"));
  TASSERT (project->remove_project_dir() == Error::BAD_PROJECT);
  TASSERT (Path::stringread (unrelated) == "unrelated file" && Path::check (target, "f"));
  TASSERT (!project->grep_serialized ("projectID=\"([^\"]+)\"", 1).empty());
  TASSERT (!project->save_project (""));
  TASSERT (saved_xml (dir).find ("projectID=") != String::npos);
  project->discard();
}
TEST_ADD (project_open_errors);

static void
project_missing_vst3()
{
  ProjectFiles files;
  const String dir = Path::join (files.root, "Missing VST3");
  const String xml = "<EDIT name=\"Missing VST3\"><TRACK id=\"1001\" name=\"Synth\">"
    "<PLUGIN type=\"vst\" enabled=\"1\" name=\"Unavailable Synth\" filename=\"/missing/synth.vst3\" "
    "uniqueId=\"12345678\" uid=\"12345678\" manufacturer=\"Test\" format=\"VST3\" state=\"saved-state\"/>"
    "</TRACK></EDIT>";
  write_project (dir, xml);
  TASSERT (!Server::instance().open_project (dir));
  ProjectP project = Server::instance().last_project();
  const String element = project->grep_serialized ("<PLUGIN[^>]*name=\"Unavailable Synth\"[^>]*");
  TASSERT (!element.empty() && element.find ("saved-state") != String::npos);
  TASSERT (!project->save_project (""));
  TASSERT (saved_xml (dir).find (element) != String::npos);
  project->discard();
  TASSERT (!Server::instance().open_project (dir));
  project = Server::instance().last_project();
  TASSERT (project->grep_serialized ("<PLUGIN[^>]*name=\"Unavailable Synth\"[^>]*") == element);
  project->discard();
}
TEST_ADD (project_missing_vst3);

}
