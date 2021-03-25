// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_COMBO_HH__
#define __ASE_COMBO_HH__

#include <ase/processor.hh>

namespace Ase {

class AudioCombo : public AudioProcessor, protected ProcessorManager {
protected:
  AudioProcessorS  processors_;
  AudioProcessorP  eproc_;
  virtual void    reconnect          (size_t index, bool insertion) = 0;
  explicit        AudioCombo         ();
  virtual        ~AudioCombo         ();
public:
  void            insert             (AudioProcessorP proc, size_t pos = ~size_t (0));
  bool            remove             (AudioProcessor &proc);
  AudioProcessorP at                 (uint nth);
  ssize_t         find_pos           (AudioProcessor &proc);
  size_t          size               ();
  void            set_event_source   (AudioProcessorP eproc);
  AudioProcessorS list_processors    () const;
};

class AudioChain : public AudioCombo {
  ASE_CLASS_DECLS (Inlet);
  const SpeakerArrangement ispeakers_ = SpeakerArrangement (0);
  const SpeakerArrangement ospeakers_ = SpeakerArrangement (0);
  InletP           inlet_;
  AudioProcessor  *last_output_ = nullptr;
protected:
  void     initialize        (SpeakerArrangement busses) override;
  void     reset             () override;
  void     render            (uint n_frames) override;
  void     schedule_children () override;
  void     reconnect         (size_t index, bool insertion) override;
  uint     chain_up          (AudioProcessor &pfirst, AudioProcessor &psecond);
public:
  explicit AudioChain        (SpeakerArrangement iobuses = SpeakerArrangement::STEREO);
  virtual ~AudioChain        ();
  void     query_info        (AudioProcessorInfo &info) const override;
};

} // Ase

#endif // __ASE_COMBO_HH__
