// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "engine.hh"
#include "processor.hh"
#include "utils.hh"
#include "loop.hh"
#include "driver.hh"
#include "server.hh"
#include "datautils.hh"
#include "atomics.hh"
#include "internal.hh"

namespace Ase {

using VoidFunc = std::function<void()>;
using StartQueue = AsyncBlockingQueue<char>;
ASE_CLASS_DECLS (EngineMidiInput);
constexpr uint fixed_sample_rate = 48000;

// == AudioEngineThread ==
class AudioEngineThread : public AudioEngine {
  PcmDriverP                   null_pcm_driver_, pcm_driver_;
  MidiDriverS                  midi_drivers_;
  static constexpr uint        fixed_n_channels = 2;
  constexpr static size_t      buffer_size_ = AUDIO_BLOCK_MAX_RENDER_SIZE * fixed_n_channels;
  float                        buffer_data_[buffer_size_] = { 0, };
  uint64                       write_stamp_ = 0, render_stamp_ = AUDIO_BLOCK_MAX_RENDER_SIZE;
  std::vector<AudioProcessor*> schedule_;
  bool                         schedule_invalid_ = true;
  EngineMidiInputP             midi_proc_;
  JobQueue                     synchronized_jobs;
public:
  struct Job {
    std::atomic<Job*> next = nullptr;
    VoidFunc func;
  };
  AtomicIntrusiveStack<Job> async_jobs_, const_jobs_, trash_jobs_;
  VoidF                     owner_wakeup_;
  std::thread              *thread_ = nullptr;
  MainLoopP                 event_loop_ = MainLoop::create();
  AudioProcessorS           oprocs_;
  virtual        ~AudioEngineThread     ();
  explicit        AudioEngineThread     (uint sample_rate, SpeakerArrangement speakerarrangement);
  uint64          frame_counter         () const override      { return render_stamp_; }
  void            enable_output         (AudioProcessor &aproc, bool onoff) override;
  void            start_thread          (const VoidF &owner_wakeup) override;
  void            stop_thread           () override;
  void            wakeup_thread_mt      () override;
  bool            ipc_pending           () override;
  void            ipc_dispatch          () override;
  AudioProcessorP get_event_source      () override;
  void            schedule_queue_update () override;
  void            schedule_add          (AudioProcessor &aproc, uint level) override;
  void            schedule_render       (uint64 frames);
  void            schedule_clear        ();
  bool            pcm_check_write       (bool write_buffer, int64 *timeout_usecs_p = nullptr);
  bool            driver_dispatcher     (const LoopState &state);
  bool            process_jobs          (AtomicIntrusiveStack<Job> &joblist);
  void            update_drivers        (bool fullio);
  void            run                   (const VoidF &owner_wakeup, StartQueue *sq);
  void            swap_midi_drivers_sync (const MidiDriverS &midi_drivers);
};

static inline std::atomic<AudioEngineThread::Job*>&
atomic_next_ptrref (AudioEngineThread::Job *j)
{
  return j->next;
}

AudioEngineThread::~AudioEngineThread ()
{
  fatal_error ("AudioEngine references must persist");
}

AudioEngineThread::AudioEngineThread (uint sample_rate, SpeakerArrangement speakerarrangement) :
  AudioEngine (sample_rate, speakerarrangement),
  synchronized_jobs (*this, 2)
{
  oprocs_.reserve (16);
}

template<int ADDING> static void
interleaved_stereo (const size_t frames, float *buffer, AudioProcessor &proc, OBusId obus)
{
  if (proc.n_ochannels (obus) >= 2)
    {
      const float *src0 = proc.ofloats (obus, 0);
      const float *src1 = proc.ofloats (obus, 1);
      float *d = buffer, *const b = d + 2 * frames;
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
      float *d = buffer, *const b = d + 2 * frames;
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
          interleaved_stereo<0> (AUDIO_BLOCK_MAX_RENDER_SIZE, buffer_data_, *oprocs_[i], MAIN_OBUS);
        else
          interleaved_stereo<1> (AUDIO_BLOCK_MAX_RENDER_SIZE, buffer_data_, *oprocs_[i], MAIN_OBUS);
      }
  if (n == 0)
    floatfill (buffer_data_, 0.0, buffer_size_);
  render_stamp_ = target_stamp;
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
AudioEngineThread::update_drivers (bool fullio)
{
  Error er = {};
  // PCM fallback
  PcmDriverConfig pconfig { .n_channels = fixed_n_channels, .mix_freq = fixed_sample_rate,
                            .latency_ms = 8, .block_length = AUDIO_BLOCK_MAX_RENDER_SIZE };
  const String null_driver = "null";
  if (!null_pcm_driver_)
    {
      null_pcm_driver_ = PcmDriver::open (null_driver, Driver::WRITEONLY, Driver::WRITEONLY, pconfig, &er);
      if (!null_pcm_driver_ || er != 0)
        fatal_error ("failed to open internal PCM driver ('%s'): %s", null_driver, ase_error_blurb (er));
    }
  if (!pcm_driver_)
    pcm_driver_ = null_pcm_driver_;
  // MIDI Processor
  if (!midi_proc_)
    swap_midi_drivers_sync ({});
  if (!fullio)
    return;
  // PCM Output
  if (pcm_driver_ == null_pcm_driver_)
    {
      const String pcm_driver_name = ServerImpl::instancep()->preferences().pcm_driver;
      er = {};
      if (pcm_driver_name != null_driver)
        pcm_driver_ = PcmDriver::open (pcm_driver_name, Driver::WRITEONLY, Driver::WRITEONLY, pconfig, &er);
      if (!pcm_driver_)
        {
          printerr ("failed to open PCM driver ('%s'): %s\n", pcm_driver_name, ase_error_blurb (er));
          pcm_driver_ = null_pcm_driver_;
        }
    }
  // MIDI Driver List
  MidiDriverS old_drivers = midi_drivers_, new_drivers;
  const auto midi_driver_names = {
    ServerImpl::instancep()->preferences().midi_driver_1,
    ServerImpl::instancep()->preferences().midi_driver_2,
    ServerImpl::instancep()->preferences().midi_driver_3,
    ServerImpl::instancep()->preferences().midi_driver_4,
  };
  for (const auto &devid : midi_driver_names)
    {
      if (devid == null_driver)
        continue;
      if (Aux::contains (new_drivers, [&] (auto &d) {
        return d->devid() == devid; }))
        {
          er = Error::DEVICE_BUSY;
          printerr ("failed to open MIDI driver ('%s'): %s\n", devid, ase_error_blurb (er));
          continue;
        }
      MidiDriverP d;
      for (MidiDriverP o : old_drivers)
        if (o->devid() == devid)
          d = o;
      if (d)
        {
          Aux::erase_first (old_drivers, [&] (auto &o) { return o == d; });
          new_drivers.push_back (d);    // keep opened driver
          continue;
        }
      er = {};
      d = MidiDriver::open (devid, Driver::READONLY, &er);
      if (d)
        new_drivers.push_back (d);      // add new driver
      else
        printerr ("failed to open MIDI driver ('%s'): %s\n", devid, ase_error_blurb (er));
    }
  midi_drivers_ = new_drivers;
  swap_midi_drivers_sync (midi_drivers_);
  while (!old_drivers.empty())
    {
      MidiDriverP old = old_drivers.back();
      old_drivers.pop_back();
      old->close();                     // close old driver *after* sync
    }
}

void
AudioEngineThread::run (const VoidF &owner_wakeup, StartQueue *sq)
{
  assert_return (pcm_driver_);
  // FIXME: assert owner_wakeup and free trash
  this_thread_set_name ("AudioEngine-0"); // max 16 chars
  thread_id_ = std::this_thread::get_id();
  owner_wakeup_ = owner_wakeup;
  event_loop_->exec_dispatcher (std::bind (&AudioEngineThread::driver_dispatcher, this, std::placeholders::_1));
  sq->push ('R'); // StartQueue becomes invalid after this call
  sq = nullptr;
  event_loop_->run();
  owner_wakeup_ = nullptr;
}

bool
AudioEngineThread::process_jobs (AtomicIntrusiveStack<Job> &joblist)
{
  Job *const jobs = joblist.pop_reversed(), *last = nullptr;
  for (Job *job = jobs; job; last = job, job = job->next)
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
  pcm_driver_->pcm_write (buffer_size_, buffer_data_);
  write_stamp_ += buffer_size_ / fixed_n_channels;
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
    case LoopState::CHECK:
      if (!const_jobs_.empty() || !async_jobs_.empty())
        return true; // jobs pending
      if (render_stamp_ <= write_stamp_)
        return true; // must render
      // FIXME: add pcm driver pollfd with 1-block threshold
      return pcm_check_write (false, timeout_usecs);
    case LoopState::DISPATCH:
      pcm_check_write (true);
      process_jobs (const_jobs_);
      if (render_stamp_ <= write_stamp_)
        {
          process_jobs (async_jobs_);
          if (schedule_invalid_)
            {
              schedule_clear();
              for (AudioProcessorP proc :  oprocs_)
                proc->schedule_processor();
              schedule_invalid_ = false;
            }
          schedule_render (AUDIO_BLOCK_MAX_RENDER_SIZE);
          pcm_check_write (true); // minimize drop outs
        }
      if (ipc_pending())
        owner_wakeup_(); // owner needs to ipc_dispatch()
      return true; // keep alive
    default: ;
    }
  return false;
}

bool
AudioEngineThread::ipc_pending ()
{
  const bool have_jobs = !trash_jobs_.empty();
  return have_jobs || AudioProcessor::enotify_pending();
}

void
AudioEngineThread::ipc_dispatch ()
{
  if (AudioProcessor::enotify_pending())
    AudioProcessor::enotify_dispatch();
  Job *job = trash_jobs_.pop_all();
  while (job)
    {
      Job *old = job;
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
AudioEngineThread::start_thread (const VoidF &owner_wakeup)
{
  schedule_.reserve (8192);
  assert_return (thread_ == nullptr);
  update_drivers (false);
  schedule_queue_update();
  StartQueue start_queue;
  thread_ = new std::thread (&AudioEngineThread::run, this, owner_wakeup, &start_queue);
  const char reply = start_queue.pop(); // synchronize with thread start
  assert_return (reply == 'R');
  Emittable::Connection onprefs_;
  onprefs_ = ASE_SERVER.on_event ("change:prefs", [this] (auto...) {
    update_drivers (true);
  });
  update_drivers (true);
}

void
AudioEngineThread::stop_thread ()
{
  AudioEngineThread &engine = *dynamic_cast<AudioEngineThread*> (this);
  assert_return (engine.thread_ != nullptr);
  engine.event_loop_->quit (0);
  engine.thread_->join();
  thread_id_ = {};
  auto oldthread = engine.thread_;
  engine.thread_ = nullptr;
  delete oldthread;
}

// == SpeakerArrangement ==
// Count the number of channels described by the SpeakerArrangement.
uint8
speaker_arrangement_count_channels (SpeakerArrangement spa)
{
  const uint64_t bits = uint64_t (speaker_arrangement_channels (spa));
  if_constexpr (sizeof (bits) == sizeof (long))
    return __builtin_popcountl (bits);
  return __builtin_popcountll (bits);
}

// Check if the SpeakerArrangement describes auxillary channels.
bool
speaker_arrangement_is_aux (SpeakerArrangement spa)
{
  return uint64_t (spa) & uint64_t (SpeakerArrangement::AUX);
}

// Retrieve the bitmask describing the SpeakerArrangement channels.
SpeakerArrangement
speaker_arrangement_channels (SpeakerArrangement spa)
{
  const uint64_t bits = uint64_t (spa) & uint64_t (speaker_arrangement_channels_mask);
  return SpeakerArrangement (bits);
}

const char*
speaker_arrangement_bit_name (SpeakerArrangement spa)
{
  switch (spa)
    { // https://wikipedia.org/wiki/Surround_sound
    case SpeakerArrangement::NONE:              	return "-";
      // case SpeakerArrangement::MONO:                 return "Mono";
    case SpeakerArrangement::FRONT_LEFT:        	return "FL";
    case SpeakerArrangement::FRONT_RIGHT:       	return "FR";
    case SpeakerArrangement::FRONT_CENTER:      	return "FC";
    case SpeakerArrangement::LOW_FREQUENCY:     	return "LFE";
    case SpeakerArrangement::BACK_LEFT:         	return "BL";
    case SpeakerArrangement::BACK_RIGHT:                return "BR";
    case SpeakerArrangement::AUX:                       return "AUX";
    case SpeakerArrangement::STEREO:                    return "Stereo";
    case SpeakerArrangement::STEREO_21:                 return "Stereo-2.1";
    case SpeakerArrangement::STEREO_30:	                return "Stereo-3.0";
    case SpeakerArrangement::STEREO_31:	                return "Stereo-3.1";
    case SpeakerArrangement::SURROUND_50:	        return "Surround-5.0";
    case SpeakerArrangement::SURROUND_51:	        return "Surround-5.1";
#if 0 // TODO: dynamic multichannel support
    case SpeakerArrangement::FRONT_LEFT_OF_CENTER:      return "FLC";
    case SpeakerArrangement::FRONT_RIGHT_OF_CENTER:     return "FRC";
    case SpeakerArrangement::BACK_CENTER:               return "BC";
    case SpeakerArrangement::SIDE_LEFT:	                return "SL";
    case SpeakerArrangement::SIDE_RIGHT:	        return "SR";
    case SpeakerArrangement::TOP_CENTER:	        return "TC";
    case SpeakerArrangement::TOP_FRONT_LEFT:	        return "TFL";
    case SpeakerArrangement::TOP_FRONT_CENTER:	        return "TFC";
    case SpeakerArrangement::TOP_FRONT_RIGHT:	        return "TFR";
    case SpeakerArrangement::TOP_BACK_LEFT:	        return "TBL";
    case SpeakerArrangement::TOP_BACK_CENTER:	        return "TBC";
    case SpeakerArrangement::TOP_BACK_RIGHT:	        return "TBR";
    case SpeakerArrangement::SIDE_SURROUND_50:	        return "Side-Surround-5.0";
    case SpeakerArrangement::SIDE_SURROUND_51:	        return "Side-Surround-5.1";
#endif
    }
  return nullptr;
}

std::string
speaker_arrangement_desc (SpeakerArrangement spa)
{
  const bool isaux = speaker_arrangement_is_aux (spa);
  const SpeakerArrangement chan = speaker_arrangement_channels (spa);
  const char *chname = SpeakerArrangement::MONO == chan ? "Mono" : speaker_arrangement_bit_name (chan);
  std::string s (chname ? chname : "<INVALID>");
  if (isaux)
    s = std::string (speaker_arrangement_bit_name (SpeakerArrangement::AUX)) + "(" + s + ")";
  return s;
}

// == AudioEngine ==
AudioEngine::AudioEngine (uint sample_rate, SpeakerArrangement speakerarrangement) :
  nyquist_ (0.5 * sample_rate), inyquist_ (1.0 / nyquist_), sample_rate_ (sample_rate),
  speaker_arrangement_ (speakerarrangement),
  const_jobs (*this, 1), async_jobs (*this, 0)
{
  assert_return (sample_rate == 48000);
}

void
AudioEngine::add_job_mt (const std::function<void()> &jobfunc, int flags)
{
  AudioEngineThread &engine = *dynamic_cast<AudioEngineThread*> (this);
  if (!engine.thread_)
    {
      jobfunc();
      return;
    }
  if (flags == 0) // async_jobs flag
    {
      AudioEngineThread::Job *job = new AudioEngineThread::Job { nullptr, jobfunc };
      const bool was_empty = engine.async_jobs_.push (job);
      if (was_empty)
        wakeup_thread_mt();
      return;
    }
  ScopedSemaphore sem;
  std::function<void()> wrapper = [&sem, &jobfunc] () {
    jobfunc();
    sem.post();
  };
  AudioEngineThread::Job *job = new AudioEngineThread::Job { nullptr, wrapper };
  bool need_wakeup;
  if (flags == 1)
    need_wakeup = engine.const_jobs_.push (job);
  else if (flags == 2)
    need_wakeup = engine.async_jobs_.push (job);
  else
    assert_return_unreached();
  if (need_wakeup)
    wakeup_thread_mt();
  sem.wait();
}

SpeakerArrangement
AudioEngine::speaker_arrangement () const
{
  return speaker_arrangement_;
}

AudioEngine&
make_audio_engine (uint sample_rate, SpeakerArrangement speakerarrangement)
{
  return *new AudioEngineThread (sample_rate, speakerarrangement);
}

AudioProcessorP
make_audio_processor (AudioEngine &engine, const String &uuiduri)
{
  return AudioProcessor::registry_create (engine, uuiduri);
}

// == MidiInput ==
// Processor providing MIDI device events
class EngineMidiInput : public AudioProcessor {
  void
  query_info (AudioProcessorInfo &info) const override
  {
    info.uri          = "Anklang.Devices.EngineMidiInput";
    info.version      = "1";
    info.label        = "MIDI Input";
    info.category     = "Routing";
    info.creator_name = "Tim Janik";
    info.website_url  = "https://anklang.testbit.eu";
  }
  void
  initialize (SpeakerArrangement busses) override
  {
    prepare_event_output();
  }
  void
  reset (uint64 target_stamp) override
  {
    MidiEventStream &estream = get_event_output();
    estream.clear();
    estream.reserve (256);
  }
  void
  render (uint n_frames) override
  {
    MidiEventStream &estream = get_event_output();
    estream.clear();
    for (size_t i = 0; i < midi_drivers_.size(); i++)
      midi_drivers_[i]->fetch_events (estream, sample_rate());
  }
public:
  MidiDriverS midi_drivers_;
};
static auto engine_midi_input = register_audio_processor<EngineMidiInput>();

template<class T> struct Mutable {
  mutable T value;
  Mutable (const T &v) : value (v) {}
  operator T& () { return value; }
};

void
AudioEngineThread::swap_midi_drivers_sync (const MidiDriverS &midi_drivers)
{
  if (!midi_proc_)
    {
      AudioProcessorP aprocp = make_audio_processor (*this, "Anklang.Devices.EngineMidiInput");
      assert_return (aprocp);
      midi_proc_ = std::dynamic_pointer_cast<EngineMidiInput> (aprocp);
      assert_return (midi_proc_);
      EngineMidiInputP midi_proc = midi_proc_;
      async_jobs += [midi_proc] () {
        midi_proc->enable_engine_output (true);
      };
    }
  EngineMidiInputP midi_proc = midi_proc_;
  Mutable<MidiDriverS> new_drivers { midi_drivers };
  synchronized_jobs += [midi_proc, new_drivers] () {
    midi_proc->midi_drivers_.swap (new_drivers.value);
    // use swap() to defer dtor to user thread
  };
}

AudioProcessorP
AudioEngineThread::get_event_source ()
{
  return midi_proc_;
}

} // Ase
