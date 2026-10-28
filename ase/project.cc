// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "trkn/tracktion.hh"   // PCH include must come first

#include "project.hh"
#include "plugin.hh"
#include "main.hh"
#include "path.hh"
#include "platform.hh"
#include "strings.hh"
#include "unicode.hh"
#include "storage.hh"
#include "server.hh"
#include "internal.hh"
#include "devices/liquidsfz/liquidsfzplugin.hh"
#include <filesystem>
#include <list>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

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

static Preference projects_dir_pref ({
  .ident = "anklang-projects-dir", .label = _("Projects Directory"), .initial = "", .hints = STANDARD,
});

static Preference sync_saves_pref ({
  .ident = "anklang-fsync-saves", .label = _("Sync Saves to Disk"), .initial = false, .hints = STANDARD,
  .metadata = { String ("descr=") + _("Wait until each save reaches the storage device. Slow, and wears out flash memory.") },
});

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
    if (id == te::IDs::source)
      project_.emit_notify ("unresolved_assets");
    if (id == tracktion_engine::IDs::name && vtree == project_.edit_->state)
      project_.emit_notify ("name");
    if (id == tracktion_engine::IDs::bpm) // vtree == edit_->tempoSequence.getTempo (0)->state
      project_.emit_notify ("bpm");
    if (id == tracktion_engine::IDs::numerator)
      project_.emit_notify ("numerator");
    if (id == tracktion_engine::IDs::denominator)
      project_.emit_notify ("denominator");
    if ((te::Clip::isClipState (vtree) && (id == te::IDs::start || id == te::IDs::length)) ||
        vtree.hasType (te::IDs::TEMPO) || vtree.hasType (te::IDs::TIMESIG))
      project_.emit_notify ("length");
  }
  void
  valueTreeChildAdded (juce::ValueTree &parent, juce::ValueTree &child) override
  {
    if (te::TrackList::isTrack (child) || te::Clip::isClipState (child))
      project_.emit_notify ("unresolved_assets");
    if (parent == edit_->state && te::TrackList::isTrack (child))
      project_.emit_notify ("all_tracks");
    if (te::TrackList::isTrack (child) || te::Clip::isClipState (child) || parent.hasType (te::IDs::TEMPOSEQUENCE))
      project_.emit_notify ("length");
  }
  void
  valueTreeChildRemoved (juce::ValueTree &parent, juce::ValueTree &child, int) override
  {
    if (te::TrackList::isTrack (child) || te::Clip::isClipState (child))
      project_.emit_notify ("unresolved_assets");
    if (parent == edit_->state && te::TrackList::isTrack (child))
      project_.emit_notify ("all_tracks");
    if (te::TrackList::isTrack (child) || te::Clip::isClipState (child) || parent.hasType (te::IDs::TEMPOSEQUENCE))
      project_.emit_notify ("length");
  }
  void
  valueTreeChildOrderChanged (juce::ValueTree &parent, int, int newIndex) override
  {
    // Reordering tracks changes all_tracks() return order, so emit like add/remove
    if (parent == edit_->state && te::TrackList::isTrack (parent.getChild (newIndex)))
      project_.emit_notify ("all_tracks");
  }
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
      note.note_number = key;
      note.start_beat = start;
      note.length_beats = duration;
      note.velocity = 102;
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

static bool
trkn_has_track_name (te::Edit &edit, const juce::String &name)
{
  bool found = false;
  edit.visitAllTracksRecursive ([&] (te::Track &track) {
    found = track.getName() == name;
    return !found;
  });
  return found;
}

