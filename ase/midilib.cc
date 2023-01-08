// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "midilib.hh"
#include "server.hh"
#include "internal.hh"

#define MDEBUG(...)     Ase::debug ("midifeed", __VA_ARGS__)

namespace Ase {

/// Namespace used for Midi Processor implementations.
namespace MidiLib {

struct TickEvent {
  int64_t tick;
  MidiEvent event;
};
template<ssize_t DIR>
struct CmpTickEvents {
  int64_t
  operator() (const TickEvent &a, const TickEvent &b) const
  {
    if (DIR > 0)
      return int64_t (a.tick) - int64_t (b.tick);
    else
      return int64_t (b.tick) - int64_t (a.tick);
  }
};
// static const CmpTickEvents<+1> forward_cmp_ticks;
static const CmpTickEvents<-1> backward_cmp_ticks;

// == MidiProducerImpl ==
class MidiProducerImpl : public MidiProducerIface {
  using ClipGenerator = ClipImpl::Generator;
  MidiFeedP feed_;
  Position *position_ = nullptr;
  int64 generator_start_ = -1;
  bool must_flush = false;
  std::vector<TickEvent> future_stack; // newest events at back()
  FastMemory::Block position_block_;
public:
  MidiProducerImpl (AudioEngine &engine) :
    MidiProducerIface (engine)
  {
    position_block_ = SERVER->telemem_allocate (sizeof (Position));
    position_ = new (position_block_.block_start) Position {};
    future_stack.reserve (64); // likely enough to avoid malloc
  }
  ~MidiProducerImpl()
  {
    position_->~Position();
    position_ = nullptr;
    SERVER->telemem_release (position_block_);
  }
  static void
  static_info (AudioProcessorInfo &info)
  {} // avoid public listing
  void
  initialize (SpeakerArrangement busses) override
  {
    remove_all_buses();
    prepare_event_input();
    prepare_event_output();
  }
  void
  update_feed (MidiFeedP &feed) override
  {
    // save current play_position
    const int64 last_play_position = position_->current >= 0 ? feed_->generators[position_->current].play_position() : 0;
    // swap shared_ptr so lengthy dtors are executed in another thread
    MidiFeedP old = feed_;
    std::swap (feed_, feed);
    // restore current play_position
    if (position_->current >= 0)
      {
        if (feed_ && position_->current < feed_->generators.size())
          feed_->generators[position_->current].jumpto (last_play_position);
        else
          {
            must_flush = must_flush || position_->current != -1;
            position_->current = -1;
          }
      }
  }
  void
  reset (uint64 target_stamp) override
  {
    position_->next = -1;
    position_->current = -1;
    position_->tick = -M52MAX;
    future_stack.clear();
    must_flush = false;
  }
  void
  start () override
  {
    if (feed_ && feed_->generators.size())
      {
        if (position_->current < 0)
          {
            position_->current = 0;
            generator_start_ = transport().current_bar_tick;
            feed_->generators[position_->current].jumpto (0);
          }
      }
    // TODO: handle start within bars
  }
  void
  stop (bool restart) override
  {
    position_->tick = -M52MAX;
    must_flush = true;
    if (restart)
      {
        position_->current = -1;
        generator_start_ = -1;
      }
  }
  void
  render (uint n_frames) override
  {
    const AudioTransport &transport = this->transport();
    MidiEventRange evinp = get_event_input(); // treat MIDI input as MIDI through
    MidiEventStream &evout = get_event_output(); // needs prepare_event_output()
    const int64 begin_tick = transport.current_tick;
    const int64 end_tick = transport.current_tick + transport.sample_to_tick (n_frames);
    const int64 bpm = transport.current_bpm;
    // flush NOTE_OFF events
    if (ASE_UNLIKELY (must_flush || bpm <= 0))
      {
        must_flush = false;
        for (ssize_t i = future_stack.size() - 1; i >= 0; i--)
          {
            TickEvent tnote = future_stack[i];
            const int64 frame0 = 0;
            if (tnote.event.type == MidiEvent::NOTE_OFF)
              {
                evout.append_unsorted (frame0, tnote.event);
                MDEBUG ("FLUSH: t=%d ev=%s f=%d\n", tnote.tick, tnote.event.to_string(), frame0);
              }
          }
        future_stack.resize (0);
      }
    // enqueue pending NOTE_OFF events
    while (future_stack.size() && future_stack.back().tick < end_tick)
      {
        TickEvent tnote = future_stack.back();
        future_stack.pop_back();
        const int64 frame = transport.sample_from_tick (tnote.tick - begin_tick);
        assert_paranoid (frame >= -2048 && frame <= 2047);
        MDEBUG ("POP: t=%d ev=%s f=%d\n", tnote.tick, tnote.event.to_string(), frame);
        evout.append_unsorted (frame, tnote.event);
      }
    // enqueue pending MIDI input events
    for (const MidiEvent *midi_through = evinp.begin(); ASE_UNLIKELY (midi_through < evinp.end()); midi_through++)
      {
        MDEBUG ("THROUGH: f=%+3d ev=%s\n", midi_through->frame, midi_through->to_string());
        evout.append (midi_through->frame, *midi_through);
      }
    // enqueue new events, keep queue of future events generated on the fly (NOTE-OFF)
    if (ASE_ISLIKELY (feed_ && feed_->generators.size() &&
                      bpm > 0 && position_->current >= 0 &&
                      generator_start_ >= 0)) // in playback
      {
        // generate up to end_tick
        while (position_->current >= 0 &&
               generator_start_ + feed_->generators[position_->current].play_position() < end_tick)
          {
            // handler for incoming events
            auto qevent = [begin_tick, end_tick, &transport, &evout, this] (int64 cliptick, MidiEvent &event) {
              const int64 etick = generator_start_ + cliptick; // Generator tick to Engine tick
              if (etick < end_tick)
                {
                  const int64 frame = transport.sample_from_tick (etick - begin_tick);
                  assert_paranoid (frame >= -2048 && frame <= 2047);
                  // interleave with earlier MIDI through events
                  evout.append_unsorted (frame, event);
                  MDEBUG ("NOW: t=%d ev=%s f=%d\n", etick, event.to_string(), frame);
                }
              else
                {
                  TickEvent future_event { etick, event };
                  Aux::insert_sorted (future_stack, future_event, backward_cmp_ticks);
                  MDEBUG ("FUT: t=%d ev=%s f=%d\n", etick, event.to_string(), transport.sample_from_tick (etick - begin_tick));
                }
            };
            // generate events for this block
            const int64 advanced = feed_->generators[position_->current].generate (end_tick - generator_start_, qevent);
            (void) advanced;
            // handle generator succession
            if (feed_->generators[position_->current].done())
              {
                const int64 play_point = generator_start_ + feed_->generators[position_->current].play_position();
                assert_paranoid (play_point >= begin_tick && play_point <= end_tick);
                position_->current = feed_->scout.advance (position_->current);
                if (position_->current >= 0)
                  {
                    generator_start_ = transport.current_bar_tick;
                    while (generator_start_ < play_point)
                      generator_start_ += transport.tick_sig.bar_ticks();
                    feed_->generators[position_->current].jumpto (0);
                    if (feed_->generators[position_->current].done())
                      position_->current = -1;
                  }
                if (position_->current == -1)
                  generator_start_ = -1;
                position_->next = -1;
              }
            // position_->tick = r.etick - generator_start_; // externel tick
            if (position_->current >= 0)
              position_->tick = feed_->generators[position_->current].clip_position(); // internal
            else
              position_->tick = -M52MAX;
          }
      }
    // ensure ascending event tick order
    evout.ensure_order();
  }
  Position*
  position () const override
  {
    return position_;
  }
};

static auto midilib_midiinputimpl = register_audio_processor<MidiProducerImpl>();

} // MidiLib
} // Ase
