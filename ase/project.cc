// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "project.hh"
#include "jsonipc/jsonipc.hh"
#include "utils.hh"
#include "device.hh"
#include "processor.hh"
#include "path.hh"
#include "serialize.hh"
#include "storage.hh"
#include "server.hh"
#include "internal.hh"

namespace Ase {

// == ProjectImpl ==
JSONIPC_INHERIT (ProjectImpl, Project);

static std::vector<ProjectImplP> all_projects;

ProjectImpl::ProjectImpl()
{
  if (tracks_.empty())
    create_track (); // ensure Master track
}

ProjectImpl::~ProjectImpl()
{}

ProjectImplP
ProjectImpl::create (const String &projectname)
{
  ProjectImplP project = ProjectImpl::make_shared();
  all_projects.push_back (project);
  project->name (projectname);
  return project;
}

void
ProjectImpl::destroy ()
{
  stop_playback();
  const size_t nerased = Aux::erase_first (all_projects, [this] (auto ptr) { return ptr.get() == this; });
  if (nerased)
    ; // resource cleanups...
}

static bool
is_anklang_dir (const String path)
{
  return Path::check (Path::join (path, ".anklang.project"), "r");
}

static bool
make_anklang_dir (const String path)
{
  String mime = Path::join (path, ".anklang.project");
  return Path::stringwrite (mime, "# ANKLANG(1) project directory\n");
}

Error
ProjectImpl::save_dir (const String &pdir, bool selfcontained)
{
  const String dotanklang = ".anklang";
  String projectfile;
  String path = Path::normalize (Path::abspath (pdir));
  // check path
  if (Path::check (path, "d"))                  // existing directory
    path = Path::dir_terminate (path);
  else if (Path::check (path, "e"))             // existing file
    {
      String dir = Path::dirname (path);
      if (!is_anklang_dir (dir))
        return ase_error_from_errno (ENOTDIR);
      projectfile = Path::basename (path);
      path = dir;                               // file inside project dir
    }
  else                                          // new file name
    {
      while (path.back() == '/')                // strip trailing slashes
        path = Path::dirname (path);
      if (string_endswith (path, dotanklang))   // strip .anklang
        path.resize (path.size() - dotanklang.size());
      projectfile = Path::basename (path);
      const String parentdir = Path::dirname (path);
      if (is_anklang_dir (parentdir))
        path = parentdir;
      else if (!is_anklang_dir (path) &&
          !Path::mkdirs (path))                 // create new project dir
        return ase_error_from_errno (errno);
    }
  if (!make_anklang_dir (path))
    return ase_error_from_errno (errno);
  // here, path is_anklang_dir
  if (projectfile.empty())
    projectfile = "project";
  if (!string_endswith (projectfile, dotanklang))
    projectfile += dotanklang;
  StorageWriter ws (Storage::AUTO_ZSTD);
  Error error = ws.open_with_mimetype (Path::join (path, projectfile), "application/x-anklang");
  if (!error)
    {
      // serialize Project
      String jsd = json_stringify (*this, Writ::INDENT);
      jsd += '\n';
      error = ws.store_file_data ("project.json", jsd);
    }
  if (!error)
    error = ws.close();
  if (!!error)
    ws.remove_opened();
  return error;
}

Error
ProjectImpl::load_project (const String &filename)
{
  String fname = filename;
  // turn /dir/.anklang.project -> /dir/
  if (Path::basename (fname) == ".anklang.project" && is_anklang_dir (Path::dirname (fname)))
    fname = Path::dirname (fname);
  // turn /dir/ -> /dir/dir.anklang
  if (Path::check (fname, "d"))
    fname = Path::join (fname, Path::basename (Path::strip_slashes (Path::normalize (fname)))) + ".anklang";
  // try /dir/file
  if (!Path::check (fname, "e"))
    {
      // try /dir/file.anklang
      fname += ".anklang";
      if (!Path::check (fname, "e"))
        return ase_error_from_errno (errno);
    }
  String dirname = Path::dirname (fname);
  StorageReader rs (Storage::AUTO_ZSTD);
  // try reading .anklang container
  Error error = rs.open_for_reading (fname);
  if (!!error)
    return error;
  if (rs.stringread ("mimetype") != "application/x-anklang")
    return Error::FORMAT_INVALID;
  // find project.json *inside* container
  String jsd = rs.stringread ("project.json");
  if (jsd.empty() && errno)
    return Error::FORMAT_INVALID;
  // search in dirname or dirname/..
  if (is_anklang_dir (dirname))
    rs.search_dir (dirname);
  else
    {
      dirname = Path::dirname (dirname);
      if (is_anklang_dir (dirname))
        rs.search_dir (dirname);
    }
  // parse project
  if (!json_parse (jsd, *this))
    return Error::PARSE_ERROR;
  return Error::NONE;
}

void
ProjectImpl::serialize (WritNode &xs)
{
  GadgetImpl::serialize (xs);
  // save tracks
  if (xs.in_save())
    for (auto &trackp : tracks_)
      {
        const bool True = true;
        WritNode xc = xs["tracks"].push();
        xc & *trackp;
        if (trackp == tracks_.back())           // master_track
          xc.front ("mastertrack") & True;
      }
  // load tracks
  if (xs.in_load())
    for (auto &xc : xs["tracks"].to_nodes())
      {
        TrackImplP trackp = tracks_.back();     // master_track
        if (!xc["mastertrack"].as_int())
          trackp = shared_ptr_cast<TrackImpl> (create_track());
        xc & *trackp;
      }
}

TelemetryFieldS
ProjectImpl::telemetry () const
{
  TelemetryFieldS v;
  AudioProcessorP proc = master_processor ();
  assert_return (proc, v);
  const AudioTransport &transport = proc->transport();
  v.push_back (telemetry_field ("bpm", &transport.current_bpm));
  v.push_back (telemetry_field ("tick_pos", &transport.current_tick));
  // v.push_back (telemetry_field ("sample_frames", &transport.sample_frames));
  return v;
}

AudioProcessorP
ProjectImpl::master_processor () const
{
  return_unless (!tracks_.empty(), nullptr);
  TrackP master = const_cast<ProjectImpl*> (this)->master_track();
  return_unless (master, nullptr);
  DeviceP device = master->access_device();
  return_unless (device, nullptr);
  AudioProcessorP proc = dynamic_cast<DeviceImpl*> (device.get())->audio_processor();
  return_unless (proc, nullptr);
  return proc;
}

void
ProjectImpl::start_playback ()
{
  AudioProcessorP proc = master_processor();
  return_unless (proc);
  const double bpm = 90;
  auto job = [proc, bpm] () {
    AudioTransport &transport = const_cast<AudioTransport&> (proc->engine().transport());
    transport.tempo (bpm);
  };
  proc->engine().async_jobs += job;
}

void
ProjectImpl::stop_playback ()
{
  AudioProcessorP proc = master_processor();
  return_unless (proc);
  auto job = [proc] () {
    AudioTransport &transport = const_cast<AudioTransport&> (proc->engine().transport());
    if (transport.current_bpm == 0)
      transport.tickto (0);
    else
      transport.tempo (0);
  };
  proc->engine().async_jobs += job;
}

bool
ProjectImpl::is_playing ()
{
  AudioProcessorP proc = master_processor();
  return_unless (proc, false);
  return proc->engine().transport().current_bpm > 0.0;
}

TrackP
ProjectImpl::create_track ()
{
  const bool havemaster = tracks_.size() != 0;
  TrackImplP track = TrackImpl::make_shared (!havemaster);
  tracks_.insert (tracks_.end() - int (havemaster), track);
  emit_event ("track", "insert", { { "track", track }, });
  track->set_project (this);
  return track;
}

bool
ProjectImpl::remove_track (Track &child)
{
  TrackImplP track = shared_ptr_cast<TrackImpl> (&child);
  return_unless (track && !track->is_master(), false);
  if (!Aux::erase_first (tracks_, [track] (TrackP t) { return t == track; }))
    return false;
  // destroy Track
  track->set_project (nullptr);
  emit_event ("track", "remove");
  return true;
}

TrackS
ProjectImpl::list_tracks ()
{
  TrackS tracks (tracks_.size());
  std::copy (tracks_.begin(), tracks_.end(), tracks.begin());
  return tracks;
}

ssize_t
ProjectImpl::track_index (const Track &child) const
{
  for (size_t i = 0; i < tracks_.size(); i++)
    if (&child == tracks_[i].get())
      return i;
  return -1;
}

TrackP
ProjectImpl::master_track ()
{
  assert_return (!tracks_.empty(), nullptr);
  return tracks_.back();
}

// == Project ==
ProjectP
Project::last_project()
{
  return all_projects.empty() ? nullptr : all_projects.back();
}

} // Ase