ProjectImpl::ProjectImpl (const juce::ValueTree &state, const String &dir, bool new_project) :
  project_dir_ (dir)
{
  // Tracktion stores clip files relative to the marker in the project dir.
  edit_ = std::make_unique<te::Edit> (te::Edit::Options {
    .engine = *trkn_engine(), .editState = state,
    .editProjectItemID = te::ProjectItemID::fromProperty (state, te::IDs::projectID),
    .editFileRetriever = [this] {
      // Do not create a dir while the Edit loads or is destroyed.
      const String dir = project_dir (edit_ != nullptr);
      return dir.empty() ? juce::File() : juce::File (Path::join (dir, ".anklang.project"));
    },
    .filePathResolver = [this] (const juce::String &path) {
      const String file = resolve_asset (path.toStdString());
      return file.empty() ? juce::File() : juce::File (file);
    },
    .numAudioTracks = new_project ? 1u : 0u,
  });
  if (new_project)
    if (auto track = te::getFirstAudioTrack (*edit_))
      // From Edit::Edit(Engine&, EditRole), don't route AudioTrack to default MIDI
      track->getOutput().setOutputToDefaultDevice (false);
  if (edit_) {
    register_ase_obj (this, *edit_);
    transport_listener_ = std::make_unique<TransportListener> (edit_->getTransport(), *this);
  }
  if (!edit_ || !transport_listener_)
    fatal_error ("failed to create tracktion::engine::edit");
  if (new_project)
    if (auto filename = getenv ("SFZ")) // TODO: remove SFZ test code
      test_sfz (this, edit_.get(), filename);

  edit_->visitAllTracksRecursive ([] (te::Track &track) {
    if (track.isAudioTrack() || track.isFolderTrack())
      track.setName (track.getName());
    return true;
  });
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
  transport_listener_ = nullptr; // detach from edit_ and transport
  edit_ = nullptr;
}

ProjectImpl::~ProjectImpl()
{
  unregister_ase_obj (this, edit_.get());
  deactivate_edit();
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
  edit_->getUndoManager().beginNewTransaction ("Set Project Name");
  edit_->state.setProperty (tracktion_engine::IDs::name, juce::String (nm), &edit_->getUndoManager());
}

TelemetryFieldS
ProjectImpl::telemetry () const
{
  TelemetryFieldS v;
  return_unless (transport_listener_, v);
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
ProjectImpl::foreach_track (const std::function<bool(Track&,int)> &cb) const
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
  auto state = te::createEmptyEdit (*trkn_engine());
  state.setProperty (te::IDs::name, juce::String (projectname), nullptr);
  ProjectImplP project = ProjectImpl::make_shared (state, "", true);
  g_projects.push_back (project);
  return project;
}

void
ProjectImpl::discard ()
{
  return_unless (!discarded_);
  stop_playback();
  Aux::erase_first (g_projects, [this] (auto ptr) { return ptr.get() == this; });
  discarded_ = true;
}

void
ProjectImpl::remove_self ()
{
  // Project has no parent; just emit the `removed` event
  GadgetImpl::remove_self();
}

static bool
make_anklang_dir (const String &path)
{
  String mime = Path::join (path, ".anklang.project");
  return Path::stringwrite (mime, "# ANKLANG(1) project directory\n");
}

static String
projects_root()
{
  String root = getenv ("ANKLANG_PROJECTS_DIR") ?: "";
  if (root.empty())
    root = decodefs (projects_dir_pref.gets());
  if (root.empty())
    root = Path::xdg_dir ("PROJECTS", false);
  if (root.empty())
    {
      String documents = getenv ("XDG_DOCUMENTS_DIR") ?: "";
      if (documents.empty())
        documents = Path::xdg_dir ("DOCUMENTS");
      root = Path::join (documents, "Anklang");
    }
  return Path::abspath (Path::expand_tilde (root));
}

static String
project_dir_basename (String name)
{
  for (char &c : name)
    if (c == '/' || c == '\\' || uint8_t (c) < 32 || c == 127)
      c = '_';
  return name == "." || name == ".." ? "" : name;
}

static String
reserve_project_dir (const String &root, const String &basename)
{
  if (Path::mkdirs (root))
    for (uint i = 0; ; i++)
      {
        const String leaf = basename.empty() ? string_format ("Unnamed-%u", i + 1) :
                            i ? string_format ("%s(%u)", basename, i) : basename;
        const String candidate = Path::join (root, leaf);
        if (mkdir (candidate.c_str(), 0700) == 0)
          return candidate;
        if (errno != EEXIST)
          break;
      }
  return "";
}

String
ProjectImpl::create_project_dir (const String &basename)
{
  const String dir = reserve_project_dir (projects_root(), basename);
  if (dir.empty() || !make_anklang_dir (dir))
    {
      const int err = errno;
      if (!dir.empty())
        Path::rmrf (dir);
      warning ("Project: failed to create project dir: %s", strerror (err));
      errno = err;
      return "";
    }
  project_dir_ = dir;
  project_dir_changed();
  return project_dir_;
}

String
ProjectImpl::project_dir (bool create)
{
  if (project_dir_.empty() && create)
    return create_project_dir ("");
  return project_dir_;
}

