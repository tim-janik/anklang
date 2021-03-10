// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "engine.hh"
#include "processor.hh"
#include "utils.hh"
#include "loop.hh"
#include "driver.hh"
#include "datautils.hh"
#include "internal.hh"

namespace Ase {

using StartQueue = AsyncBlockingQueue<char>;
constexpr uint fixed_sample_rate = 48000;
constexpr uint fixed_n_channels = 2;

// == AudioEngineThread ==
class AudioEngineThread : public AudioEngine {
  ~AudioEngineThread ();
  PcmDriverP pcm_driver_, null_pcm_driver_;
  constexpr static size_t buffer_size_ = AUDIO_BLOCK_MAX_RENDER_SIZE * fixed_n_channels;
  float buffer_data_[buffer_size_] = { 0, };
public:
  std::thread *thread_ = nullptr;
  MainLoopP    event_loop_ = MainLoop::create();
  AudioProcessorP aproc_;
  explicit AudioEngineThread (uint sample_rate);
  void     run               (const VoidF &owner_wakeup, StartQueue *sq);
  bool     driver_dispatcher (const LoopState &state);
  void     ensure_driver     ();
  void     assign_root       (AudioProcessorP aproc) override;
  void     enqueue           (AudioProcessor &aproc) override;
};

AudioEngineThread::~AudioEngineThread ()
{
  fatal_error ("AudioEngine references must persist");
}

AudioEngineThread::AudioEngineThread (uint sample_rate) :
  AudioEngine (sample_rate)
{}

void
AudioEngineThread::enqueue (AudioProcessor &aproc)
{
  // TODO: impl missing
}

void
AudioEngineThread::assign_root (AudioProcessorP aproc)
{
  AudioProcessorP oldp;
  std::swap (oldp, aproc_);
  aproc_ = aproc;
  // TODO: move oldp into main thread
}

void
AudioEngineThread::ensure_driver()
{
  return_unless (!null_pcm_driver_);
  PcmDriverConfig pconfig { .n_channels = fixed_n_channels, .mix_freq = fixed_sample_rate,
                            .latency_ms = 8, .block_length = AUDIO_BLOCK_MAX_RENDER_SIZE };
  const String null_driver = "null";
  Ase::Error er = {};
  null_pcm_driver_ = PcmDriver::open (null_driver, Driver::WRITEONLY, Driver::WRITEONLY, pconfig, &er);
  if (!null_pcm_driver_ || er != 0)
    fatal_error ("failed to open internal PCM driver ('%s'): %s", null_driver, ase_error_blurb (er));
  if (!pcm_driver_)
    pcm_driver_ = PcmDriver::open ("auto", Driver::WRITEONLY, Driver::WRITEONLY, pconfig, &er);
  if (!pcm_driver_)
    pcm_driver_ = null_pcm_driver_;
}

void
AudioEngineThread::run (const VoidF &owner_wakeup, StartQueue *sq)
{
  assert_return (pcm_driver_);
  this_thread_set_name ("AudioEngine-0"); // max 16 chars
  thread_id_ = std::this_thread::get_id();
  printerr ("AudioEngineThread.run: running\n");
  event_loop_->exec_dispatcher (std::bind (&AudioEngineThread::driver_dispatcher, this, std::placeholders::_1));
  sq->push ('R'); // StartQueue becomes invalid after this call
  sq = nullptr;
  event_loop_->run();
  printerr ("AudioEngineThread.run: exiting\n");
}

bool
AudioEngineThread::driver_dispatcher (const LoopState &state)
{
  switch (state.phase)
    {
    case LoopState::PREPARE:
      {
        int64 *timeout_usecs = const_cast<int64*> (&state.timeout_usecs);
        return pcm_driver_->pcm_check_io (timeout_usecs);
      }
    case LoopState::CHECK:
      {
        int64 timeout_usecs = INT64_MAX;
        return pcm_driver_->pcm_check_io (&timeout_usecs) || timeout_usecs == 0;
      }
    case LoopState::DISPATCH: {
      pcm_driver_->pcm_write (buffer_size_, buffer_data_);
      // render
      frame_counter_ += AUDIO_BLOCK_MAX_RENDER_SIZE;
      if (aproc_ && aproc_->n_obuses())
        { // render_block
          // TODO: assert_return (!(eflags_ & RESCHEDULE));
          // TODO: for (auto procp : schedule_) procp->render_block();
          render_block (aproc_);
          constexpr auto MAIN_OBUS = OBusId (1);
          if (aproc_->n_ochannels (MAIN_OBUS) >= 2)
            {
              const float *src0 = aproc_->ofloats (MAIN_OBUS, 0);
              const float *src1 = aproc_->ofloats (MAIN_OBUS, 1);
              float *d = buffer_data_, *b = d + 2 * AUDIO_BLOCK_MAX_RENDER_SIZE;
              do {
                *d++ = *src0++;
                *d++ = *src1++;
              } while (d < b);
            }
          else if (aproc_->n_ochannels (MAIN_OBUS) >= 1)
            {
              const float *src = aproc_->ofloats (MAIN_OBUS, 0);
              float *d = buffer_data_, *b = d + 2 * AUDIO_BLOCK_MAX_RENDER_SIZE;
              do {
                *d++ = *src;
                *d++ = *src++;
              } while (d < b);
            }
        }
      else
        floatfill (buffer_data_, 0.0, buffer_size_);
      return true; } // keep alive
    default: ;
    }
  return false;
}

// == AudioEngine ==
AudioEngine::AudioEngine (uint sample_rate) :
  nyquist_ (0.5 * sample_rate), inyquist_ (1.0 / nyquist_), sample_rate_ (sample_rate),
  frame_counter_ (1024 * 1024 * 1024)
{
  assert_return (sample_rate == 48000);
}

void
AudioEngine::start_thread (const VoidF &owner_wakeup)
{
  AudioEngineThread &engine = *dynamic_cast<AudioEngineThread*> (this);
  engine.ensure_driver();
  assert_return (engine.thread_ == nullptr);
  StartQueue start_queue;
  engine.thread_ = new std::thread (&AudioEngineThread::run, &engine, owner_wakeup, &start_queue);
  const char reply = start_queue.pop(); // synchronize with thread start
  assert_return (reply == 'R');
}

void
AudioEngine::stop_thread ()
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

void
AudioEngine::operator+= (const std::function<void()> &job)
{}  // TODO: implement, also need trash queue

void
AudioEngine::reschedule ()
{}

bool
AudioEngine::ipc_pending ()
{
  return false;
}

void
AudioEngine::ipc_dispatch ()
{}

void
AudioEngine::ipc_wakeup_mt ()
{}

void
AudioEngine::render_block (AudioProcessorP ap)
{
  ap->render_block();
}

AudioEngine&
make_audio_engine (uint sample_rate)
{
  return *new AudioEngineThread (sample_rate);
}

AudioProcessorP
make_audio_processor (AudioEngine &engine, const String &uuiduri)
{
  return AudioProcessor::registry_create (engine, uuiduri);
}

} // Ase
