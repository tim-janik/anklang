// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"
#include "project.hh"
#include "main.hh"
#include "path.hh"
#include "strings.hh"
#include "unicode.hh"
#include "internal.hh"
#include <unistd.h>
#include <set>

/* Asset resolution requirements.
 *
 * Project ownership, location and lifetime
 * - Each project needs its own on-disk directory for state and working files.
 *   After creating a new project, the first write creates that directory.
 *   A project that was never saved gets Unnamed-1, Unnamed-2, etc in the
 *   projects root, where users also find it after a crash. Its first save asks
 *   for a name.
 * - Choose the projects root in this order:
 *   1. ANKLANG_PROJECTS_DIR from the environment.
 *   2. The user's anklang-projects-dir setting.
 *   3. XDG_PROJECTS_DIR from the environment.
 *   4. ${XDG_DOCUMENTS_DIR-~/Documents}/Anklang.
 *   Skip unset or empty choices. Create a distinct directory per project;
 *   never overwrite another project because its name matches, append '(1)',
 *   '(2)', etc to the basename.
 * - Projectdirs are persistent user data. Closing a project, discarding its
 *   object or exiting Anklang must not delete them. Cache purging must not touch
 *   them. ~/.cache/anklang is for disposable, reproducible data only.
 * - discard() is object bookkeeping and never does file IO. Only an explicit
 *   remove_projectdir() deletes a projectdir, never a destructor. On a normal
 *   exit, the user either saves an unnamed project, giving it a name, or its
 *   projectdir gets removed with remove_projectdir().
 * - Program start never blocks on a recovery prompt or anything like it.
 * - Purging old Unnamed projectdirs is unresolved and optional, for later.
 * - Changing the projects root affects new projects, not existing directories.
 *
 * Save, restore, import and export
 * - Save writes the current state as XML and any required plugin blobs into
 *   the projectdir. It is a fast local operation, without packing assets.
 *   Save must preserve the previous usable state if writing fails.
 * - Open or restore an existing local project from its persistent projectdir.
 *   An existing project's Edit is not reloaded; construct a new project object.
 * - Import a .anklang file, other project format or URL into a fresh projectdir
 *   under the projects root. Extract embedded assets there. The imported source
 *   remains separate from the working project and is not its save destination.
 * - Export writes a .anklang file anywhere the user chooses, optionally
 *   embedding assets. Writing a bitwig/dawproject format is also export.
 *   By default, the export location is:
 *   1. The user's anklang-export-dir setting.
 *   2. XDG_MUSIC_DIR from the environment.
 *   Export does not move the projectdir or change where Save writes state.
 * - Exported projects contain XML, blobs and file references, never filesystem
 *   symlink entries. Reopening an exported .anklang file is another import.
 * - Autosave snapshots live within the persistent projectdir. They must survive
 *   exit and project object destruction so a later restore can recover them.
 * - External assets selected by the user can be symlinked into the local
 *   projectdir without copying their bytes. These links are local bookkeeping;
 *   their targets must not be restored from an imported archive. Opening a
 *   local projectdir trusts its links, however the directory got there.
 *
 * References and resolution
 * - Preserve the stored reference separately from its current resolution.
 * - Imported asset references must be relative. Absolute paths and any path
 *   component equal to ".." stay unresolved, including leading or trailing
 *   components. Recognize Windows absolute paths and separators as well.
 * - Resolve allowed relative references to the first match when searching:
 *   1. The projectdir.
 *   2. anklang_factory_dir, the sample library shipped with Anklang.
 *   3. ANKLANG_SAMPLE_DIRS from the environment.
 *   4. The user's anklang-sample-dirs setting.
 *   5. SFZ_PATH and SF2_PATH from the environment, which Carla also reads.
 *   Lists are separated by ':', or ';' on Windows.
 *   Absolute library roots belong to local configuration, never the project.
 * - Preserve absolute, invalid and missing references and show them to the user
 *   as unresolved assets. Their text is not permission to open those paths.
 * - Choosing a replacement explicitly admits that source into the project.
 * - Tracktion project item IDs point to catalogue entries containing file paths.
 *   Apply the same path checks to those paths before opening any files.
 *
 * Round trips and embedding
 * - Preserve unresolved references unchanged through save, export and import,
 *   including invalid ones.
 * - Retry resolution on import and local restore so assets recover when a
 *   temporarily unmounted library returns.
 * - When exporting with embedding enabled, embed available, approved assets and
 *   retain the references for unavailable assets. Without embedding, export
 *   state and references without copying external asset bytes.
 * - Embedding must use the same approved resolutions as playback. It must not
 *   independently open arbitrary filenames found in imported state.
 *
 */

