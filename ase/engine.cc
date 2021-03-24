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
  std::thread    *thread_ = nullptr;
  MainLoopP       event_loop_ = MainLoop::create();
  AudioProcessorS oprocs_;
  explicit AudioEngineThread (uint sample_rate, SpeakerArrangement speakerarrangement);
  void     run               (const VoidF &owner_wakeup, StartQueue *sq);
  bool     driver_dispatcher (const LoopState &state);
  void     ensure_driver     ();
  void     add_output        (AudioProcessorP aproc) override;
  bool     remove_output     (AudioProcessorP aproc) override;
  void     enqueue           (AudioProcessor &aproc) override;
};

AudioEngineThread::~AudioEngineThread ()
{
  fatal_error ("AudioEngine references must persist");
}

AudioEngineThread::AudioEngineThread (uint sample_rate, SpeakerArrangement speakerarrangement) :
  AudioEngine (sample_rate, speakerarrangement)
{
  oprocs_.reserve (64);
}

void
AudioEngineThread::enqueue (AudioProcessor &aproc)
{
  // TODO: impl missing
  reschedule();
}

void
AudioEngineThread::add_output (AudioProcessorP aproc)
{
  oprocs_.push_back (aproc);
  reschedule();
}

bool
AudioEngineThread::remove_output (AudioProcessorP aproc)
{
  const bool count = Aux::erase_first (oprocs_, [aproc] (AudioProcessorP c) { return c == aproc; });
  if (count)
    reschedule();
  return count;
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
  reschedule();
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
      constexpr auto MAIN_OBUS = OBusId (1);
      // render
      frame_counter_ += AUDIO_BLOCK_MAX_RENDER_SIZE;
      if (oprocs_.size() == 0)
        floatfill (buffer_data_, 0.0, buffer_size_);
      else
        { // render_block
          // TODO: assert_return (!(eflags_ & RESCHEDULE));
          // TODO: for (auto procp : schedule_) procp->render_block();
          for (auto op : oprocs_)
            render_block (op);
          interleaved_stereo<0> (AUDIO_BLOCK_MAX_RENDER_SIZE, buffer_data_, *oprocs_[0], MAIN_OBUS);
          for (size_t i = 1; i < oprocs_.size(); i++)
            interleaved_stereo<1> (AUDIO_BLOCK_MAX_RENDER_SIZE, buffer_data_, *oprocs_[i], MAIN_OBUS);
        }
      return true; } // keep alive
    default: ;
    }
  return false;
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
  speaker_arrangement_ (speakerarrangement), frame_counter_ (1024 * 1024 * 1024),
  const_jobs (*this, 0), async_jobs (*this, 1)
{
  assert_return (sample_rate == 48000);
}

void
AudioEngine::start_thread (const VoidF &owner_wakeup)
{
  AudioEngineThread &engine = *dynamic_cast<AudioEngineThread*> (this);
  engine.ensure_driver();
  assert_return (engine.thread_ == nullptr);
  reschedule();
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
AudioEngine::add_job (const std::function<void()> &job, int flags)
{
  assert_return (!"CALLED");
  // TODO: implement, also need trash queue
}

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

} // Ase
