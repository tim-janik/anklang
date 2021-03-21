// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "combo.hh"
#include "randomhash.hh"
#include "internal.hh"

#define PDEBUG(...)     Ase::debug ("combo", __VA_ARGS__)

namespace Ase {

// == Inlet ==
class AudioCombo::Inlet : public AudioProcessor {
  AudioCombo &audio_combo_;
public:
  Inlet (const std::any &any) :
    audio_combo_ (*std::any_cast<AudioCombo*> (any))
  {
    assert_return (nullptr != std::any_cast<AudioCombo*> (any));
  }
  void query_info (AudioProcessorInfo &info) const override { info.label = "Anklang.Devices.AudioCombo.Inlet"; }
  void reset      () override                               {}
  void initialize () override                               {}
  void
  configure (uint n_ibusses, const SpeakerArrangement *ibusses, uint n_obusses, const SpeakerArrangement *obusses) override
  {
    remove_all_buses();
    auto output = add_output_bus ("Output", audio_combo_.ispeakers_);
    (void) output;
  }
  void
  render (uint n_frames) override
  {
    const IBusId i1 = IBusId (1);
    const OBusId o1 = OBusId (1);
    const uint ni = audio_combo_.n_ichannels (i1);
    const uint no = this->n_ochannels (o1);
    assert_return (ni == no);
    for (size_t i = 0; i < ni; i++)
      redirect_oblock (o1, i, audio_combo_.ifloats (i1, i));
  }
};

// == AudioCombo ==
AudioCombo::AudioCombo (SpeakerArrangement iobuses) :
  ispeakers_ (iobuses), ospeakers_ (iobuses)
{
  static const auto reg_id = register_audio_processor<AudioCombo::Inlet>();
  assert_return (speaker_arrangement_count_channels (iobuses) > 0);
  AudioProcessorP inlet = AudioProcessor::registry_create (engine_, reg_id, this);
  assert_return (inlet != nullptr);
  inlet_ = std::dynamic_pointer_cast<Inlet> (inlet);
  assert_return (inlet_ != nullptr);
}

AudioCombo::~AudioCombo ()
{
  pm_remove_all_buses (*inlet_);
  inlet_ = nullptr;
  eproc_ = nullptr;
}

/// Add a new AudioProcessor `proc` at position `pos` to the AudioCombo.
/// The processor `proc` must not be previously contained by another AudioCombo.
void
AudioCombo::insert (AudioProcessorP proc, size_t pos)
{
  assert_return (proc != nullptr);
  const AudioProcessorS &cprocessors = processors_mt_;
  const size_t index = CLAMP (pos, 0, cprocessors.size());
  {
    std::lock_guard<std::mutex> locker (mt_mutex_);
    processors_mt_.insert (processors_mt_.begin() + index, proc);
  }
  // fixup following connections
  reconnect (index);
  engine_.reschedule();
  enqueue_notify_mt (INSERTION);
}

/// Remove a previously added AudioProcessor `proc` from the AudioCombo.
bool
AudioCombo::remove (AudioProcessor &proc)
{
  const AudioProcessorS &cprocessors = processors_mt_;
  std::vector<AudioProcessor*> unconnected;
  AudioProcessorP processorp;
  size_t pos; // find proc
  for (pos = 0; pos < cprocessors.size(); pos++)
    if (cprocessors[pos].get() == &proc)
      {
        processorp = cprocessors[pos]; // and remove...
        std::lock_guard<std::mutex> locker (mt_mutex_);
        processors_mt_.erase (processors_mt_.begin() + pos);
        break;
      }
  if (!processorp)
    return false;
  // clear stale connections
  pm_disconnect_ibuses (*processorp);
  pm_disconnect_obuses (*processorp);
  // fixup following connections
  reconnect (pos);
  enqueue_notify_mt (REMOVAL);
  return true;
}

/// Return the AudioProcessor at position `nth` in the AudioCombo.
AudioProcessorP
AudioCombo::at (uint nth)
{
  return_unless (nth < size(), {});
  const AudioProcessorS &cprocessors = processors_mt_;
  return cprocessors[nth];
}

/// Return the index of AudioProcessor `proc` in the AudioCombo.
ssize_t
AudioCombo::find_pos (AudioProcessor &proc)
{
  const AudioProcessorS &cprocessors = processors_mt_;
  for (size_t i = 0; i < cprocessors.size(); i++)
    if (cprocessors[i].get() == &proc)
      return i;
  return  ~size_t (0);
}

/// Return the number of AudioProcessor instances in the AudioCombo.
size_t
AudioCombo::size ()
{
  const AudioProcessorS &cprocessors = processors_mt_;
  return cprocessors.size();
}

/// Retrieve list of AudioProcessorS contained in `this` AudioCombo.
AudioProcessorS
AudioCombo::list_processors_mt () const
{
  std::lock_guard<std::mutex> locker (const_cast<std::mutex&> (mt_mutex_));
  return processors_mt_; // copy with guard
}

/// Assign event source for future auto-connections of chld processors.
void
AudioCombo::set_event_source (AudioProcessorP eproc)
{
  if (eproc)
    assert_return (eproc->has_event_output());
  eproc_ = eproc;
}

/// Reconnect AudioCombo child processors at start and after.
void
AudioCombo::reconnect (size_t start)
{
  const AudioProcessorS &cprocessors = processors_mt_;
  // clear stale inputs
  for (size_t i = start; i < cprocessors.size(); i++)
    pm_disconnect_ibuses (*cprocessors[i]);
  // reconnect pairwise
  for (size_t i = start; i < cprocessors.size(); i++)
    chain_up (*(i ? cprocessors[i - 1] : inlet_), *cprocessors[i]);
}

/// Connect the main audio input of `next` to audio output of `prev`.
uint
AudioCombo::chain_up (AudioProcessor &prev, AudioProcessor &next)
{
  assert_return (this != &prev, 0);
  assert_return (this != &next, 0);
  const uint ni = next.n_ibuses();
  const uint no = prev.n_obuses();
  // assign event source
  if (eproc_)
    {
      if (prev.has_event_input())
        pm_connect_events (*eproc_, prev);
      if (next.has_event_input())
        pm_connect_events (*eproc_, next);
    }
  // check need for audio connections
  if (ni == 0 || no == 0)
    return 0; // nothing to do
  uint n_connected = 0;
  // try to connect prev main obus (1) with next main ibus (1)
  const OBusId obusid { 1 };
  const IBusId ibusid { 1 };
  SpeakerArrangement ospa = speaker_arrangement_channels (prev.bus_info (obusid).speakers);
  SpeakerArrangement ispa = speaker_arrangement_channels (next.bus_info (ibusid).speakers);
  if (0 == (uint64_t (ispa) & ~uint64_t (ospa)) || // exact match
      (ospa == SpeakerArrangement::MONO &&         // allow MONO -> STEREO connections
       ispa == SpeakerArrangement::STEREO))
    {
      n_connected += speaker_arrangement_count_channels (ispa);
      pm_connect (next, ibusid, prev, obusid);
    }
  return n_connected;
}


// == AudioChain ==
AudioChain::AudioChain (SpeakerArrangement iobuses) :
  AudioCombo (iobuses)
{}

AudioChain::~AudioChain()
{}

void
AudioChain::query_info (AudioProcessorInfo &info) const
{
  info.uri          = "Anklang.Devices.AudioChain";
  info.version      = "1";
  info.label        = "Ase.AudioChain";
  info.category     = "Routing";
  info.creator_name = "Anklang Authors";
  info.website_url  = "https://anklang.testbit.eu";
}

} // Ase
