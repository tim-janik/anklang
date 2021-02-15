// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "engine.hh"
#include "utils.hh"
#include "loop.hh"
#include "internal.hh"

namespace Ase {

using StartQueue = AsyncBlockingQueue<char>;

// == AudioEngineThread ==
class AudioEngineThread : public AudioEngine {
  ~AudioEngineThread ();
public:
  std::thread *thread_ = nullptr;
  MainLoopP    event_loop_ = MainLoop::create();
  explicit AudioEngineThread (uint sample_rate);
  void run (const VoidF &owner_wakeup, StartQueue *sq);
};

AudioEngineThread::~AudioEngineThread ()
{
  fatal_error ("AudioEngine references must persist");
}

AudioEngineThread::AudioEngineThread (uint sample_rate) :
  AudioEngine (sample_rate)
{}

void
AudioEngineThread::run (const VoidF &owner_wakeup, StartQueue *sq)
{
  this_thread_set_name ("AudioEngine-0"); // max 16 chars
  thread_id_ = std::this_thread::get_id();
  printerr ("AudioEngineThread.run: running\n");
  sq->push ('R'); // StartQueue becomes invalid after this call
  sq = nullptr;
  event_loop_->run();
  printerr ("AudioEngineThread.run: exiting\n");
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