namespace te = tracktion::engine;
namespace Ase {

static Preference sample_dirs_pref ({
  .ident = "anklang-sample-dirs", .label = _("Sample Directories"), .initial = "", .hints = STANDARD,
});

static StringS
asset_roots()
{
  StringS roots { anklang_runpath (RPath::SAMPLEDIR) };
  for (const String &list : StringS { getenv ("ANKLANG_SAMPLE_DIRS") ?: "", decodefs (sample_dirs_pref.gets()),
                                    getenv ("SFZ_PATH") ?: "", getenv ("SF2_PATH") ?: "" })
    for (const String &root : Path::searchpath_split (list))
      if (const String dir = Path::expand_tilde (root); Path::isabs (dir))
        roots.push_back (dir);
  return roots;
}

String
ProjectImpl::resolve_asset (const String &reference) const
{
  // Tracktion reads project item IDs without the resolver, they never resolve without Tracktion projects
  return_unless (Path::isportable_relpath (reference) && !te::ProjectItemID (juce::String (reference)).isValid(), "");
  if (!project_dir_.empty())
    {
      const String file = Path::join (project_dir_, reference);
      if (Path::check (file, "fr"))
        return file;
    }
  return Path::find_contained (asset_roots(), reference);
}

/// Admit the audio file `utf8filename` by its path in the project dir or a library, else through a link.
Error
ProjectImpl::admit_asset (const String &utf8filename, String &reference, bool *created)
{
  const String file = Path::realpath (Path::abspath (decodefs (utf8filename)));
  return_unless (!utf8filename.empty() && Path::check (file, "fr"), Error::FILE_NOT_FOUND);
  StringS roots = asset_roots();
  roots.insert (roots.begin(), project_dir_);
  reference.clear();
  if (juce::CharPointer_UTF8::isValidString (file.c_str(), file.size())) // other names get a link with an encoded name
    for (const String &root : roots)
      if (String relative; Path::dircontains (root, file, &relative) && Path::equals (resolve_asset (relative), file))
        {
          reference = relative;
          break;
        }
  bool linked = false;
  if (reference.empty())
    reference = link_sample (file, linked);
  return_unless (!reference.empty(), Error::FILE_WRITE_FAILED);
  const String resolved = resolve_asset (reference);
  te::AudioFile audiofile (edit_->engine, juce::File (resolved));
  if (linked)
    edit_->engine.getAudioFileManager().forceFileUpdate (audiofile);
  if (!audiofile.isValid())
    {
      if (linked)
        unlink (resolved.c_str());
      return Error::FORMAT_INVALID;
    }
  if (created)
    *created = linked;
  return Error::NONE;
}

StringS
ProjectImpl::unresolved_assets () const
{
  std::set<String> unresolved;
  if (edit_)
    edit_->clipCache.visitItems ([&] (te::Clip *clip) {
      const String reference = clip->getSourceFileReference().source.get().toStdString();
      if (!reference.empty() && resolve_asset (reference).empty())
        unresolved.insert (reference);
    });
  return { unresolved.begin(), unresolved.end() };
}

void
ProjectImpl::unresolved_assets (const StringS&)
{
  warning ("Project.unresolved_assets is read-only");
}

Error
ProjectImpl::replace_asset (const String &reference, const String &utf8filename)
{
  return_unless (edit_ && !reference.empty(), Error::INVALID_PROPERTY);
  std::vector<te::Clip*> clips;
  edit_->clipCache.visitItems ([&] (te::Clip *clip) {
    if (clip->getSourceFileReference().source.get().toStdString() == reference)
      clips.push_back (clip);
  });
  return_unless (!clips.empty(), Error::INVALID_PROPERTY);
  CoalesceNotifies coalesce_notifies;
  // admit_asset() may create the project dir, Tracktion's clip refresh for it belongs in this undo step
  auto undo_scope = this->undo_scope (_("Replace Asset"));
  String replacement;
  if (const Error error = admit_asset (utf8filename, replacement); !!error)
    return error;
  for (auto clip : clips)
    clip->getSourceFileReference().source = juce::String (replacement);
  if (replacement == reference) // the file is back, but the unchanged source tells Tracktion nothing
    asset_paths_changed();
  return Error::NONE;
}

} // Ase
