// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/* Asset resolution requirements. Implementation is pending.
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