static bool
is_unnamed_dirname (const String &name)
{
  return !Re::grep ("^Unnamed-[1-9][0-9]*$", name).empty();
}

String
ProjectImpl::project_dir () const
{
  return encodefs (project_dir_);
}

void
ProjectImpl::project_dir (const String&)
{
  warning ("Project.project_dir is read-only");
}

bool
ProjectImpl::unnamed () const
{
  return project_dir_.empty() || is_unnamed_dirname (Path::basename (project_dir_));
}

void
ProjectImpl::unnamed (bool)
{
  warning ("Project.unnamed is read-only");
}

void
ProjectImpl::project_dir_changed ()
{
  return_unless (!!edit_);
  edit_->sendSourceFileUpdate();
  for (const auto &track : all_tracks())
    for (const auto &clip : track->all_clips())
      clip->emit_notify ("original_file");
  emit_notify ("project_dir");
  emit_notify ("unnamed");
  emit_notify ("unresolved_assets");
}

Error
ProjectImpl::remove_project_dir ()
{
  if (discarded_ || !edit_)
    return Error::NONE;
  if (project_dir_.empty())
    return Error::NONE;
  if (!Path::check (Path::join (project_dir_, ".anklang.project"), "f"))
    return Error::BAD_PROJECT;
  stop_playback();
  Path::rmrf (project_dir_);
  if (Path::check (project_dir_, "e"))
    return Error::FILE_WRITE_FAILED;
  project_dir_.clear();
  project_dir_changed();
  return Error::NONE;
}

// All Edit XML is written here, so grep patterns match saved files: `\n` line ends, no wrapping
static String
edit_xml (const juce::ValueTree &state)
{
  juce::XmlElement::TextFormat format;
  format.newLineChars = "\n";
  format.lineWrapLength = std::numeric_limits<int>::max();
  return state.toXmlString (format).toStdString();
}

