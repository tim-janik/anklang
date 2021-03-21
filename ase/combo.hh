// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_COMBO_HH__
#define __ASE_COMBO_HH__

#include <ase/processor.hh>
#include <ase/engine.hh>

namespace Ase {

class AudioCombo : public AudioProcessor, ProcessorManager {
  ASE_CLASS_DECLS (Inlet);
  const SpeakerArrangement ispeakers_ = SpeakerArrangement (0);
  const SpeakerArrangement ospeakers_ = SpeakerArrangement (0);
  AudioProcessorS  processors_mt_;
  std::mutex       mt_mutex_; // FIXME: should *NOT* be needed all mods need to be engine jobs
  InletP           inlet_;
  AudioProcessorP  eproc_;
protected:
  uint            chain_up           (AudioProcessor &pfirst, AudioProcessor &psecond);
  void            reconnect          (size_t start);
public:
  explicit        AudioCombo         (SpeakerArrangement iobuses = SpeakerArrangement::STEREO);
  virtual        ~AudioCombo         ();
  void            insert             (AudioProcessorP proc, size_t pos = ~size_t (0));
  bool            remove             (AudioProcessor &proc);
  AudioProcessorP at                 (uint nth);
  ssize_t         find_pos           (AudioProcessor &proc);
  size_t          size               ();
  void            set_event_source   (AudioProcessorP eproc);
  AudioProcessorS list_processors_mt () const;
};

class AudioChain : public AudioCombo {
public:
  explicit        AudioChain       (SpeakerArrangement iobuses = SpeakerArrangement::STEREO);
  virtual        ~AudioChain       ();
  void            query_info       (AudioProcessorInfo &info) const override;
};

} // Ase

#endif // __ASE_COMBO_HH__
