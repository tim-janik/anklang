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
{}

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
