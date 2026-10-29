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

static bool
asset_reference_valid (const String &reference)
{
  return Path::isportable_relpath (reference) && !te::ProjectItemID (juce::String (reference)).isValid();
}

String
ProjectImpl::resolve_asset (const String &reference) const
{
  return_unless (asset_reference_valid (reference), "");
  if (!project_dir_.empty())
    {
      const String file = Path::join (project_dir_, reference);
      if (Path::check (file, "fr"))
        return file;
    }
  return Path::find_contained (asset_roots(), reference);
}

String
ProjectImpl::admit_asset (const String &file, bool &created)
{
  created = false;
  String relative;
  auto reference_in = [&] (const String &root) {
    return !root.empty() && Path::dircontains (root, file, &relative) && asset_reference_valid (relative) &&
           juce::CharPointer_UTF8::isValidString (file.c_str(), file.size()) &&
           Path::equals (resolve_asset (relative), file);
  };
  if (reference_in (project_dir_))
    return relative;
  for (const String &root : asset_roots())
    if (reference_in (root))
      return relative;
  const String dir = project_dir (true);
  return_unless (!dir.empty(), "");
  const String samples = Path::join (dir, "samples");
  String name = encodefs (Path::basename (file));
  std::replace (name.begin(), name.end(), '\\', '_');
  const StringPair parts = Path::split_extension (name, true);
  if (Path::mkdirs (samples))
    for (uint i = 0; ; i++)
      {
        const String reference = Path::join ("samples", i ? string_format ("%s(%u)%s", parts.first, i, parts.second) : name);
        const String link = Path::join (dir, reference);
        if (Path::equals (link, file))
          return reference;
        if (symlink (file.c_str(), link.c_str()) == 0)
          {
            created = true;
            return reference;
          }
        if (errno != EEXIST)
          break;
      }
  warning ("Project: failed to link %s: %s", file, strerror (errno));
  return "";
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
  const String file = Path::realpath (Path::abspath (decodefs (utf8filename)));
  return_unless (!utf8filename.empty() && Path::check (file, "fr"), Error::FILE_NOT_FOUND);
  CoalesceNotifies coalesce_notifies;
  auto undo_scope = this->undo_scope (_("Replace Asset"));
  bool created = false;
  const String replacement = admit_asset (file, created);
  return_unless (!replacement.empty(), Error::FILE_WRITE_FAILED);
  const String resolved = resolve_asset (replacement);
  te::AudioFile audiofile (edit_->engine, juce::File (resolved));
  if (created)
    edit_->engine.getAudioFileManager().forceFileUpdate (audiofile);
  if (!audiofile.isValid())
    {
      if (created)
        unlink (resolved.c_str());
      return Error::FORMAT_INVALID;
    }
  for (auto clip : clips)
    clip->getSourceFileReference().source = juce::String (replacement);
  if (replacement == reference)
    asset_paths_changed();
  return Error::NONE;
}

} // Ase
