// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first

#include "project.hh"
#include "plugin.hh"
#include "jsonipc/jsonipc.hh"
#include "main.hh"
#include "compress.hh"
#include "path.hh"
#include "strings.hh"
#include "unicode.hh"
#include "storage.hh"
#include "server.hh"
#include "internal.hh"
#include "devices/liquidsfz/liquidsfzplugin.hh"
#include <list>

#define UDEBUG(...)     Ase::debug ("undo", __VA_ARGS__)

using namespace std::literals;
namespace te = tracktion::engine;

namespace Ase {

static Preference synth_latency_pref =
  Preference ({
      "project.default_license", _("Default License"), "",
      "CC-BY-SA-4.0 - https://creativecommons.org/licenses/by-sa/4.0/legalcode",
      "",
      {}, STANDARD, {
        String ("descr=") + _("Default LICENSE to apply in the project properties."), } });

static std::vector<ProjectImplP> &g_projects = *new std::vector<ProjectImplP>();

ProjectP
Project::last_project()
{
  return g_projects.empty() ? nullptr : g_projects.back();
}

// == TransportListener ==
class ProjectImpl::TransportListener : juce::ChangeListener, tracktion::TransportControl::Listener, juce::ValueTree::Listener
{
  tracktion::TransportControl &transport;
  ProjectImpl &project_;
  ASE_CLASS_NON_COPYABLE (TransportListener);
  LoopID ppt = LoopID::INVALID;
  FastMemory::Block transport_block_;
  std::list<std::function<void()>> stopped_callbacks_;
  te::Edit *edit_ = nullptr;
public:
  struct Position {
    int    fps = 0, frame = 0;
    int    bar = 0, beat = 0;
    int    sxth = 0, tick = 0;
    int    snum = 0, sden = 0;
    double bpm = 0, sec = 0;
    int    min = 0;
  } &pos;
  TransportListener (tracktion::TransportControl &tc, ProjectImpl &project) :
    transport (tc), project_ (project),
    transport_block_ (SERVER->telemem_allocate (sizeof (Position))),
    edit_ (project.edit_.get()),
    pos (*new (transport_block_.block_start) Position{})
  {
    assert_return (this_thread_is_ase());
    transport.addChangeListener (this); // for ChangeListener
    transport.addListener (this);       // for TransportControl::Listener
    if (edit_)
      edit_->state.addListener (this);
  }
  ~TransportListener() override
  {
    assert_return (this_thread_is_ase());
    if (edit_)
      edit_->state.removeListener (this);
    transport.removeListener (this);
    transport.removeChangeListener (this);
    SERVER->telemem_release (transport_block_);
  }
  void
  valueTreePropertyChanged (juce::ValueTree &vtree, const juce::Identifier &id) override
  {
    return_unless (project_.edit_);
    if (id == tracktion_engine::IDs::name) // vtree == edit_->state
      project_.emit_notify ("name");
    if (id == tracktion_engine::IDs::bpm) // vtree == edit_->tempoSequence.getTempo (0)->state
      project_.emit_notify ("bpm");
    if (id == tracktion_engine::IDs::numerator)
      project_.emit_notify ("numerator");
    if (id == tracktion_engine::IDs::denominator)
      project_.emit_notify ("denominator");
    if (id == tracktion_engine::IDs::volume) {
      auto mvp = project_.edit_->getMasterVolumePlugin();
      if (mvp && vtree == mvp->state)
        project_.emit_notify ("master_volume");
    }
  }
  void
  valueTreeChildAdded (juce::ValueTree &parent, juce::ValueTree &child) override
  {
    if (parent == edit_->state && te::TrackList::isTrack (child))
      project_.emit_notify ("all_tracks");
  }
  void
  valueTreeChildRemoved (juce::ValueTree &parent, juce::ValueTree &child, int) override
  {
    if (parent == edit_->state && te::TrackList::isTrack (child))
      project_.emit_notify ("all_tracks");
  }
  void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
  void valueTreeParentChanged (juce::ValueTree&) override {}
  void
  changeListenerCallback (juce::ChangeBroadcaster *source) override
  {
    if (source == &transport) {
      transport_changed ("change");
    }
  }
  void autoSaveNow             () override {}
  void setAllLevelMetersActive (bool become_inactive) override {}
  void setVideoPosition        (tracktion::TimePosition pos, bool force_jump) override {}
  void recordingStarted        (tracktion::SyncPoint start, std::optional<tracktion::TimeRange> punch_range) override {}
  void recordingStopped        (tracktion::SyncPoint sync_point, bool discard_recordings) override {}
  void recordingAboutToStart   (tracktion::InputDeviceInstance &device, tracktion::EditItemID target) override {}
  void recordingAboutToStop    (tracktion::InputDeviceInstance &device, tracktion::EditItemID target) override {}
  void recordingFinished       (tracktion::InputDeviceInstance &device, tracktion::EditItemID target,
                                const juce::ReferenceCountedArray<tracktion::Clip> &recording) override {}
  void
  playbackContextChanged () override
  {
    tracktion::EditPlaybackContext *context = transport.getCurrentPlaybackContext();
    Ase::diag ("PlaybackContextChanged: context=%p graph=%d playing=%d position=%.3fsecs\n", context,
               context ? context->isPlaybackGraphAllocated() : 0,
               context ? context->isPlaying() : 0,
               context ? context->getPosition().inSeconds() : 0);
  }
  void
  startVideo () override
  {
    assert_return (this_thread_is_ase());
    poll_position();
    if (ppt == LoopID::INVALID) // TODO: can we optimize telemetry form trkn?
      ppt = main_loop->add ([this] { this->poll_position(); return true; }, std::chrono::milliseconds (16));
    project_.emit_notify ("is_playing");
    transport_changed ("start-video");
  }
  void
  stopVideo () override
  {
    assert_return (this_thread_is_ase());
    main_loop->cancel (&ppt);
    poll_position();
    project_.emit_notify ("is_playing");
    transport_changed ("stop-video");
    while (!stopped_callbacks_.empty()) {
      const auto f = stopped_callbacks_.front();
      stopped_callbacks_.pop_front();
      f();
    }
  }
  void
  run_when_stopped (const std::function<void()> &f)
  {
    if (transport.isPlaying())
      stopped_callbacks_.push_back (f);
    else
      f();
  }
  void
  transport_changed (const std::string &what)
  {
    auto position = transport.getPosition();
    Ase::printerr ("Transport: playing=%d position=%.3fsecs (%s)\n",
                   transport.isPlaying(), position.inSeconds(), what.c_str());
  }
  void
  poll_position()
  {
    auto context = transport.getCurrentPlaybackContext();
    return_unless (!!context);

    auto &transport = project_.edit_->getTransport();
    auto &tempoSeq = project_.edit_->tempoSequence;

    // Get Current Time - getPosition() is the cursor position.
    // Use edit->getCurrentPlaybackContext()->getAudibleTimelineTime() for compensating for latency
    const tracktion::TimePosition currentPos = transport.getPosition();
    const double totalSeconds = currentPos.inSeconds();

    // Calculate Minutes / Seconds / Millis
    // We use std::abs to handle potential negative times (pre-roll) safely
    const double absSeconds = std::abs (totalSeconds);
    const int intSeconds = int (absSeconds);
    pos.min = intSeconds / 60;
    pos.sec = absSeconds - pos.min * 60;

    // Calculate Musical Position (Bars & Beats)
    tracktion::tempo::BarsAndBeats bab = tempoSeq.toBarsAndBeats (currentPos);

    // Tracktion uses 0-based indexing for Bars and Beats internally.
    pos.bar = bab.bars;
    pos.beat = bab.getWholeBeats();

    // Calculate Sub-beat divisions (Sixteenths and Ticks)
    // bab.getFractionalBeats() returns the remainder of the beat (0.0 to 0.999...)
    double fractionalBeat = bab.getFractionalBeats().inBeats();

    // Sixteenths: There are 4 sixteenths in a beat
    pos.sxth = int (fractionalBeat * 4.0);

    // Ticks: Tracktion standard is 960 PPQ (Pulses Per Quarter note)
    pos.tick = int (fractionalBeat * 960.0);

    // Get Tempo and Time Signature at this specific moment
    // (Tempo can change during the song, so we ask for the value *at* currentPos)
    pos.bpm = tempoSeq.getBpmAt (currentPos);
    auto &timesig = tempoSeq.getTimeSigAt (currentPos);
    pos.snum = timesig.numerator;
    pos.sden = timesig.denominator;

    // Calculate Frames (SMPTE)
    const te::TimecodeDisplayFormat tdf = project_.edit_->getTimecodeFormat();
    pos.fps = tdf.getFPS();

    // Simple frame calculation: seconds * fps
    // (Note: This is a basic calculation. For Drop-frame SMPTE, use tdf.toFullTimecode(...))
    pos.frame = int (absSeconds * pos.fps) % int (pos.fps);
  }
};


// == ProjectImpl ==
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

static void
test_sfz (ProjectImpl *project, te::Edit *edit, const String &filename)
{
  TrackP track = project->create_track();
  assert (track);
  track->name ("LiquidSFZTest");

  TrackImplP trackimpl = std::dynamic_pointer_cast<TrackImpl> (track);
  assert (trackimpl);

  double start = 0;
  double duration = 4;
  ClipP clip = trackimpl->create_midi_clip ("NotesClip", start, duration);
  assert (clip);

  ClipNoteS batch;
  auto add_note = [&] (int start, int key, int duration)
    {
      ClipNote note;
      note.id = -1;
      note.key = key;
      note.channel = 0;
      note.tick = start * TRANSPORT_PPQN;
      note.duration = duration * TRANSPORT_PPQN;
      note.velocity = 0.8f;
      batch.push_back (note);
    };
  add_note (0, 60, 1);
  add_note (1, 64, 1);
  add_note (2, 67, 1);
  clip->change_batch (batch, "Add Note");

  auto& engine = edit->engine;
  engine.getPluginManager().createBuiltInType<LiquidSFZPlugin>();

  auto plugin = trackimpl->create_plugin (LiquidSFZPlugin::xmlTypeName);
  assert (plugin);

  if (auto pluginimpl = std::dynamic_pointer_cast<PluginImpl> (plugin))
    if (auto liquidsfz = dynamic_cast<LiquidSFZPlugin *> (pluginimpl->plugin()))
      liquidsfz->load (filename);

  auto &transport = edit->getTransport();
  transport.setLoopRange({ tracktion::TimePosition::fromSeconds (start), tracktion::TimeDuration::fromSeconds (duration) });
  transport.looping = true;
  transport.setPosition (tracktion::TimePosition::fromSeconds (start));
}

ProjectImpl::ProjectImpl()
{
  bpm (120);
  numerator (4);
  denominator (4);
  edit_ = std::make_unique<te::Edit> (*trkn_engine(), te::Edit::forEditing);
  if (edit_) {
    register_ase_obj (this, *edit_);
    transport_listener_ = std::make_unique<TransportListener> (edit_->getTransport(), *this);
  }
  if (!edit_ || !transport_listener_)
    fatal_error ("failed to create tracktion::engine::edit");

  if (auto filename = getenv ("SFZ"))
    test_sfz (this, edit_.get(), filename);

  edit_->getUndoManager().clearUndoHistory();

  /* TODO: MusicalTuning
   * group = _("Tuning");
   * Prop ("musical_tuning", _("Musical Tuning"), _("Tuning"), MusicalTuning::OD_12_TET, {
   *   "descr="s + _("The tuning system which specifies the tones or pitches to be used. "
   *                 "Due to the psychoacoustic properties of tones, various pitch combinations can "
   *                 "sound \"natural\" or \"pleasing\" when used in combination, the musical "
   *                 "tuning system defines the number and spacing of frequency values applied."), "" },
   *   enum_lister<MusicalTuning>);
   */
}

void
ProjectImpl::deactivate_edit()
{
  return_unless (!!edit_);
  auto &transport = edit_->getTransport();
  if (transport.isPlaying() || transport.isRecording())
    transport.stop (true, true);
  transport.freePlaybackContext();
  edit_->cancelAnyPendingUpdates();
  edit_ = nullptr;
}

ProjectImpl::~ProjectImpl()
{
  unregister_ase_obj (this, edit_.get());
  deactivate_edit();
  transport_listener_ = nullptr;
  edit_ = nullptr;
}


void
ProjectImpl::force_shutdown_all ()
{
 rescan:
  for (size_t i = 0; i < g_projects.size(); i++)
    if (g_projects[i]->edit_) {
      g_projects[i]->deactivate_edit();
      goto rescan; // callbacks can change anything
    }
}

String
ProjectImpl::name() const
{
  // Edit.getName() requires af ProjectItem, which we dont use
  return edit_ ? edit_->state.getProperty (tracktion_engine::IDs::name).toString().toStdString() : "";
}

void
ProjectImpl::name (const std::string &nm)
{
  return_unless (!!edit_);
  // tracktion_engine::getProjectItemForEdit (*edit_)->setName (nm, tracktion_engine::ProjectItem::SetNameMode::doDefault);
  edit_->state.setProperty (tracktion_engine::IDs::name, juce::String (nm), &edit_->getUndoManager());
}

TelemetryFieldS
ProjectImpl::telemetry () const
{
  TelemetryFieldS v;
  v.push_back (telemetry_field ("current_tick", &transport_listener_->pos.tick));
  v.push_back (telemetry_field ("current_bar", &transport_listener_->pos.bar));
  v.push_back (telemetry_field ("current_beat", &transport_listener_->pos.beat));
  v.push_back (telemetry_field ("current_sixteenth", &transport_listener_->pos.sxth));
  v.push_back (telemetry_field ("current_bpm", &transport_listener_->pos.bpm));
  v.push_back (telemetry_field ("current_numerator", &transport_listener_->pos.snum));
  v.push_back (telemetry_field ("current_denominator", &transport_listener_->pos.sden));
  v.push_back (telemetry_field ("current_minutes", &transport_listener_->pos.min));
  v.push_back (telemetry_field ("current_seconds", &transport_listener_->pos.sec));
  return v;
}

void
ProjectImpl::foreach_track (const std::function<bool(Track&,int)> &cb)
{
  std::function<bool(te::Track&,int)> foreach_track = [&] (te::Track &t, int depth)
  {
    const TrackImplP trackp = TrackImpl::from_trkn (t);
    if (!trackp || !cb (*trackp, depth))
      return false;
    if (trackp->is_folder())
      for (auto subtrack : dynamic_cast<te::FolderTrack*> (&t)->getAllSubTracks (false /*recursive*/))
        if (subtrack &&
            false == foreach_track (*subtrack, depth + 1))
          return false;
    return true;
  };
  edit_->visitAllTopLevelTracks ([&] (te::Track &t) { return foreach_track (t, 0); });
}

ProjectImplP
ProjectImpl::create (const String &projectname)
{
  ProjectImplP project = ProjectImpl::make_shared();
  g_projects.push_back (project);
  project->name (projectname);
  project->edit_->getUndoManager().clearUndoHistory();
  return project;
}

void
ProjectImpl::discard ()
{
  return_unless (!discarded_);
  stop_playback();
  const size_t nerased = Aux::erase_first (g_projects, [this] (auto ptr) { return ptr.get() == this; });
  if (nerased)
    {} // resource cleanups
  discarded_ = true;
}

void
ProjectImpl::remove_self ()
{
  // Project has no parent; just emit the `removed` event
  GadgetImpl::remove_self();
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
ProjectImpl::save_project (const String &utf8filename, bool collect)
{
  const String savepath = decodefs (utf8filename);
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
      // serialize Project (TODO: use tracktion_engine saving & loading)
      error = ws.store_file_data ("project.json", "{}\n", true);
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

Error
ProjectImpl::snapshot_project (String &json)
{
  assert_return (storage_ == nullptr, Error::OPERATION_BUSY);
  // writer setup
  PStorage storage (&storage_); // storage_ = &storage;
  storage_->writer_cachedir = anklang_cachedir_create();
  if (storage_->writer_cachedir.empty() || !Path::check (storage_->writer_cachedir, "d"))
    return Error::NO_PROJECT_DIR;
  storage_->anklang_dir = storage_->writer_cachedir;
  storage_->asset_hashes.clear();
  // serialize Project (TODO: use tracktion_engine saving & loading)
  json = "{}";
  // cleanup
  anklang_cachedir_cleanup (storage_->writer_cachedir);
  return Error::NONE;
}

String
ProjectImpl::writer_file_name (const String &fspath) const
{
  assert_return (storage_ != nullptr, "");
  assert_return (!storage_->writer_cachedir.empty(), "");
  return Path::join (storage_->writer_cachedir, fspath);
}

Error
ProjectImpl::writer_add_file (const String &fspath)
{
  assert_return (storage_ != nullptr, Error::INTERNAL);
  assert_return (!storage_->writer_cachedir.empty(), Error::INTERNAL);
  if (!Path::check (fspath, "frw"))
    return Error::FILE_NOT_FOUND;
  if (!string_startswith (fspath, storage_->writer_cachedir))
    return Error::FILE_OPEN_FAILED;
  storage_->writer_files.push_back ({ fspath, Path::basename (fspath) });
  return Error::NONE;
}

Error
ProjectImpl::writer_collect (const String &fspath, String *hexhashp)
{
  assert_return (storage_ != nullptr, Error::INTERNAL);
  assert_return (!storage_->anklang_dir.empty(), Error::INTERNAL);
  if (!Path::check (fspath, "fr"))
    return Error::FILE_NOT_FOUND;
  // determine hash of file to collect
  const String hexhash = string_to_hex (blake3_hash_file (fspath));
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
  if (Path::dircontains (storage_->anklang_dir, fspath, &relpath))
    {
      storage_->asset_hashes.push_back ({ hexhash, relpath });
      *hexhashp = hexhash;
      return Error::NONE;
    }
  // determine unique path name
  const size_t file_size = Path::file_size (fspath);
  const String basedir = storage_->anklang_dir;
  relpath = Path::join ("samples", Path::basename (fspath));
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
  const bool copied = Path::copy_file (fspath, dest);
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
  return encodefs (saved_filename_);
}

Error
ProjectImpl::load_project (const String &utf8filename)
{
  const String filename = decodefs (utf8filename);
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
  // parse project (TODO: use tracktion_engine loading)
  // jsd contains project state from tracktion_engine (currently {})
  saved_filename_ = storage_->loading_file;
  return Error::NONE;
}

StreamReaderP
ProjectImpl::load_blob (const String &fspath)
{
  assert_return (storage_ != nullptr, nullptr);
  assert_return (!storage_->loading_file.empty(), nullptr);
  return stream_reader_zip_member (storage_->loading_file, fspath);
}

/// Find file from hash code, returns fspath.
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

String
ProjectImpl::match_serialized (const String &regex, int group)
{
  String json;
  Error error = snapshot_project (json);
  if (!!error) {
    warning ("Project: failed to serialize project: %s\n", ase_error_blurb (error));
    return "";
  }
  return Re::grep (regex, json, group);
}

UndoScope::UndoScope (ProjectImplP projectp, const String &scopename) :
  projectp_ (projectp),
  scopename_ (scopename)
{
  assert_return (projectp);
  assert_return (projectp->edit_);
  projectp->edit_->getUndoManager().beginNewTransaction (juce::String (scopename));
}

UndoScope::~UndoScope()
{
  assert_return (projectp_);
  assert_return (projectp_->edit_);
  projectp_->edit_->getUndoManager().beginNewTransaction();
}

UndoScope
ProjectImpl::undo_scope (const String &scopename)
{
  assert_warn (scopename != "");
  return UndoScope (shared_ptr_cast<ProjectImpl> (this), scopename);
}

UndoScope
ProjectImpl::add_undo_scope (const String &scopename)
{
  return UndoScope (shared_ptr_cast<ProjectImpl> (this), scopename);
}

void
ProjectImpl::undo ()
{
  return_unless (!!edit_);
  const bool had_undo = edit_->getUndoManager().canUndo();
  edit_->getUndoManager().undo();
  if (had_undo)
    emit_notify ("dirty");
}

bool
ProjectImpl::can_undo ()
{
  return_unless (!!edit_, false);
  return edit_->getUndoManager().canUndo();
}

void
ProjectImpl::redo ()
{
  return_unless (!!edit_);
  const bool had_redo = edit_->getUndoManager().canRedo();
  edit_->getUndoManager().redo();
  if (had_redo)
    emit_notify ("dirty");
}

bool
ProjectImpl::can_redo ()
{
  return_unless (!!edit_, false);
  return edit_->getUndoManager().canRedo();
}

double
ProjectImpl::length () const
{
  return_unless (!!edit_, 0.0);
  return edit_->getLength().inSeconds();
}

double
ProjectImpl::master_volume () const
{
  return_unless (!!edit_, 0.0);
  auto volPlugin = edit_->getMasterVolumePlugin();
  return_unless (!!volPlugin, 0.0);
  return te::volumeFaderPositionToDB (volPlugin->volume.get());
}

void
ProjectImpl::master_volume (double db)
{
  return_unless (!!edit_);
  auto volPlugin = edit_->getMasterVolumePlugin();
  return_unless (!!volPlugin);
  const float sliderPos = te::decibelsToVolumeFaderPosition (db);
  volPlugin->volume = sliderPos;
  volPlugin->volParam->updateFromAttachedValue();
}

void
ProjectImpl::group_undo (const String &undoname)
{
  return_unless (!!edit_);
  edit_->getUndoManager().beginNewTransaction (juce::String (undoname));
}

void
ProjectImpl::ungroup_undo ()
{
  return_unless (!!edit_);
  edit_->getUndoManager().beginNewTransaction();
}

void
ProjectImpl::clear_undo ()
{
  return_unless (!!edit_);
  edit_->getUndoManager().clearUndoHistory();
  emit_notify ("dirty");
}

void
ProjectImpl::bpm (double newbpm)
{
  return_unless (!!edit_);
  const double nbpm = CLAMP (newbpm, MIN_BPM, MAX_BPM);
  auto &tempoSeq = edit_->tempoSequence;
  auto *tempo = tempoSeq.getTempo (0);
  if (tempo && tempo->getBpm() != nbpm)
    tempo->setBpm (nbpm);
}

double
ProjectImpl::bpm () const
{
  return_unless (!!edit_, 120.0);
  auto *tempo = edit_->tempoSequence.getTempo (0);
  return tempo ? tempo->getBpm() : 120.0;
}

void
ProjectImpl::numerator (double num)
{
  return_unless (!!edit_);
  auto &tempoSeq = edit_->tempoSequence;
  auto *timeSig = tempoSeq.getTimeSig (0);
  if (timeSig && timeSig->numerator != num)
    timeSig->numerator = num;
}

double
ProjectImpl::numerator () const
{
  return_unless (!!edit_, 4.0);
  auto *timeSig = edit_->tempoSequence.getTimeSig (0);
  return timeSig ? timeSig->numerator : 4.0;
}

void
ProjectImpl::denominator (double den)
{
  return_unless (!!edit_);
  auto &tempoSeq = edit_->tempoSequence;
  auto *timeSig = tempoSeq.getTimeSig (0);
  if (timeSig && timeSig->denominator != den)
    timeSig->denominator = den;
}

double
ProjectImpl::denominator () const
{
  return_unless (!!edit_, 4.0);
  auto *timeSig = edit_->tempoSequence.getTimeSig (0);
  return timeSig ? timeSig->denominator : 4.0;
}

void
ProjectImpl::start_playback (double autostop)
{
  assert_return (!discarded_);
  edit_->getTransport().ensureContextAllocated();
  if (edit_->getTransport().isPlayContextActive())
    edit_->getTransport().play (false);
}

void
ProjectImpl::pause_playback ()
{
  if (edit_->getTransport().isPlaying())
    edit_->getTransport().stop (false, false);
}

void
ProjectImpl::stop_playback ()
{
  edit_->getTransport().stop (false, true);
  transport_listener_->run_when_stopped ([this] {
    // wait until stopped, so the new position persists
    edit_->getTransport().setPosition (tracktion::TimePosition::fromSeconds (0.0));
    transport_listener_->poll_position();
  });
}

bool
ProjectImpl::is_playing () const
{
  return edit_->getTransport().isPlaying();
}

void
ProjectImpl::is_playing (bool play)
{
  if (is_playing() == play)
    return;
  if (is_playing())
    pause_playback();
  else
    start_playback();
}

TrackP
ProjectImpl::create_track ()
{
  return_unless (edit_ && !discarded_, nullptr);
  auto t = edit_->insertNewAudioTrack (tracktion::TrackInsertPoint (nullptr, nullptr), nullptr);
  if (!t) return nullptr;
  TrackImplP track = TrackImpl::from_trkn (*t);
  emit_event ("track", "insert", { { "track", track }, });
  emit_notify ("all_tracks");
  return track;
}

TrackS
ProjectImpl::all_tracks ()
{
  TrackS tracks;
  auto tf = [&] (Track &track, int depth)
  {
    tracks.push_back (shared_ptr_cast<TrackImpl> (&track));
    return true;
  };
  foreach_track (tf);
  return tracks;
}

ssize_t
ProjectImpl::track_index (const Track &child) const
{
  ssize_t index = 0;
  ssize_t found = -1;
  auto tf = [&] (Track &track, int depth)
  {
    if (&track == &child)
      {
        found = index;
        return false;
      }
    index++;
    return true;
  };
  const_cast<ProjectImpl*> (this)->foreach_track (tf);
  return found;
}

int64_t
ProjectImpl::bar_ticks () const
{
  return_unless (!!edit_, 0);
  auto &tempoSeq = edit_->tempoSequence;
  auto *timeSig = tempoSeq.getTimeSig (0);
  if (!timeSig)
    return 0;

  const int beats_per_bar = timeSig->numerator;
  const int beat_unit = timeSig->denominator;

  // Calculate beat ticks: SEMIQUAVER_TICKS * (16 / beat_unit)
  // SEMIQUAVER_TICKS = TRANSPORT_PPQN / 4 = 1209600
  const int64 SEMIQUAVER_TICKS = 1209600;
  const int semiquavers_per_beat = 16 / beat_unit;
  const int64 beat_ticks = SEMIQUAVER_TICKS * semiquavers_per_beat;

  return beat_ticks * beats_per_bar;
}

TrackP
ProjectImpl::master_track ()
{
  return_unless (!!edit_, nullptr);
  auto *masterTrack = edit_->getMasterTrack();
  return_unless (masterTrack, nullptr);
  return TrackImpl::from_trkn (*masterTrack);
}

DeviceInfo
ProjectImpl::device_info ()
{
  return {}; // TODO: DeviceInfo
}

} // Ase