/// Flush `path` to the storage device with fsync(), which is slow and wears out flash memory.
static Error
sync_path (const String &path)
{
  const int fd = open (path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return ase_error_from_errno (errno);
  Error error = fsync (fd) < 0 ? ase_error_from_errno (errno) : Error::NONE;
  if (close (fd) < 0 && !error)
    error = ase_error_from_errno (errno);
  return error;
}

/// Replace `dir/project.anklang` with `xml`, after copying the old file into `dir/backup/`.
static Error
write_project_file (const String &dir, const String &xml)
{
  String temporary = Path::join (dir, ".project.anklang-XXXXXX");
  const int fd = mkstemp (temporary.data());
  if (fd < 0)
    return ase_error_from_errno (errno);
  close (fd);
  StorageWriter writer;
  Error error = writer.open_with_mimetype (temporary, "application/x-anklang");
  if (!error)
    error = writer.store_file_data ("edit.tracktionedit", xml);
  if (!error)
    error = writer.close();
  else
    writer.remove_opened();
  const String target = Path::join (dir, "project.anklang");
  const String backupdir = Path::join (dir, "backup");
  if (!error && Path::check (target, "e"))
    {
      Error backup_error = Error::NONE;
      if (!Path::mkdirs (backupdir))
        backup_error = ase_error_from_errno (errno);
      else
        for (uint64 stamp = timestamp_realtime(); ; stamp++)
          {
            const String backup = Path::join (backupdir, string_format ("project (%s-%u).anklang",
                                                    now_strftime ("%y%m%dT%H%M%S"), stamp));
            if (Path::copy_file (target, backup))
              break;
            if (errno != EEXIST)
              {
                backup_error = ase_error_from_errno (errno);
                break;
              }
          }
      if (!!backup_error)
        ASE_SERVER.user_note (string_format ("## Backup failed\n%s: %s", backupdir, ase_error_blurb (backup_error)));
    }
  if (!error && !Path::rename (temporary, target))
    error = ase_error_from_errno (errno);
  if (!!error)
    {
      unlink (temporary.c_str());
      return error;
    }
  StringS backups;
  Path::glob (Path::join (Path::glob_escape (backupdir), "project (*).anklang"), backups);
  strings_version_sort (&backups, true);
  while (backups.size() > 24)
    {
      Path::rmrf (backups.back());
      backups.pop_back();
    }
  return sync_saves_pref.getb() ? sync_path (target) : Error::NONE;
}

Error
ProjectImpl::save_project (const String &utf8name)
{
  if (discarded_ || !edit_)
    return Error::INTERNAL;
  const bool naming = unnamed();
  const String basename = project_dir_basename (string_strip (utf8name));
  if (naming && (basename.empty() || is_unnamed_dirname (basename)))
    return Error::INVALID_PROPERTY;
  edit_->flushState();
  String dest = project_dir_;
  if (naming)
    {
      dest = reserve_project_dir (project_dir_.empty() ? projects_root() : Path::dirname (project_dir_), basename);
      if (dest.empty())
        return ase_error_from_errno (errno);
      if (project_dir_.empty() && !make_anklang_dir (dest))
        {
          const Error error = ase_error_from_errno (errno);
          Path::rmrf (dest);
          return error;
        }
    }
  auto state = edit_->state;
  if (naming)
    {
      state = state.createCopy();
      state.setProperty (te::IDs::name, juce::String (string_strip (utf8name)), nullptr);
    }
  Error error = write_project_file (project_dir_.empty() ? dest : project_dir_, edit_xml (state));
  if (!error && naming && !project_dir_.empty() && !Path::rename (project_dir_, dest))
    error = ase_error_from_errno (errno);
  if (!!error)
    {
      if (naming)
        Path::rmrf (dest);
      return error;
    }
  if (naming)
    {
      project_dir_ = dest;
      edit_->state.setProperty (te::IDs::name, juce::String (string_strip (utf8name)), nullptr);
      project_dir_changed();
    }
  edit_->resetChangedStatus();
  return Error::NONE;
}

/// Copy the files of project dir `src` into `dest`, except its save file and backups.
static Error
copy_project_files (const String &src, const String &dest, bool project_root = true)
{
  StringS entries;
  for (const char *pattern : { "*", ".[!.]*", "..?*" })
    Path::glob (Path::join (Path::glob_escape (src), pattern), entries);
  for (const String &entry : entries)
    {
      const String name = Path::basename (entry);
      if (project_root && (name == ".anklang.project" || name == "project.anklang" || name == "backup"))
        continue;
      const String target = Path::join (dest, name);
      std::error_code ec;
      const auto status = std::filesystem::symlink_status (entry, ec);
      if (ec)
        return ase_error_from_errno (ec.value());
      if (std::filesystem::is_regular_file (status))
        {
          if (!Path::copy_file (entry, target))
            return ase_error_from_errno (errno);
          std::filesystem::permissions (target, status.permissions(), ec);
        }
      else if (std::filesystem::is_directory (status))
        {
          std::filesystem::create_directory (target, entry, ec);
          if (!ec)
            {
              const Error error = copy_project_files (entry, target, false);
              if (!!error)
                return error;
            }
        }
      else
        std::filesystem::copy (entry, target, std::filesystem::copy_options::copy_symlinks, ec);
      if (ec)
        return ase_error_from_errno (ec.value());
    }
  return Error::NONE;
}

Error
ProjectImpl::save_project_as (const String &utf8path)
{
  if (discarded_ || !edit_)
    return Error::INTERNAL;
  const String dir = Path::strip_slashes (Path::normalize (Path::abspath (decodefs (utf8path))));
  const String basename = Path::basename (dir);
  if (basename.empty() || is_unnamed_dirname (basename))
    return Error::INVALID_PROPERTY;
  if (!Path::mkdirs (Path::dirname (dir)) || mkdir (dir.c_str(), 0700) != 0)
    return ase_error_from_errno (errno);
  edit_->flushState();
  const String old_dir = project_dir_;
  const bool naming = unnamed();
  auto state = edit_->state;
  if (naming)
    {
      state = state.createCopy();
      state.setProperty (te::IDs::name, juce::String (basename), nullptr);
    }
  const String xml = edit_xml (state);
  struct stat old_stat = {}, new_stat = {};
  bool moved = false;
  // Write before moving so a failed save keeps the unnamed directory.
  if (naming && !old_dir.empty() && stat (old_dir.c_str(), &old_stat) == 0 && stat (dir.c_str(), &new_stat) == 0 &&
      old_stat.st_dev == new_stat.st_dev && !write_project_file (old_dir, xml))
    moved = Path::rename (old_dir, dir);
  Error error = Error::NONE;
  if (!moved)
    {
      error = make_anklang_dir (dir) ? Error::NONE : ase_error_from_errno (errno);
      if (!error && !old_dir.empty())
        error = copy_project_files (old_dir, dir);
      if (!error)
        error = write_project_file (dir, xml);
    }
  if (!!error)
    {
      Path::rmrf (dir);
      return error;
    }
  project_dir_ = dir;
  if (naming)
    edit_->state.setProperty (te::IDs::name, juce::String (basename), nullptr);
  project_dir_changed();
  edit_->resetChangedStatus();
  if (naming && !old_dir.empty() && !moved)
    Path::rmrf (old_dir);
  return Error::NONE;
}

ProjectImplP
ProjectImpl::from_xml (const String &xml, const String &dir)
{
  auto element = juce::parseXML (juce::String (xml));
  if (!element)
    return nullptr;
  te::updateLegacyEdit (*element);
  auto state = juce::ValueTree::fromXml (*element);
  if (!state.hasType (te::IDs::EDIT))
    return nullptr;
  if (!te::ProjectItemID::fromProperty (state, te::IDs::projectID).isValid())
    state.setProperty (te::IDs::projectID, te::ProjectItemID::createNewID (0).toString(), nullptr);
  ProjectImplP project = ProjectImpl::make_shared (state, dir, false);
  g_projects.push_back (project);
  return project;
}

Error
ProjectImpl::open_project (const String &utf8path)
{
  String filename = Path::realpath (decodefs (utf8path));
  if (errno)
    return ase_error_from_errno (errno);
  if (Path::check (filename, "d"))
    filename = Path::join (filename, "project.anklang");
  if (Path::basename (filename) != "project.anklang")
    return Error::BAD_PROJECT;
  const String dir = Path::dirname (filename);
  for (const auto &project : g_projects)
    if (!project->project_dir_.empty() && Path::equals (dir, project->project_dir_))
      return Error::OPERATION_BUSY;
  StorageReader reader;
  Error error = reader.open_for_reading (filename);
  if (!!error)
    return error;
  if (reader.stringread ("mimetype") != "application/x-anklang")
    return Error::BAD_PROJECT;
  const String xml = reader.stringread ("edit.tracktionedit");
  if (!from_xml (xml, dir))
    return Error::FORMAT_INVALID;
  return Error::NONE;
}

String
ProjectImpl::grep_serialized (const String &regex, int group)
{
  return_unless (!!edit_, "");
  for (te::Plugin *plugin : te::getAllPlugins (*edit_, true))
    edit_->flushPluginStateIfNeeded (*plugin);
  return Re::grep (regex, edit_xml (edit_->state), group);
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
  CoalesceNotifies coalesce_notifies;
  if (edit_->getUndoManager().undo()) {
    trkn_restore_parameter_values (*edit_);
    emit_notify ("dirty");
  }
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
  CoalesceNotifies coalesce_notifies;
  if (edit_->getUndoManager().redo()) {
    trkn_restore_parameter_values (*edit_);
    emit_notify ("dirty");
  }
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

void
ProjectImpl::length (double)
{
  warning ("Project.length is read-only");
}

StringS
ProjectImpl::read_only_properties () const
{
  return { "all_tracks", "length", "project_dir", "unnamed", "unresolved_assets" };
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
  edit_->tempoSequence.getTempo (0)->setBpm (newbpm);
}

double
ProjectImpl::bpm () const
{
  return edit_->tempoSequence.getTempo (0)->getBpm();
}

void
ProjectImpl::numerator (int num)
{
  edit_->tempoSequence.getTimeSig (0)->numerator = num;
}

int
ProjectImpl::numerator () const
{
  return edit_->tempoSequence.getTimeSig (0)->numerator;
}

void
ProjectImpl::denominator (int den)
{
  edit_->tempoSequence.getTimeSig (0)->denominator = den;
}

int
ProjectImpl::denominator () const
{
  return edit_->tempoSequence.getTimeSig (0)->denominator;
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
  int n = te::getAudioTracks (*edit_).size();
  while (trkn_has_track_name (*edit_, "Track " + juce::String (n)))
    n++;
  t->setName ("Track " + juce::String (n));
  TrackImplP track = TrackImpl::from_trkn (*t);
  emit_event ("track", "insert", { { "track", track }, });
  emit_notify ("all_tracks");
  return track;
}

TrackS
ProjectImpl::all_tracks () const
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

void
ProjectImpl::all_tracks (const TrackS &)
{
  warning ("Project.all_tracks is read-only");
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
