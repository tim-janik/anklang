// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "project.hh"
#include "jsonipc/jsonipc.hh"
#include "utils.hh"
#include "internal.hh"

namespace Ase {

// == ProjectImpl ==
JSONIPC_INHERIT (ProjectImpl, Project);

static std::vector<ProjectImplP> projects;

ProjectImpl::ProjectImpl (const String &projectname)
{
  if (tracks_.empty())
    create_track (); // ensure Master track
}

ProjectImpl::~ProjectImpl()
{}

void
ProjectImpl::destroy ()
{
  stop_playback();
  const size_t nerased = Aux::erase_first (projects, [this] (auto ptr) { return ptr.get() == this; });
  if (nerased)
    ; // resource cleanups...
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
  return projects.empty() ? nullptr : projects.back();
}

ProjectP
Project::create (const String &projectname)
{
  ProjectImplP project = std::make_shared<ProjectImpl> (projectname);
  projects.push_back (project);
  return project;
}

} // Ase
