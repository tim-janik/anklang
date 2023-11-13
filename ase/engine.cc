// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "engine.hh"
#include "processor.hh"
#include "utils.hh"
#include "loop.hh"
#include "driver.hh"
#include "server.hh"
#include "datautils.hh"
#include "atomics.hh"
#include "project.hh"
#include "wave.hh"
#include "main.hh"      // main_loop_autostop_mt
#include "memory.hh"
#include "internal.hh"

#define EDEBUG(...)             Ase::debug ("engine", __VA_ARGS__)

namespace Ase {

constexpr const uint FIXED_N_CHANNELS = 2;
constexpr const uint FIXED_SAMPLE_RATE = 48000;
constexpr const uint FIXED_N_MIDI_DRIVERS = 4;

// == decls ==
using VoidFunc = std::function<void()>;
using StartQueue = AsyncBlockingQueue<char>;
ASE_CLASS_DECLS (EngineMidiInput);
static void apply_driver_preferences ();

// == EngineJobImpl ==
struct EngineJobImpl {
  VoidFunc func;
  std::atomic<EngineJobImpl*> next = nullptr;
  explicit EngineJobImpl (const VoidFunc &jobfunc) : func (jobfunc) {}
};
static inline std::atomic<EngineJobImpl*>&
atomic_next_ptrref (EngineJobImpl *j)
{
  return j->next;
}

struct DriverSet {
  PcmDriverP  null_pcm_driver;
  String      pcm_name;
  PcmDriverP  pcm_driver;
  StringS     midi_names;
  MidiDriverS midi_drivers;
};

// == AudioEngineThread ==
class AudioEngineThread : public AudioEngine {
public:
  static constexpr uint        fixed_n_channels = 2;
  PcmDriverP                   null_pcm_driver_, pcm_driver_;
  constexpr static size_t      MAX_BUFFER_SIZE = AUDIO_BLOCK_MAX_RENDER_SIZE;
  size_t                       buffer_size_ = MAX_BUFFER_SIZE; // mono buffer size
  float                        chbuffer_data_[MAX_BUFFER_SIZE * fixed_n_channels] = { 0, };
  uint64                       write_stamp_ = 0, render_stamp_ = MAX_BUFFER_SIZE;
  std::vector<AudioProcessor*> schedule_;
  EngineMidiInputP             midi_proc_;
  bool                         schedule_invalid_ = true;
  bool                         output_needsrunning_ = false;
  AtomicIntrusiveStack<EngineJobImpl> async_jobs_, const_jobs_, trash_jobs_;
  const VoidF                  owner_wakeup_;
  std::thread                 *thread_ = nullptr;
  MainLoopP                    event_loop_ = MainLoop::create();
  AudioProcessorS              oprocs_;
  ProjectImplP                 project_;
  WaveWriterP                  wwriter_;
  FastMemory::Block            transport_block_;
  DriverSet                    driver_set_ml; // accessed by main_loop thread
  std::atomic<uint64>          autostop_ = U64MAX;
  struct UserNoteJob {
    std::atomic<UserNoteJob*> next = nullptr;
    UserNote note;
  };
  AtomicIntrusiveStack<UserNoteJob> user_notes_;
public:
  virtual        ~AudioEngineThread      ();
  explicit        AudioEngineThread      (const VoidF&, uint, SpeakerArrangement, const FastMemory::Block&);
  void            schedule_clear         ();
  void            schedule_add           (AudioProcessor &aproc, uint level);
  void            schedule_queue_update  ();
  uint64          frame_counter          () const               { return render_stamp_; }
  void            schedule_render        (uint64 frames);
  void            enable_output          (AudioProcessor &aproc, bool onoff);
  void            wakeup_thread_mt       ();
  void            capture_start          (const String &filename, bool needsrunning);
  void            capture_stop           ();
  bool            ipc_pending            ();
  void            ipc_dispatch           ();
  AudioProcessorP get_event_source       ();
  void            add_job_mt             (EngineJobImpl *aejob, const AudioEngine::JobQueue *jobqueue);
  bool            pcm_check_write        (bool write_buffer, int64 *timeout_usecs_p = nullptr);
  bool            driver_dispatcher      (const LoopState &state);
  bool            process_jobs           (AtomicIntrusiveStack<EngineJobImpl> &joblist);
  void            run                    (StartQueue *sq);
  void            queue_user_note        (const String &channel, UserNote::Flags flags, const String &text);
  void            set_project            (ProjectImplP project);
  ProjectImplP    get_project            ();
  void            update_driver_set      (DriverSet &dset);
  void            start_threads_ml       ();
  void            stop_threads_ml        ();
  void            create_processors_ml   ();
  String          engine_stats_string    (uint64_t stats) const;
};

static std::thread::id audio_engine_thread_id = {};
const ThreadId &AudioEngine::thread_id = audio_engine_thread_id;

static inline std::atomic<AudioEngineThread::UserNoteJob*>&
atomic_next_ptrref (AudioEngineThread::UserNoteJob *j)
{
  return j->next;
}

template<int ADDING> static void
interleaved_stereo (const size_t n_frames, float *buffer, AudioProcessor &proc, OBusId obus)
{
  if (proc.n_ochannels (obus) >= 2)
    {
      const float *src0 = proc.ofloats (obus, 0);
      const float *src1 = proc.ofloats (obus, 1);
      float *d = buffer, *const b = d + n_frames;
      do {
        if_constexpr (ADDING == 0)
          {
            *d++ = *src0++;
            *d++ = *src1++;
          }
        else
          {
            *d++ += *src0++;
            *d++ += *src1++;
          }
      } while (d < b);
    }
  else if (proc.n_ochannels (obus) >= 1)
    {
      const float *src = proc.ofloats (obus, 0);
      float *d = buffer, *const b = d + n_frames;
      do {
        if_constexpr (ADDING == 0)
          {
            *d++ = *src;
            *d++ = *src++;
          }
        else
          {
            *d++ += *src;
            *d++ += *src++;
          }
      } while (d < b);
    }
}

void
AudioEngineThread::schedule_queue_update()
{
  schedule_invalid_ = true;
}

void
AudioEngineThread::schedule_clear()
{
  while (schedule_.size() != 0)
    {
      AudioProcessor *cur = schedule_.back();
      schedule_.pop_back();
      while (cur)
        {
          AudioProcessor *const proc = cur;
          cur = proc->sched_next_;
          proc->flags_ &= ~AudioProcessor::SCHEDULED;
          proc->sched_next_ = nullptr;
        }
    }
  schedule_invalid_ = true;
}

void
AudioEngineThread::schedule_add (AudioProcessor &aproc, uint level)
{
  return_unless (0 == (aproc.flags_ & AudioProcessor::SCHEDULED));
  assert_return (aproc.sched_next_ == nullptr);
  if (schedule_.size() <= level)
    schedule_.resize (level + 1);
  aproc.sched_next_ = schedule_[level];
  schedule_[level] = &aproc;
  aproc.flags_ |= AudioProcessor::SCHEDULED;
  if (aproc.render_stamp_ != render_stamp_)
    aproc.reset_state (render_stamp_);
}

void
AudioEngineThread::schedule_render (uint64 frames)
{
  assert_return (0 == (frames & (8 - 1)));
  // render scheduled AudioProcessor nodes
  const uint64 target_stamp = render_stamp_ + frames;
  for (size_t l = 0; l < schedule_.size(); l++)
    {
      AudioProcessor *proc = schedule_[l];
      while (proc)
        {
          proc->render_block (target_stamp);
          proc = proc->sched_next_;
        }
    }
  // render output buffer interleaved
  constexpr auto MAIN_OBUS = OBusId (1);
  size_t n = 0;
  for (size_t i = 0; i < oprocs_.size(); i++)
    if (oprocs_[i]->n_obuses())
      {
        if (n++ == 0)
          interleaved_stereo<0> (buffer_size_ * fixed_n_channels, chbuffer_data_, *oprocs_[i], MAIN_OBUS);
        else
          interleaved_stereo<1> (buffer_size_ * fixed_n_channels, chbuffer_data_, *oprocs_[i], MAIN_OBUS);
        static_assert (2 == fixed_n_channels);
      }
  if (n == 0)
    floatfill (chbuffer_data_, 0.0, buffer_size_ * fixed_n_channels);
  render_stamp_ = target_stamp;
  transport_.advance (frames);
}

void
AudioEngineThread::enable_output (AudioProcessor &aproc, bool onoff)
{
  AudioProcessorP procp = shared_ptr_cast<AudioProcessor> (&aproc);
  assert_return (procp != nullptr);
  if (onoff && !(aproc.flags_ & AudioProcessor::ENGINE_OUTPUT))
    {
      oprocs_.push_back (procp);
      aproc.flags_ |= AudioProcessor::ENGINE_OUTPUT;
      schedule_queue_update();
    }
  else if (!onoff && (aproc.flags_ & AudioProcessor::ENGINE_OUTPUT))
    {
      const bool foundproc = Aux::erase_first (oprocs_, [procp] (AudioProcessorP c) { return c == procp; });
      aproc.flags_ &= ~AudioProcessor::ENGINE_OUTPUT;
      schedule_queue_update();
      assert_return (foundproc);
    }
}

void
AudioEngineThread::capture_start (const String &filename, bool needsrunning)
{
  const uint sample_rate = transport_.samplerate;
  capture_stop();
  output_needsrunning_ = needsrunning;
  if (string_endswith (filename, ".wav"))
    {
      wwriter_ = wave_writer_create_wav (sample_rate, fixed_n_channels, filename);
      if (!wwriter_)
        printerr ("%s: failed to open file: %s\n", filename, strerror (errno));
    }
  else if (string_endswith (filename, ".opus"))
    {
      wwriter_ = wave_writer_create_opus (sample_rate, fixed_n_channels, filename);
      if (!wwriter_)
        printerr ("%s: failed to open file: %s\n", filename, strerror (errno));
    }
  else if (string_endswith (filename, ".flac"))
    {
      wwriter_ = wave_writer_create_flac (sample_rate, fixed_n_channels, filename);
      if (!wwriter_)
        printerr ("%s: failed to open file: %s\n", filename, strerror (errno));
    }
  else if (!filename.empty())
    printerr ("%s: unknown sample file: %s\n", filename, strerror (ENOSYS));
}

void
AudioEngineThread::capture_stop()
{
  if (wwriter_)
    {
      wwriter_->close();
      wwriter_ = nullptr;
    }
}

void
AudioEngineThread::run (StartQueue *sq)
{
  assert_return (null_pcm_driver_);
  if (!pcm_driver_)
    pcm_driver_ = null_pcm_driver_;
  floatfill (chbuffer_data_, 0.0, MAX_BUFFER_SIZE * fixed_n_channels);
  buffer_size_ = std::min (MAX_BUFFER_SIZE, size_t (pcm_driver_->pcm_block_length()));
  write_stamp_ = render_stamp_ - buffer_size_; // write an initial buffer of zeros
  // FIXME: assert owner_wakeup and free trash
  this_thread_set_name ("AudioEngine-0"); // max 16 chars
  audio_engine_thread_id = std::this_thread::get_id();
  sched_fast_priority (this_thread_gettid());
  event_loop_->exec_dispatcher (std::bind (&AudioEngineThread::driver_dispatcher, this, std::placeholders::_1));
  sq->push ('R'); // StartQueue becomes invalid after this call
  sq = nullptr;
  event_loop_->run();
}

bool
AudioEngineThread::process_jobs (AtomicIntrusiveStack<EngineJobImpl> &joblist)
{
  EngineJobImpl *const jobs = joblist.pop_reversed(), *last = nullptr;
  for (EngineJobImpl *job = jobs; job; last = job, job = job->next)
    job->func();
  if (last)
    {
      if (trash_jobs_.push_chain (jobs, last))
        owner_wakeup_();
    }
  return last != nullptr;
}

bool
AudioEngineThread::pcm_check_write (bool write_buffer, int64 *timeout_usecs_p)
{
  int64 timeout_usecs = INT64_MAX;
  const bool can_write = pcm_driver_->pcm_check_io (&timeout_usecs) || timeout_usecs == 0;
  if (timeout_usecs_p)
    *timeout_usecs_p = timeout_usecs;
  if (!write_buffer)
    return can_write;
  if (!can_write || write_stamp_ >= render_stamp_)
    return false;
  pcm_driver_->pcm_write (buffer_size_ * fixed_n_channels, chbuffer_data_);
  if (wwriter_ && fixed_n_channels == 2 && write_stamp_ < autostop_ &&
      (!output_needsrunning_ || transport_.running()))
    wwriter_->write (chbuffer_data_, buffer_size_);
  write_stamp_ += buffer_size_;
  if (write_stamp_ >= autostop_)
    main_loop_autostop_mt();
  assert_warn (write_stamp_ == render_stamp_);
  return false;
}

bool
AudioEngineThread::driver_dispatcher (const LoopState &state)
{
  int64 *timeout_usecs = nullptr;
  switch (state.phase)
    {
    case LoopState::PREPARE:
      timeout_usecs = const_cast<int64*> (&state.timeout_usecs);
      /* fall-through */
    case LoopState::CHECK:
      if (atquit_triggered())
        return false;   // stall engine once program is aborted
      if (!const_jobs_.empty() || !async_jobs_.empty())
        return true; // jobs pending
      if (render_stamp_ <= write_stamp_)
        return true; // must render
      // FIXME: add pcm driver pollfd with 1-block threshold
      return pcm_check_write (false, timeout_usecs);
    case LoopState::DISPATCH:
      pcm_check_write (true);
      if (render_stamp_ <= write_stamp_)
        {
          process_jobs (async_jobs_); // apply pending modifications before render
          if (schedule_invalid_)
            {
              schedule_clear();
              for (AudioProcessorP &proc : oprocs_)
                proc->schedule_processor();
              schedule_invalid_ = false;
            }
          if (render_stamp_ <= write_stamp_) // async jobs may have adjusted stamps
            schedule_render (buffer_size_);
          pcm_check_write (true); // minimize drop outs
        }
      if (!const_jobs_.empty()) {   // owner may be blocking for const_jobs_ execution
        process_jobs (async_jobs_); // apply pending modifications first
        process_jobs (const_jobs_);
      }
      if (ipc_pending())
        owner_wakeup_(); // owner needs to ipc_dispatch()
      return true; // keep alive
    default: ;
    }
  return false;
}

void
AudioEngineThread::queue_user_note (const String &channel, UserNote::Flags flags, const String &text)
{
  UserNoteJob *uj = new UserNoteJob { nullptr, { 0, flags, channel, text } };
  if (user_notes_.push (uj))
    owner_wakeup_();
}

bool
AudioEngineThread::ipc_pending ()
{
  const bool have_jobs = !trash_jobs_.empty() || !user_notes_.empty();
  return have_jobs || AudioProcessor::enotify_pending();
}

void
AudioEngineThread::ipc_dispatch ()
{
  UserNoteJob *uj = user_notes_.pop_reversed();
  while (uj)
    {
      ASE_SERVER.user_note (uj->note.text, uj->note.channel, uj->note.flags);
      UserNoteJob *const old = uj;
      uj = old->next;
      delete old;
    }
  if (AudioProcessor::enotify_pending())
    AudioProcessor::enotify_dispatch();
  EngineJobImpl *job = trash_jobs_.pop_all();
  while (job)
    {
      EngineJobImpl *old = job;
      job = job->next;
      delete old;
    }
}

void
AudioEngineThread::wakeup_thread_mt()
{
  assert_return (event_loop_);
  event_loop_->wakeup();
}

void
AudioEngineThread::start_threads_ml()
{
  assert_return (this_thread_is_ase()); // main_loop thread
  assert_return (thread_ == nullptr);
  assert_return (midi_proc_ == nullptr);
  schedule_.reserve (8192);
  create_processors_ml();
  update_drivers ("null", 0, {}); // create drivers
  null_pcm_driver_ = driver_set_ml.null_pcm_driver;
  schedule_queue_update();
  StartQueue start_queue;
  thread_ = new std::thread (&AudioEngineThread::run, this, &start_queue);
  const char reply = start_queue.pop(); // synchronize with thread start
  assert_return (reply == 'R');
  apply_driver_preferences();
}

void
AudioEngineThread::stop_threads_ml()
{
  assert_return (this_thread_is_ase()); // main_loop thread
  assert_return (thread_ != nullptr);
  event_loop_->quit (0);
  thread_->join();
  audio_engine_thread_id = {};
  auto oldthread = thread_;
  thread_ = nullptr;
  delete oldthread;
}

void
AudioEngineThread::add_job_mt (EngineJobImpl *job, const AudioEngine::JobQueue *jobqueue)
{
  assert_return (job != nullptr);
  AudioEngineThread &engine = *dynamic_cast<AudioEngineThread*> (this);
  // engine not running, run job right away
  if (!engine.thread_)
    {
      job->func();
      delete job;
      return;
    }
  // enqueue async_jobs
  if (jobqueue == &async_jobs)  // non-blocking, via async_jobs_ queue
    { // run asynchronously
      const bool was_empty = engine.async_jobs_.push (job);
      if (was_empty)
        wakeup_thread_mt();
      return;
    }
  // blocking jobs, queue wrapper that synchronizes via Semaphore
  ScopedSemaphore sem;
  VoidFunc jobfunc = job->func;
  std::function<void()> wrapper = [&sem, jobfunc] () {
    jobfunc();
    sem.post();
  };
  job->func = wrapper;
  bool need_wakeup;
  if (jobqueue == &const_jobs)  // blocking, via const_jobs_ queue
    need_wakeup = engine.const_jobs_.push (job);
  else if (jobqueue == &synchronized_jobs)  // blocking, via async_jobs_ queue
    need_wakeup = engine.async_jobs_.push (job);
  else
    assert_unreached();
  if (need_wakeup)
    wakeup_thread_mt();
  sem.wait();
}

void
AudioEngineThread::set_project (ProjectImplP project)
{
  if (project)
    {
      assert_return (project_ == nullptr);
      assert_return (!project->is_active());
    }
  if (project_)
    project_->_deactivate();
  const ProjectImplP old = project_;
  project_ = project;
  if (project_)
    project_->_activate();
  // dtor of old runs here
}

String
AudioEngineThread::engine_stats_string (uint64_t stats) const
{
  String s;
  for (size_t i = 0; i < oprocs_.size(); i++) {
    AudioProcessorInfo pinfo;
    pinfo.label = "INTERNAL";
    AudioProcessor::registry_foreach ([&] (const String &aseid, AudioProcessor::StaticInfo static_info) {
      if (aseid == oprocs_[i]->aseid_)
        static_info (pinfo); // TODO: this is a bit awkward to fetch AudioProcessorInfo for an AudioProcessor
    });
    s += string_format ("%s: %s (MUST_SCHEDULE)\n", pinfo.label, oprocs_[i]->debug_name());
  }
  return s;
}

ProjectImplP
AudioEngineThread::get_project ()
{
  return project_;
}

AudioEngineThread::~AudioEngineThread ()
{
  FastMemory::Block transport_block = transport_block_; // keep alive until after ~AudioEngine
  main_jobs += [transport_block] () { ServerImpl::instancep()->telemem_release (transport_block); };
}

AudioEngineThread::AudioEngineThread (const VoidF &owner_wakeup, uint sample_rate, SpeakerArrangement speakerarrangement,
                                      const FastMemory::Block &transport_block) :
  AudioEngine (*this, *new (transport_block.block_start) AudioTransport (speakerarrangement, sample_rate)),
  owner_wakeup_ (owner_wakeup), transport_block_ (transport_block)
{
  oprocs_.reserve (16);
  assert_return (transport_.samplerate == 48000);
}

AudioEngine&
make_audio_engine (const VoidF &owner_wakeup, uint sample_rate, SpeakerArrangement speakerarrangement)
{
  ASE_ASSERT_ALWAYS (sample_rate == FIXED_SAMPLE_RATE);
  ASE_ASSERT_ALWAYS (speaker_arrangement_count_channels (speakerarrangement) == FIXED_N_CHANNELS);
  FastMemory::Block transport_block = ServerImpl::instancep()->telemem_allocate (sizeof (AudioTransport));
  return *new AudioEngineThread (owner_wakeup, sample_rate, speakerarrangement, transport_block);
}

// == AudioEngine ==
AudioEngine::AudioEngine (AudioEngineThread &audio_engine_thread, AudioTransport &transport) :
  transport_ (transport), async_jobs (audio_engine_thread), const_jobs (audio_engine_thread), synchronized_jobs (audio_engine_thread)
{}

AudioEngine::~AudioEngine()
{
  // some ref-counted objects keep AudioEngine& members around
  fatal_error ("AudioEngine must not be destroyed");
}

String
AudioEngine::engine_stats (uint64_t stats) const
{
  String strstats;
  const AudioEngineThread &engine_thread = static_cast<const AudioEngineThread&> (*this);
  const_cast<AudioEngine*> (this)->synchronized_jobs += [&] () { strstats = engine_thread.engine_stats_string (stats); };
  return strstats;
}

uint64
AudioEngine::frame_counter () const
{
  const AudioEngineThread &impl = static_cast<const AudioEngineThread&> (*this);
  return impl.frame_counter();
}

void
AudioEngine::set_autostop (uint64_t nsamples)
{
  AudioEngineThread &impl = static_cast<AudioEngineThread&> (*this);
  impl.autostop_ = nsamples;
}

void
AudioEngine::schedule_queue_update()
{
  AudioEngineThread &impl = static_cast<AudioEngineThread&> (*this);
  impl.schedule_queue_update();
}

void
AudioEngine::schedule_add (AudioProcessor &aproc, uint level)
{
  AudioEngineThread &impl = static_cast<AudioEngineThread&> (*this);
  impl.schedule_add (aproc, level);
}

void
AudioEngine::enable_output (AudioProcessor &aproc, bool onoff)
{
  AudioEngineThread &impl = static_cast<AudioEngineThread&> (*this);
  return impl.enable_output (aproc, onoff);
}

void
AudioEngine::start_threads()
{
  AudioEngineThread &impl = static_cast<AudioEngineThread&> (*this);
  return impl.start_threads_ml();
}

void
AudioEngine::stop_threads()
{
  AudioEngineThread &impl = static_cast<AudioEngineThread&> (*this);
  return impl.stop_threads_ml();
}

void
AudioEngine::queue_capture_start (CallbackS &callbacks, const String &filename, bool needsrunning)
{
  AudioEngineThread *impl = static_cast<AudioEngineThread*> (this);
  String file = filename;
  callbacks.push_back ([impl,file,needsrunning] () {
    impl->capture_start (file, needsrunning);
  });
}

void
AudioEngine::queue_capture_stop (CallbackS &callbacks)
{
  AudioEngineThread *impl = static_cast<AudioEngineThread*> (this);
  callbacks.push_back ([impl] () {
    impl->capture_stop();
  });
}

void
AudioEngine::wakeup_thread_mt ()
{
  AudioEngineThread &impl = static_cast<AudioEngineThread&> (*this);
  return impl.wakeup_thread_mt();
}

bool
AudioEngine::ipc_pending ()
{
  AudioEngineThread &impl = static_cast<AudioEngineThread&> (*this);
  return impl.ipc_pending();
}

void
AudioEngine::ipc_dispatch ()
{
  AudioEngineThread &impl = static_cast<AudioEngineThread&> (*this);
  return impl.ipc_dispatch();
}

AudioProcessorP
AudioEngine::get_event_source ()
{
  AudioEngineThread &impl = static_cast<AudioEngineThread&> (*this);
  return impl.get_event_source();
}

void
AudioEngine::set_project (ProjectImplP project)
{
  AudioEngineThread &impl = static_cast<AudioEngineThread&> (*this);
  return impl.set_project (project);
}

ProjectImplP
AudioEngine::get_project ()
{
  AudioEngineThread &impl = static_cast<AudioEngineThread&> (*this);
  return impl.get_project();
}

AudioEngine::JobQueue::JobQueue (AudioEngine &aet) :
  queue_tag_ (ptrdiff_t (this) - ptrdiff_t (&aet))
{
  assert_return (ptrdiff_t (this) < 256 + ptrdiff_t (&aet));
}

void
AudioEngine::JobQueue::operator+= (const std::function<void()> &job)
{
  AudioEngine *audio_engine = reinterpret_cast<AudioEngine*> (ptrdiff_t (this) - queue_tag_);
  AudioEngineThread &audio_engine_thread = static_cast<AudioEngineThread&> (*audio_engine);
  return audio_engine_thread.add_job_mt (new EngineJobImpl (job), this);
}

bool
AudioEngine::update_drivers (const String &pcm_name, uint latency_ms, const StringS &midi_prefs)
{
  AudioEngineThread &engine_thread = static_cast<AudioEngineThread&> (*this);
  DriverSet &dset = engine_thread.driver_set_ml;
  const char *const null_driver = "null";
  int must_update = 0;
  // PCM Config
  const PcmDriverConfig pcm_config { .n_channels = engine_thread.fixed_n_channels, .mix_freq = FIXED_SAMPLE_RATE,
                                     .block_length = AUDIO_BLOCK_MAX_RENDER_SIZE, .latency_ms = latency_ms };
  // PCM Fallback
  if (!dset.null_pcm_driver) {
    must_update++;
    Error er = {};
    dset.null_pcm_driver = PcmDriver::open (null_driver, Driver::WRITEONLY, Driver::WRITEONLY, pcm_config, &er);
    if (!dset.null_pcm_driver || er != 0)
      fatal_error ("failed to open internal PCM driver ('%s'): %s", null_driver, ase_error_blurb (er));
  }
  // PCM Driver
  if (pcm_name != dset.pcm_name) {
    must_update++;
    dset.pcm_name = pcm_name;
    Error er = {};
    dset.pcm_driver = dset.pcm_name == null_driver ? dset.null_pcm_driver :
                      PcmDriver::open (dset.pcm_name, Driver::WRITEONLY, Driver::WRITEONLY, pcm_config, &er);
    if (!dset.pcm_driver || er != 0) {
      dset.pcm_driver = dset.null_pcm_driver;
      const String errmsg = string_format ("# Audio I/O Error\n" "Failed to open audio device:\n" "%s:\n" "%s",
                                           dset.pcm_name, ase_error_blurb (er));
      engine_thread.queue_user_note ("driver.pcm", UserNote::CLEAR, errmsg);
      printerr ("%s\n", string_replace (errmsg, "\n", " "));
    }
  }
  // Deduplicate MIDI Drivers
  StringS midis = midi_prefs;
  midis.resize (FIXED_N_MIDI_DRIVERS);
  for (size_t i = 0; i < midis.size(); i++)
    if (midis[i].empty())
      midis[i] = null_driver;
    else
      for (size_t j = 0; j < i; j++) // dedup
        if (midis[i] != null_driver && midis[i] == midis[j]) {
          midis[i] = null_driver;
          break;
        }
  // MIDI Drivers
  dset.midi_names.resize (midis.size());
  dset.midi_drivers.resize (dset.midi_names.size());
  for (size_t i = 0; i < dset.midi_drivers.size(); i++) {
    if (midis[i] == dset.midi_names[i])
      continue;
    must_update++;
    dset.midi_names[i] = midis[i];
    Error er = {};
    dset.midi_drivers[i] = dset.midi_names[i] == null_driver ? nullptr :
                           MidiDriver::open (dset.midi_names[i], Driver::READONLY, &er);
    if (er != 0) {
      dset.midi_drivers[i] = nullptr;
      const String errmsg = string_format ("# MIDI I/O Error\n" "Failed to open MIDI device #%u:\n" "%s:\n" "%s",
                                           1 + i, dset.midi_names[i], ase_error_blurb (er));
      engine_thread.queue_user_note ("driver.midi", UserNote::CLEAR, errmsg);
      printerr ("%s\n", string_replace (errmsg, "\n", " "));
    }
  }
  // Update running engine
  if (must_update) {
    Mutable<DriverSet> mdset = dset; // use Mutable so Job can swap and the remains are cleaned up in ~Job
    synchronized_jobs += [mdset,&engine_thread] () { engine_thread.update_driver_set (mdset.value); };
    return true;
  }
  return false;
}

// == EngineMidiInput ==
class EngineMidiInput : public AudioProcessor {
  // Processor providing MIDI device events
  void
  initialize (SpeakerArrangement busses) override
  {
    prepare_event_output();
  }
  void
  reset (uint64 target_stamp) override
  {
    MidiEventOutput &estream = get_event_output();
    estream.clear();
    estream.reserve (256);
  }
  void
  render (uint n_frames) override
  {
    MidiEventOutput &estream = get_event_output();
    estream.clear();
    for (size_t i = 0; i < midi_drivers_.size(); i++)
      if (midi_drivers_[i])
        midi_drivers_[i]->fetch_events (estream, sample_rate());
  }
public:
  MidiDriverS midi_drivers_;
  EngineMidiInput (const ProcessorSetup &psetup) :
    AudioProcessor (psetup)
  {}
};

void
AudioEngineThread::create_processors_ml ()
{
  assert_return (this_thread_is_ase()); // main_loop thread
  assert_return (midi_proc_ == nullptr);
  AudioProcessorP aprocp = AudioProcessor::create_processor<EngineMidiInput> (*this);
  assert_return (aprocp);
  midi_proc_ = std::dynamic_pointer_cast<EngineMidiInput> (aprocp);
  assert_return (midi_proc_);
  EngineMidiInputP midi_proc = midi_proc_;
  async_jobs += [midi_proc] () {
    midi_proc->enable_engine_output (true); // MUST_SCHEDULE
  };
}

AudioProcessorP
AudioEngineThread::get_event_source ()
{
  return midi_proc_;
}

void
AudioEngineThread::update_driver_set (DriverSet &dset)
{
  // use swap() to defer dtor to user thread
  assert_return (midi_proc_);
  // PCM Driver
  if (pcm_driver_ != dset.pcm_driver) {
    pcm_driver_.swap (dset.pcm_driver);
    floatfill (chbuffer_data_, 0.0, MAX_BUFFER_SIZE * fixed_n_channels);
    buffer_size_ = std::min (MAX_BUFFER_SIZE, size_t (pcm_driver_->pcm_block_length()));
    write_stamp_ = render_stamp_ - buffer_size_; // write an initial buffer of zeros
    EDEBUG ("AudioEngineThread::%s: update PCM to \"%s\": channels=%d pcmblock=%d enginebuffer=%d ws=%u rs=%u bs=%u\n", __func__,
            dset.pcm_name, fixed_n_channels, pcm_driver_->pcm_block_length(), buffer_size_, write_stamp_, render_stamp_, buffer_size_);
  }
  // MIDI Drivers
  if (midi_proc_->midi_drivers_ != dset.midi_drivers) {
    midi_proc_->midi_drivers_.swap (dset.midi_drivers);
    EDEBUG ("AudioEngineThread::%s: swapping %u MIDI drivers: \"%s\"\n", __func__, midi_proc_->midi_drivers_.size(), string_join ("\" \"", dset.midi_names));
  }
}

// == DriverSet ==
static Choice
choice_from_driver_entry (const DriverEntry &e, const String &icon_keywords)
{
  String blurb;
  if (!e.device_info.empty() && !e.capabilities.empty())
    blurb = e.capabilities + "\n" + e.device_info;
  else if (!e.capabilities.empty())
    blurb = e.capabilities;
  else
    blurb = e.device_info;
  Choice c (e.devid, e.device_name, blurb);
  if (string_startswith (string_tolower (e.notice), "warn"))
    c.warning = e.notice;
  else
    c.notice = e.notice;
  // e.priority
  // e.readonly
  // e.writeonly
  // e.modem
  c.icon = MakeIcon::KwIcon (icon_keywords + "," + e.hints);
  return c;
}

static ChoiceS
pcm_driver_pref_list_choices (const CString &ident)
{
  static ChoiceS choices;
  static uint64 cache_age = 0;
  if (choices.empty() || timestamp_realtime() > cache_age + 500 * 1000) {
    choices.clear();
    for (const DriverEntry &e : PcmDriver::list_drivers())
      choices.push_back (choice_from_driver_entry (e, "pcm"));
    cache_age = timestamp_realtime();
  }
  return choices;
}

static ChoiceS
midi_driver_pref_list_choices (const CString &ident)
{
  static ChoiceS choices;
  static uint64 cache_age = 0;
  if (choices.empty() || timestamp_realtime() > cache_age + 500 * 1000) {
    choices.clear();
    for (const DriverEntry &e : MidiDriver::list_drivers())
      if (!e.writeonly)
        choices.push_back (choice_from_driver_entry (e, "midi"));
    cache_age = timestamp_realtime();
  }
  return choices;
}

static Preference pcm_driver_pref =
  Preference ({
      "driver.pcm.devid", _("PCM Driver"), "", "auto", "ms",
      { pcm_driver_pref_list_choices }, STANDARD, "",
      _("Driver and device to be used for PCM input and output"), },
    [] (const CString&,const Value&) { apply_driver_preferences(); });

static Preference synth_latency_pref =
  Preference ({
      "driver.pcm.synth_latency", _("Synth Latency"), "", 15, "ms",
      MinMaxStep { 0, 3000, 5 }, STANDARD + String ("step=5"), "",
      _("Processing duration between input and output of a single sample, smaller values increase CPU load") },
    [] (const CString&,const Value&) { apply_driver_preferences(); });

static Preference midi1_driver_pref =
  Preference ({
      "driver.midi1.devid", _("MIDI Controller (1)"), "", "auto", "ms",
      { midi_driver_pref_list_choices }, STANDARD, "",
      _("MIDI controller device to be used for MIDI input"), },
    [] (const CString&,const Value&) { apply_driver_preferences(); });
static Preference midi2_driver_pref =
  Preference ({
      "driver.midi2.devid", _("MIDI Controller (2)"), "", "auto", "ms",
      { midi_driver_pref_list_choices }, STANDARD, "",
      _("MIDI controller device to be used for MIDI input"), },
    [] (const CString&,const Value&) { apply_driver_preferences(); });
static Preference midi3_driver_pref =
  Preference ({
      "driver.midi3.devid", _("MIDI Controller (3)"), "", "auto", "ms",
      { midi_driver_pref_list_choices }, STANDARD, "",
      _("MIDI controller device to be used for MIDI input"), },
    [] (const CString&,const Value&) { apply_driver_preferences(); });
static Preference midi4_driver_pref =
  Preference ({
      "driver.midi4.devid", _("MIDI Controller (4)"), "", "auto", "ms",
      { midi_driver_pref_list_choices }, STANDARD, "",
      _("MIDI controller device to be used for MIDI input"), },
    [] (const CString&,const Value&) { apply_driver_preferences(); });

static void
apply_driver_preferences ()
{
  static uint engine_driver_set_timerid = 0;
  main_loop->exec_once (97, &engine_driver_set_timerid,
                        []() {
                          StringS midis = { midi1_driver_pref.gets(), midi2_driver_pref.gets(), midi3_driver_pref.gets(), midi4_driver_pref.gets(), };
                          main_config.engine->update_drivers (pcm_driver_pref.gets(), synth_latency_pref.getn(), midis);
                        });
}

} // Ase
