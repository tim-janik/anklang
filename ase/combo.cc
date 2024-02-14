// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "combo.hh"
#include "randomhash.hh"
#include "server.hh"
#include "internal.hh"

#define PDEBUG(...)     Ase::debug ("combo", __VA_ARGS__)

namespace Ase {

static constexpr OBusId OUT1 = OBusId (1);

// == Inlet ==
class AudioChain::Inlet : public AudioProcessor {
  AudioChain &audio_chain_;
public:
  Inlet (const ProcessorSetup &psetup, AudioChain *audiochain) :
    AudioProcessor (psetup),
    audio_chain_ (*audiochain)
  {
    assert_return (nullptr != audiochain);
  }
  void reset      (uint64 target_stamp) override            {}
  void
  initialize (SpeakerArrangement busses) override
  {
    remove_all_buses();
    auto output = add_output_bus ("Output", audio_chain_.ispeakers_);
    (void) output;
  }
  void
  render (uint n_frames) override
  {
    const IBusId i1 = IBusId (1);
    const OBusId o1 = OBusId (1);
    const uint ni = audio_chain_.n_ichannels (i1);
    const uint no = this->n_ochannels (o1);
    assert_return (ni == no);
    for (size_t i = 0; i < ni; i++)
      redirect_oblock (o1, i, audio_chain_.ifloats (i1, i));
  }
};

// == AudioCombo ==
AudioCombo::AudioCombo (const ProcessorSetup &psetup) :
  AudioProcessor (psetup)
{}

AudioCombo::~AudioCombo ()
{
  eproc_ = nullptr;
}

/// Add a new AudioProcessor `proc` at position `pos` to the AudioCombo.
/// The processor `proc` must not be previously contained by another AudioCombo.
void
AudioCombo::insert (AudioProcessorP proc, ssize_t pos)
{
  assert_return (proc != nullptr);
  if (pos == -1)
    pos = processors_.size(); // insert at the end of the chain
  const size_t index = CLAMP (pos, 0, processors_.size());
  processors_.insert (processors_.begin() + index, proc);
  // fixup following connections
  reconnect (index, true);
  reschedule();
  enotify_enqueue_mt (INSERTION);
}

/// Remove a previously added AudioProcessor `proc` from the AudioCombo.
bool
AudioCombo::remove (AudioProcessor &proc)
{
  std::vector<AudioProcessor*> unconnected;
  AudioProcessorP processorp;
  size_t pos; // find proc
  for (pos = 0; pos < processors_.size(); pos++)
    if (processors_[pos].get() == &proc)
      {
        processorp = processors_[pos]; // and remove...
        processors_.erase (processors_.begin() + pos);
        break;
      }
  if (!processorp)
    return false;
  // clear stale connections
  pm_disconnect_ibuses (*processorp);
  pm_disconnect_obuses (*processorp);
  // fixup following connections
  reconnect (pos, false);
  enotify_enqueue_mt (REMOVAL);
  reschedule();
  return true;
}

/// Return the AudioProcessor at position `nth` in the AudioCombo.
AudioProcessorP
AudioCombo::at (uint nth)
{
  return_unless (nth < size(), {});
  return processors_[nth];
}

/// Return the index of AudioProcessor `proc` in the AudioCombo.
ssize_t
AudioCombo::find_pos (AudioProcessor &proc)
{
  for (size_t i = 0; i < processors_.size(); i++)
    if (processors_[i].get() == &proc)
      return i;
  return ~size_t (0);
}

/// Return the number of AudioProcessor instances in the AudioCombo.
size_t
AudioCombo::size ()
{
  return processors_.size();
}

/// Retrieve list of AudioProcessorS contained in `this` AudioCombo.
AudioProcessorS
AudioCombo::list_processors () const
{
  return processors_; // copy
}

/// Assign event source for future auto-connections of chld processors.
void
AudioCombo::set_event_source (AudioProcessorP eproc)
{
  if (eproc)
    assert_return (eproc->has_event_output());
  eproc_ = eproc;
}

// == AudioChain ==
AudioChain::AudioChain (const ProcessorSetup &psetup, SpeakerArrangement iobuses) :
  AudioCombo (psetup),
  ispeakers_ (iobuses), ospeakers_ (iobuses)
{
  assert_return (speaker_arrangement_count_channels (iobuses) > 0);
  inlet_ = AudioProcessor::create_processor<AudioChain::Inlet> (engine_, this);
  assert_return (inlet_ != nullptr);
  probe_block_ = SERVER->telemem_allocate (sizeof (ProbeArray));
  probes_ = new (probe_block_.block_start) ProbeArray{};
}

AudioChain::~AudioChain()
{
  pm_remove_all_buses (*inlet_);
  inlet_ = nullptr;
  probes_->~ProbeArray();
  probes_ = nullptr;
  SERVER->telemem_release (probe_block_);
}

void
AudioChain::static_info (AudioProcessorInfo &info)
{} // avoid public listing

void
AudioChain::initialize (SpeakerArrangement busses)
{
  auto ibus = add_input_bus ("Input", ispeakers_);
  auto obus = add_output_bus ("Output", ospeakers_);
  (void) ibus;
  assert_return (OUT1 == obus);
}

void
AudioChain::reset (uint64 target_stamp)
{}

uint
AudioChain::schedule_children()
{
  last_output_ = nullptr;
  uint level = schedule_processor (*inlet_);
  for (auto procp : processors_)
    {
      const uint l = schedule_processor (*procp);
      level = std::max (level, l);
      if (procp->n_obuses())
        last_output_ = procp.get();
    }
  // last_output_ is only valid during render()
  return level;
}

void
AudioChain::render (uint n_frames)
{
  // make the last processor output the chain output
  const size_t nlastchannels = last_output_ ? last_output_->n_ochannels (OUT1) : 0;
  const size_t n_och = n_ochannels (OUT1);
  std::array<Probe,2> *probes = n_och <= 2 ? probes_ : nullptr;
  for (size_t c = 0; c < n_och; c++)
    {
      // an enqueue_children() call is guranteed *before* render(), so last_output_ is valid
      if (UNLIKELY (!last_output_))
        {
          redirect_oblock (OUT1, c, nullptr);
          if (probes)
            (*probes)[c].dbspl = -192;
        }
      else
        {
          const float *cblock = last_output_->ofloats (OUT1, std::min (c, nlastchannels - 1));
          redirect_oblock (OUT1, c, cblock);
          if (probes)
            {
              // SPL = 20 * log10 (root_mean_square (p) / p0) dB        ; https://en.wikipedia.org/wiki/Sound_pressure#Sound_pressure_level
              // const float sqrsig = square_sum (n_frames, cblock) / n_frames; // * 1.0 / p0^2
              const float sqrsig = square_max (n_frames, cblock);
              const float log2div = 3.01029995663981; // 20 / log2 (10) / 2.0
              const float db_spl = ISLIKELY (sqrsig > 0.0) ? log2div * fast_log2 (sqrsig) : -192;
              (*probes)[c].dbspl = db_spl;
            }
        }
    }
  // FIXME: assign obus if no children are present
}

/// Reconnect AudioChain child processors at start and after.
void
AudioChain::reconnect (size_t index, bool insertion)
{
  // clear stale inputs
  for (size_t i = index; i < processors_.size(); i++)
    pm_disconnect_ibuses (*processors_[i]);
  // reconnect pairwise
  for (size_t i = index; i < processors_.size(); i++)
    chain_up (*(i ? processors_[i - 1] : inlet_), *processors_[i]);
}

/// Connect the main audio input of `next` to audio output of `prev`.
uint
AudioChain::chain_up (AudioProcessor &prev, AudioProcessor &next)
{
  assert_return (this != &prev, 0);
  assert_return (this != &next, 0);
  const uint ni = next.n_ibuses();
  const uint no = prev.n_obuses();
  // assign event source
  if (eproc_)
    {
      if (prev.has_event_input())
        pm_connect_event_input (*eproc_, prev);
      if (next.has_event_input())
        pm_connect_event_input (*eproc_, next);
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

AudioChain::ProbeArray*
AudioChain::run_probes (bool enable)
{
  if (enable && !probes_enabled_)
    for (size_t i = 0; i < probes_->size(); i++)
      (*probes_)[i] = Probe{};
  probes_enabled_ = enable;
  return probes_enabled_ ? probes_ : nullptr;
}

static const auto audio_chain_id = register_audio_processor<AudioChain>();

} // Ase
