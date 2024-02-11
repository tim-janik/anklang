// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "project.hh"
#include "jsonipc/jsonipc.hh"
#include "main.hh"
#include "processor.hh"
#include "compress.hh"
#include "path.hh"
#include "serialize.hh"
#include "storage.hh"
#include "server.hh"
#include "internal.hh"

#define UDEBUG(...)     Ase::debug ("undo", __VA_ARGS__)

namespace Ase {

static std::vector<ProjectImplP> &all_projects = *new std::vector<ProjectImplP>();

// == Project ==
ProjectP
Project::last_project()
{
  return all_projects.empty() ? nullptr : all_projects.back();
}

// == ProjectImpl ==
JSONIPC_INHERIT (ProjectImpl, Project);

using StringPairS = std::vector<std::tuple<String,String>>;

struct ProjectImpl::PStorage {
  String loading_file;
  String writer_cachedir;
  String anklang_dir;
  StringPairS writer_files;
  StringPairS asset_hashes;
  PStorage (PStorage **ptrp) :
    ptrp_ (ptrp)
  {
    *ptrp_ = this;
  }
  ~PStorage()
  {
    *ptrp_ = nullptr;
  }
private:
  PStorage **const ptrp_ = nullptr;
};

ProjectImpl::ProjectImpl()
{
  if (tracks_.empty())
    create_track (); // ensure Master track
  tick_sig_.set_bpm (90);

  if (0)
    autoplay_timer_ = main_loop->exec_timer ([this] () {
      return_unless (autoplay_timer_, false);
      autoplay_timer_ = 0;
      if (!is_playing())
        start_playback();
      return false;
    }, 500);
}

ProjectImpl::~ProjectImpl()
{
  main_loop->clear_source (&autoplay_timer_);
}


ProjectImplP
ProjectImpl::create (const String &projectname)
{
  ProjectImplP project = ProjectImpl::make_shared();
  all_projects.push_back (project);
  project->name (projectname);
  return project;
}

void
ProjectImpl::discard ()
{
  return_unless (!discarded_);
  stop_playback();
  const size_t nerased = Aux::erase_first (all_projects, [this] (auto ptr) { return ptr.get() == this; });
  if (nerased)
    {} // resource cleanups...
  discarded_ = true;
}

void
ProjectImpl::_activate ()
{
  assert_return (!is_active());
  DeviceImpl::_activate();
  for (auto &track : tracks_)
    track->_activate();
}

void
ProjectImpl::_deactivate ()
{
  assert_return (is_active());
  for (auto trackit = tracks_.rbegin(); trackit != tracks_.rend(); ++trackit)
    (*trackit)->_deactivate();
  DeviceImpl::_deactivate();
}

static bool
is_anklang_dir (const String &path)
{
  return Path::check (Path::join (path, ".anklang.project"), "r");
}

static String
find_anklang_parent_dir (const String &path)
{
  for (String p = path; !p.empty() && !Path::isroot (p); p = Path::dirname (p))
    if (is_anklang_dir (p))
      return p;
  return "";
}

static bool
make_anklang_dir (const String &path)
{
  String mime = Path::join (path, ".anklang.project");
  return Path::stringwrite (mime, "# ANKLANG(1) project directory\n");
}

Error
ProjectImpl::save_project (const String &savepath, bool collect)
{
  assert_return (storage_ == nullptr, Error::OPERATION_BUSY);
  PStorage storage (&storage_); // storage_ = &storage;
  const String dotanklang = ".anklang";
  String projectfile, path = Path::normalize (Path::abspath (savepath));
  // check path is a file
  if (path.back() == '/' ||
      Path::check (path, "d"))                  // need file not directory
    return Error::FILE_IS_DIR;
  // force .anklang extension
  if (!string_endswith (path, dotanklang))
    path += dotanklang;
  // existing files need proper project directories
  if (Path::check (path, "e"))                  // existing file
    {
      const String dir = Path::dirname (path);
      if (!is_anklang_dir (dir))
        return Error::NO_PROJECT_DIR;
      projectfile = Path::basename (path);
      path = dir;                               // file inside project dir
    }
  else                                          // new file name
    {
      projectfile = Path::basename (path);
      const String parentdir = Path::dirname (path);
      if (is_anklang_dir (parentdir))
        path = parentdir;
      else {                                    // use projectfile stem as dir
        assert_return (string_endswith (path, dotanklang), Error::INTERNAL);
        path.resize (path.size() - dotanklang.size());
      }
    }
  // create parent directory
  if (!Path::mkdirs (path))
    return ase_error_from_errno (errno);
  // ensure path is_anklang_dir
  if (!make_anklang_dir (path))
    return ase_error_from_errno (errno);
  storage_->anklang_dir = path;
  const String abs_projectfile = Path::join (path, projectfile);
  // create backups
  if (Path::check (abs_projectfile, "e"))
    {
      const String backupdir = Path::join (path, "backup");
      if (!Path::mkdirs (backupdir))
        return ase_error_from_errno (errno ? errno : EPERM);
      const StringPair parts = Path::split_extension (projectfile, true);
      const String backupname = Path::join (backupdir, parts.first + now_strftime (" (%y%m%dT%H%M%S)") + parts.second);
      const String backupglob = Path::join (backupdir, parts.first + " ([0-9]*[0-9]T[0-9]*[0-9])" + parts.second);
      if (!Path::rename (abs_projectfile, backupname))
        ASE_SERVER.user_note (string_format ("## Backup failed\n%s: \\\nFailed to create backup: \\\n%s",
                                             backupname, ase_error_blurb (ase_error_from_errno (errno))));
      else // successful backup, now prune
        {
          StringS backups;
          Path::glob (backupglob, backups);
          strings_version_sort (&backups, true);
          const int bmax = 24;
          while (backups.size() > bmax)
            {
              const String bfile = backups.back();
              backups.pop_back();
              Path::rmrf (bfile);
            }
        }
    }
  // start writing
  anklang_cachedir_clean_stale();
  storage_->writer_cachedir = anklang_cachedir_create();
  storage_->asset_hashes.clear();
  StorageWriter ws (Storage::AUTO_ZSTD);
  Error error = ws.open_with_mimetype (abs_projectfile, "application/x-anklang");
  if (!error)
    {
      // serialize Project
      String jsd = json_stringify (*this, Writ::RELAXED);
      jsd += '\n';
      error = ws.store_file_data ("project.json", jsd, true);
    }
  if (!error)
    for (const auto &[path, dest] : storage_->writer_files) {
      error = ws.store_file (dest, path);
      if (!!error) {
        printerr ("%s: %s: %s: %s\n", program_alias(), __func__, path, ase_error_blurb (error));
        break;
      }
    }
  storage_->writer_files.clear();
  if (!error)
    error = ws.close();
  if (!error)
    saved_filename_ = abs_projectfile;
  if (!!error)
    ws.remove_opened();
  anklang_cachedir_cleanup (storage_->writer_cachedir);
  return error;
}

String
ProjectImpl::writer_file_name (const String &filename) const
{
  assert_return (storage_ != nullptr, "");
  assert_return (!storage_->writer_cachedir.empty(), "");
  return Path::join (storage_->writer_cachedir, filename);
}

Error
ProjectImpl::writer_add_file (const String &filename)
{
  assert_return (storage_ != nullptr, Error::INTERNAL);
  assert_return (!storage_->writer_cachedir.empty(), Error::INTERNAL);
  if (!Path::check (filename, "frw"))
    return Error::FILE_NOT_FOUND;
  if (!string_startswith (filename, storage_->writer_cachedir))
    return Error::FILE_OPEN_FAILED;
  storage_->writer_files.push_back ({ filename, Path::basename (filename) });
  return Error::NONE;
}

Error
ProjectImpl::writer_collect (const String &filename, String *hexhashp)
{
  assert_return (storage_ != nullptr, Error::INTERNAL);
  assert_return (!storage_->anklang_dir.empty(), Error::INTERNAL);
  if (!Path::check (filename, "fr"))
    return Error::FILE_NOT_FOUND;
  // determine hash of file to collect
  const String hexhash = string_to_hex (blake3_hash_file (filename));
  if (hexhash.empty())
    return ase_error_from_errno (errno ? errno : EIO);
  // resolve against existing hashes
  for (const auto &hf : storage_->asset_hashes)
    if (std::get<0> (hf) == hexhash)
      {
        *hexhashp = hexhash;
        return Error::NONE;
      }
  // file may be within project directory
  String relpath;
  if (Path::dircontains (storage_->anklang_dir, filename, &relpath))
    {
      storage_->asset_hashes.push_back ({ hexhash, relpath });
      *hexhashp = hexhash;
      return Error::NONE;
    }
  // determine unique path name
  const size_t file_size = Path::file_size (filename);
  const String basedir = storage_->anklang_dir;
  relpath = Path::join ("samples", Path::basename (filename));
  String dest = Path::join (basedir, relpath);
  size_t i = 0;
  while (Path::check (dest, "e"))
    {
      if (file_size == Path::file_size (dest))
        {
          const String althash = string_to_hex (blake3_hash_file (dest));
          if (althash == hexhash)
            {
              // found file with same hash within project directory
              storage_->asset_hashes.push_back ({ hexhash, relpath });
              *hexhashp = hexhash;
              return Error::NONE;
            }
        }
      // add counter to create unique name
      const StringPair parts = Path::split_extension (relpath, true);
      dest = Path::join (basedir, string_format ("%s(%u)%s", parts.first, ++i, parts.second));
    }
  // create parent dir
  if (!Path::mkdirs (Path::dirname (dest)))
    return ase_error_from_errno (errno);
  // copy into project dir
  const bool copied = Path::copy_file (filename, dest);
  if (!copied)
    return ase_error_from_errno (errno);
  // success
  storage_->asset_hashes.push_back ({ hexhash, relpath });
  *hexhashp = hexhash;
  return Error::NONE;
}

String
ProjectImpl::saved_filename ()
{
  return saved_filename_;
}

Error
ProjectImpl::load_project (const String &filename)
{
  assert_return (storage_ == nullptr, Error::OPERATION_BUSY);
  PStorage storage (&storage_); // storage_ = &storage;
  String fname = filename;
  // turn /dir/.anklang.project -> /dir/
  if (Path::basename (fname) == ".anklang.project" && is_anklang_dir (Path::dirname (fname)))
    fname = Path::dirname (fname);
  // turn /dir/ -> /dir/dir.anklang
  if (Path::check (fname, "d"))
    fname = Path::join (fname, Path::basename (Path::strip_slashes (Path::normalize (fname)))) + ".anklang";
  // add missing '.anklang' extension
  if (!Path::check (fname, "e"))
    fname += ".anklang";
  // check for readable file
  if (!Path::check (fname, "e"))
    return ase_error_from_errno (errno);
  // try reading .anklang container
  StorageReader rs (Storage::AUTO_ZSTD);
  Error error = rs.open_for_reading (fname);
  if (!!error)
    return error;
  if (rs.stringread ("mimetype") != "application/x-anklang")
    return Error::BAD_PROJECT;
  // find project.json *inside* container
  String jsd = rs.stringread ("project.json");
  if (jsd.empty() && errno)
    return Error::FORMAT_INVALID;
  storage_->loading_file = fname;
  storage_->anklang_dir = find_anklang_parent_dir (storage_->loading_file);
#if 0 // unimplemented
  String dirname = Path::dirname (fname);
  // search in dirname or dirname/..
  if (is_anklang_dir (dirname))
    rs.search_dir (dirname);
  else
    {
      dirname = Path::dirname (dirname);
      if (is_anklang_dir (dirname))
        rs.search_dir (dirname);
    }
#endif
  // parse project
  if (!json_parse (jsd, *this))
    return Error::PARSE_ERROR;
  saved_filename_ = storage_->loading_file;
  return Error::NONE;
}

StreamReaderP
ProjectImpl::load_blob (const String &filename)
{
  assert_return (storage_ != nullptr, nullptr);
  assert_return (!storage_->loading_file.empty(), nullptr);
  return stream_reader_zip_member (storage_->loading_file, filename);
}

String
ProjectImpl::loader_resolve (const String &hexhash)
{
  return_unless (storage_ && storage_->asset_hashes.size(), "");
  return_unless (!storage_->anklang_dir.empty(), "");
  for (const auto& [hash,relpath] : storage_->asset_hashes)
    if (hexhash == hash)
      return Path::join (storage_->anklang_dir, relpath);
  return "";
}

void
ProjectImpl::serialize (WritNode &xs)
{
  // provide asset_hashes early on
  if (xs.in_load() && storage_ && storage_->asset_hashes.empty())
    xs["filehashes"] & storage_->asset_hashes;
  // serrialize children
  DeviceImpl::serialize (xs);
  // load tracks
  if (xs.in_load())
    for (auto &xc : xs["tracks"].to_nodes())
      {
        TrackImplP trackp = tracks_.back();     // master_track
        if (!xc["mastertrack"].as_int())
          trackp = shared_ptr_cast<TrackImpl> (create_track());
        xc & *trackp;
      }
  // save tracks
  if (xs.in_save())
    {
      for (auto &trackp : tracks_)
        {
          WritNode xc = xs["tracks"].push();
          xc & *trackp;
          if (trackp == tracks_.back())           // master_track
            xc.front ("mastertrack") << true;
        }
      // store external reference hashes *after* all other objects
      if (storage_ && storage_->asset_hashes.size())
        xs["filehashes"] & storage_->asset_hashes;
    }
}

UndoScope::UndoScope (ProjectImplP projectp) :
  projectp_ (projectp)
{
  assert_return (projectp);
  projectp->undo_scopes_open_++;
}

UndoScope::~UndoScope()
{
  assert_return (projectp_->undo_scopes_open_);
  projectp_->undo_scopes_open_--;
}

void
UndoScope::operator+= (const VoidF &func)
{
  projectp_->push_undo (func);
}

UndoScope
ProjectImpl::undo_scope (const String &scopename)
{
  assert_warn (scopename != "");
  const size_t old_undo = undostack_.size();
  const size_t old_redo = redostack_.size();
  UndoScope undoscope = add_undo_scope (scopename);
  if (undostack_.size() > old_undo && redostack_.size())
    redostack_.clear();
  if ((!old_undo ^ !undostack_.size()) || (!old_redo ^ !redostack_.size()))
    emit_notify ("dirty");
  return undoscope;
}

UndoScope
ProjectImpl::add_undo_scope (const String &scopename)
{
  UndoScope undoscope (shared_ptr_cast<ProjectImpl> (this)); // undo_scopes_open_ += 1
  assert_return (scopename != "", undoscope);
  if (undo_scopes_open_ == 1 && (undo_groups_open_ == 0 || undo_group_name_.size()))
    {
      undostack_.push_back ({ nullptr, undo_group_name_.empty() ? scopename : undo_group_name_ });
      undo_group_name_ = "";
    }
  return undoscope;
}

void
ProjectImpl::push_undo (const VoidF &func)
{
  undostack_.push_back ({ func, "" });
  if (undostack_.size() == 1)
    emit_notify ("dirty");
}

void
ProjectImpl::undo ()
{
  assert_return (undo_scopes_open_ == 0 && undo_groups_open_ == 0);
  return_unless (!undostack_.empty());
  std::vector<VoidF> funcs;
  while (!undostack_.empty() && undostack_.back().func)
    {
      funcs.push_back (undostack_.back().func);
      undostack_.pop_back();
    }
  assert_return (!undostack_.empty() && undostack_.back().func == nullptr); // must contain scope name
  const String scopename = undostack_.back().name;
  UDEBUG ("Undo: steps=%d scope: %s\n", funcs.size(), scopename);
  undostack_.pop_back(); // pop scope name
  // swap undo/redo stacks, run undo steps and scope redo
  const bool redostack_was_empty = redostack_.empty();
  undostack_.swap (redostack_);
  {
    auto undoscope = add_undo_scope (scopename); // preserves redostack_
    for (const auto &func : funcs)
      func();
  }
  undostack_.swap (redostack_);
  if (redostack_was_empty || undostack_.empty())
    emit_notify ("dirty");
}

bool
ProjectImpl::can_undo ()
{
  return undostack_.size() > 0;
}

void
ProjectImpl::redo ()
{
  assert_return (undo_scopes_open_ == 0 && undo_groups_open_ == 0);
  return_unless (!redostack_.empty());
  std::vector<VoidF> funcs;
  while (!redostack_.empty() && redostack_.back().func)
    {
      funcs.push_back (redostack_.back().func);
      redostack_.pop_back();
    }
  assert_return (!redostack_.empty() && redostack_.back().func == nullptr); // must contain scope name
  const String scopename = redostack_.back().name;
  UDEBUG ("Undo: steps=%d scope: %s\n", funcs.size(), scopename);
  redostack_.pop_back(); // pop scope name
  // run redo steps with undo scope
  const bool undostack_was_empty = undostack_.empty();
  {
    auto undoscope = add_undo_scope (scopename); // preserves redostack_
    for (const auto &func : funcs)
      func();
  }
  if (undostack_was_empty || redostack_.empty())
    emit_notify ("dirty");
}

bool
ProjectImpl::can_redo ()
{
  return redostack_.size() > 0;
}

void
ProjectImpl::group_undo (const String &undoname)
{
  assert_return (undoname != "");
  undo_groups_open_++;
  if (undo_groups_open_ == 1)
    undo_group_name_ = undoname;
  /* Opened undo groups cause:
   * a) rename of the first opened undo scope
   * b) merging of undo scopes
   * c) block undo(), redo() calls
   * We avoid group state tracking through IPC boundaries.
   */
}

void
ProjectImpl::ungroup_undo ()
{
  assert_return (undo_groups_open_ > 0);
  undo_groups_open_--;
  if (!undo_groups_open_)
    undo_group_name_ = "";
}

void
ProjectImpl::clear_undo ()
{
  assert_warn (undo_scopes_open_ == 0 && undo_groups_open_ == 0);
  undostack_.clear();
  redostack_.clear();
  emit_notify ("dirty");
}

size_t ProjectImpl::undo_mem_counter = 0;

size_t
ProjectImpl::undo_size_guess () const
{
  size_t count = undostack_.size();
  count += redostack_.size();
  size_t item = sizeof (UndoFunc);
  item += sizeof (std::shared_ptr<void>); // undofunc selfp
  item += 4 * sizeof (uint64);            // undofunc arguments: double ClipNote struct
  return count * item + undo_mem_counter;
}

TelemetryFieldS
ProjectImpl::telemetry () const
{
  TelemetryFieldS v;
  AudioProcessorP proc = master_processor ();
  assert_return (proc, v);
  const AudioTransport &transport = proc->transport();
  v.push_back (telemetry_field ("current_tick", &transport.current_tick_d));
  v.push_back (telemetry_field ("current_bar", &transport.current_bar));
  v.push_back (telemetry_field ("current_beat", &transport.current_beat));
  v.push_back (telemetry_field ("current_sixteenth", &transport.current_semiquaver));
  v.push_back (telemetry_field ("current_bpm", &transport.current_bpm));
  v.push_back (telemetry_field ("current_minutes", &transport.current_minutes));
  v.push_back (telemetry_field ("current_seconds", &transport.current_seconds));
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
  AudioProcessorP proc = device->_audio_processor();
  return_unless (proc, nullptr);
  return proc;
}

bool
ProjectImpl::set_bpm (double bpm)
{
  bpm = CLAMP (bpm, MIN_BPM, MAX_BPM);
  return_unless (tick_sig_.bpm() != bpm, false);
  tick_sig_.set_bpm (bpm);
  update_tempo();
  emit_notify ("bpm");
  return true;
}

bool
ProjectImpl::set_numerator (uint8 numerator)
{
  if (tick_sig_.set_signature (numerator, tick_sig_.beat_unit()))
    {
      update_tempo();
      emit_notify ("numerator");
      return true;
    }
  return false;
}

bool
ProjectImpl::set_denominator (uint8 denominator)
{
  if (tick_sig_.set_signature (tick_sig_.beats_per_bar(), denominator))
    {
      update_tempo();
      emit_notify ("denominator");
      return true;
    }
  return false;
}

void
ProjectImpl::update_tempo ()
{
  AudioProcessorP proc = master_processor();
  return_unless (proc);
  const TickSignature tsig (tick_sig_);
  auto job = [proc, tsig] () {
    AudioTransport &transport = const_cast<AudioTransport&> (proc->engine().transport());
    transport.tempo (tsig);
  };
  proc->engine().async_jobs += job;
}

void
ProjectImpl::start_playback (double autostop)
{
  assert_return (!discarded_);
  assert_return (main_config.engine);
  AudioEngine &engine = *main_config.engine;
  ProjectImplP selfp = shared_ptr_cast<ProjectImpl> (this);     // keep alive while engine changes refcounts
  ProjectImplP oldp = engine.get_project();                     // keep alive while engine changes refs
  if (oldp != selfp)
    {
      if (oldp)
        {
          oldp->stop_playback();
          engine.set_project (nullptr);
        }
      engine.set_project (selfp);
    }
  assert_return (this == &*engine.get_project());

  main_loop->clear_source (&autoplay_timer_);
  AudioProcessorP proc = master_processor();
  return_unless (proc);
  std::shared_ptr<CallbackS> queuep = std::make_shared<CallbackS>();
  for (auto track : tracks_)
    track->queue_cmd (*queuep, track->START);
  auto job = [proc, queuep, autostop] () {
    AudioEngine &engine = proc->engine();
    const double udmax = 18446744073709549568.0; // max double exactly matching an uint64_t
    const uint64_t s = autostop > udmax ? udmax : autostop * engine.sample_rate();
    engine.set_autostop (s);
    AudioTransport &transport = const_cast<AudioTransport&> (engine.transport());
    transport.running (true);
    for (const auto &cmd : *queuep)
      cmd();
  };
  proc->engine().async_jobs += job;
}

void
ProjectImpl::stop_playback ()
{
  main_loop->clear_source (&autoplay_timer_);
  AudioProcessorP proc = master_processor();
  return_unless (proc);
  auto stop_queuep = std::make_shared<DCallbackS>();
  for (auto track : tracks_)
    track->queue_cmd (*stop_queuep, track->STOP);
  auto job = [proc, stop_queuep] () {
    AudioTransport &transport = const_cast<AudioTransport&> (proc->engine().transport());
    const bool wasrunning = transport.running();
    transport.running (false);
    if (!wasrunning)
      transport.set_tick (-AUDIO_BLOCK_MAX_RENDER_SIZE / 2 * transport.tick_sig.ticks_per_sample());
    for (const auto &stop : *stop_queuep)
      stop (!wasrunning); // restart = !wasrunning
    if (!wasrunning)
      transport.set_tick (0); // adjust transport and track positions
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
  assert_return (!discarded_, nullptr);
  const bool havemaster = tracks_.size() != 0;
  TrackImplP track = TrackImpl::make_shared (*this, !havemaster);
  tracks_.insert (tracks_.end() - int (havemaster), track);
  emit_event ("track", "insert", { { "track", track }, });
  track->_set_parent (this);
  emit_notify ("all_tracks");
  return track;
}

bool
ProjectImpl::remove_track (Track &child)
{
  assert_return (child._parent() == this, false);
  TrackImplP track = shared_ptr_cast<TrackImpl> (&child);
  return_unless (track && !track->is_master(), false);
  clear_undo(); // TODO: implement undo for remove_track
  if (!Aux::erase_first (tracks_, [track] (TrackP t) { return t == track; }))
    return false;
  // destroy Track
  track->_set_parent (nullptr);
  emit_event ("track", "remove");
  emit_notify ("all_tracks");
  return true;
}

TrackS
ProjectImpl::all_tracks ()
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

void
ProjectImpl::create_properties ()
{
  // chain to base class
  DeviceImpl::create_properties();
  // create own properties
  auto getbpm = [this] (Value &val) { val = tick_sig_.bpm(); };
  auto setbpm = [this] (const Value &val) { return set_bpm (val.as_double()); };
  auto getbpb = [this] (Value &val) { val = tick_sig_.beats_per_bar(); };
  auto setbpb = [this] (const Value &val) { return set_numerator (val.as_int()); };
  auto getunt = [this] (Value &val) { val = tick_sig_.beat_unit(); };
  auto setunt = [this] (const Value &val) { return set_denominator (val.as_int()); };
  PropertyBag bag = property_bag();
  // bag.group = _("State");
  // TODO: bag += Bool ("dirty", &dirty_, _("Modification Flag"), _("Dirty"), false, ":r:G:", _("Flag indicating modified project state"));
  // struct Prop { CString ident; ValueGetter getter; ValueSetter setter; Param param; ValueLister lister; };
  bag.group = _("Timing");
  bag += Prop (getbpb, setbpb, { "numerator", _("Signature Numerator"), _("Numerator"), 4., "", MinMaxStep { 1., 63., 0 }, STANDARD });
  bag += Prop (getunt, setunt, { "denominator", _("Signature Denominator"), _("Denominator"), 4, "", MinMaxStep { 1, 16, 0 }, STANDARD });
  bag += Prop (getbpm, setbpm, { "bpm", _("Beats Per Minute"), _("BPM"), 90., "", MinMaxStep { 10., 1776., 0 }, STANDARD });
  bag.group = _("Tuning");
  bag += Prop (make_enum_getter<MusicalTuning> (&musical_tuning_), make_enum_setter<MusicalTuning> (&musical_tuning_),
               { "musical_tuning", _("Musical Tuning"), _("Tuning"), uint32_t (MusicalTuning::OD_12_TET), "", {}, STANDARD, {
                  String ("descr=") + _("The tuning system which specifies the tones or pitches to be used. "
                                        "Due to the psychoacoustic properties of tones, various pitch combinations can "
                                        "sound \"natural\" or \"pleasing\" when used in combination, the musical "
                                        "tuning system defines the number and spacing of frequency values applied."), } },
               enum_lister<MusicalTuning>);
}

DeviceInfo
ProjectImpl::device_info ()
{
  return {}; // TODO: DeviceInfo
}

AudioProcessorP
ProjectImpl::_audio_processor () const
{
  return {}; // TODO: AudioProcessorP
}

void
ProjectImpl::_set_event_source (AudioProcessorP esource)
{
  // TODO: _set_event_source
}

} // Ase
