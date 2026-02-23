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

} // Anon
