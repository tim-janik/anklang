// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "project.hh"
#include "jsonipc/jsonipc.hh"
#include "utils.hh"
#include "path.hh"
#include "serialize.hh"
#include "internal.hh"

namespace Ase {

// == ProjectImpl ==
JSONIPC_INHERIT (ProjectImpl, Project);

static std::vector<ProjectImplP> all_projects;

ProjectImpl::ProjectImpl (const String &projectname)
{
  if (tracks_.empty())
    create_track (); // ensure Master track
}

ProjectImpl::~ProjectImpl()
{}

ProjectImplP
ProjectImpl::create (const String &projectname)
{
  ProjectImplP project = ProjectImpl::make_shared (projectname);
  all_projects.push_back (project);
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
  String mime = Path::stringread (Path::join (path, "mimetype"));
  return mime == "application/x-ase";
}

static bool
make_anklang_dir (const String path)
{
  String mime = Path::join (path, "mimetype");
  return Path::stringwrite (mime, "application/x-ase");
}

Error
ProjectImpl::save_dir (const String &pdir, bool selfcontained)
{
  const String postfix = ".anklang";
  String path = Path::normalize (Path::abspath (pdir));
  // check path
  if (Path::check (path, "d"))                  // existing directory
    path = Path::dir_terminate (path);
  else if (Path::check (path, "e"))             // file name
    {
      String dir = Path::dirname (path);
      if (!is_anklang_dir (dir))
        return ase_error_from_errno (ENOTDIR);
      path = dir;                               // file inside project dir
    }
  else                                          // new name
    {
      if (path.back() == '/')                   // strip trailing slashes
        path = Path::dirname (path);
      if (string_endswith (path, postfix))      // strip .anklang
        path.resize (path.size() - postfix.size());
      if (!is_anklang_dir (path) &&
          !Path::mkdirs (path))                 // create new project dir
        return ase_error_from_errno (errno);
    }
  if (!make_anklang_dir (path))
    return ase_error_from_errno (errno);
  // here, path is_anklang_dir
  // serialize Project
  String jsd = json_stringify (*this, Writ::INDENT);
  jsd += '\n';
  if (!Path::stringwrite (Path::join (path, "project.anklang"), jsd))
    return ase_error_from_errno (errno);
  jsd.clear();
  // cleanup
  return Ase::Error::NONE;
}

Error
ProjectImpl::load_project (const String &filename)
{
  if (!Path::check (filename, "e"))
    return ase_error_from_errno (errno);
  String basedir = Path::check (filename, "d") ? filename : Path::dirname (filename);
  return_unless (is_anklang_dir (basedir), ase_error_from_errno (errno));
  String jsd = Path::stringread (Path::join (basedir, "project.anklang"));
  if (jsd.empty() && errno)
    return ase_error_from_errno (errno);
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

void
ProjectImpl::start_playback ()
{
  // TODO: implement playback
}

void
ProjectImpl::stop_playback ()
{
  // TODO: implement playback
}

bool
ProjectImpl::is_playing ()
{
  return false;
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
