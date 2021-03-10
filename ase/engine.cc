// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "engine.hh"
#include "utils.hh"
#include "loop.hh"
#include "driver.hh"
#include "datautils.hh"
#include "internal.hh"

namespace Ase {

using StartQueue = AsyncBlockingQueue<char>;
constexpr uint default_sample_rate = 48000;

// == AudioEngineThread ==
class AudioEngineThread : public AudioEngine {
  ~AudioEngineThread ();
  PcmDriverP pcm_driver_, null_pcm_driver_;
  size_t buffer_size_ = AUDIO_BLOCK_FLOAT_ZEROS_SIZE;
  float buffer_data_[AUDIO_BLOCK_FLOAT_ZEROS_SIZE] = { 0, };
public:
  std::thread *thread_ = nullptr;
  MainLoopP    event_loop_ = MainLoop::create();
  explicit AudioEngineThread (uint sample_rate);
  void     run               (const VoidF &owner_wakeup, StartQueue *sq);
  bool     driver_dispatcher (const LoopState &state);
  void     ensure_driver     ();
};

AudioEngineThread::~AudioEngineThread ()
{
  fatal_error ("AudioEngine references must persist");
}

AudioEngineThread::AudioEngineThread (uint sample_rate) :
  AudioEngine (sample_rate)
{}

void
AudioEngineThread::ensure_driver()
{
  return_unless (!null_pcm_driver_);
  PcmDriverConfig pconfig { .n_channels = 2, .mix_freq = default_sample_rate,
                            .latency_ms = 0, .block_length = AUDIO_BLOCK_FLOAT_ZEROS_SIZE };
  const String null_driver = "null";
  Ase::Error er = {};
  null_pcm_driver_ = PcmDriver::open (null_driver, Driver::WRITEONLY, Driver::WRITEONLY, pconfig, &er);
  if (!null_pcm_driver_ || er != 0)
    fatal_error ("failed to open internal PCM driver ('%s'): %s", null_driver, ase_error_blurb (er));
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
    case LoopState::DISPATCH:
      pcm_driver_->pcm_write (buffer_size_, buffer_data_);
      // render
      floatfill (buffer_data_, 0.0, buffer_size_);
      return true; // keep alive
    default: ;
    }
  return false;
}

// == AudioEngine ==
AudioEngine&
make_audio_engine (uint sample_rate)
{
  return *new AudioEngineThread (sample_rate);
}

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

} // Ase
