// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "processor.hh"
#include "combo.hh"
#include "device.hh"
#include "main.hh"      // feature_toggle_find
#include "utils.hh"
#include "engine.hh"
#include "internal.hh"
#include <shared_mutex>

#define PDEBUG(...)     Ase::debug ("processor", __VA_ARGS__)

namespace Ase {

// == PBus ==
AudioProcessor::PBus::PBus (const String &ident, const String &uilabel, SpeakerArrangement sa) :
  ibus (ident, uilabel, sa)
{}

AudioProcessor::IBus::IBus (const String &identifier, const String &uilabel, SpeakerArrangement sa)
{
  ident = identifier;
  label = uilabel;
  speakers = sa;
  proc = nullptr;
  obusid = {};
  assert_return (ident != "");
}

AudioProcessor::OBus::OBus (const String &identifier, const String &uilabel, SpeakerArrangement sa)
{
  ident = identifier;
  label = uilabel;
  speakers = sa;
  fbuffer_concounter = 0;
  fbuffer_count = 0;
  fbuffer_index = ~0;
  assert_return (ident != "");
}

// == AudioProcessor ==
const String AudioProcessor::GUIONLY = ":G:r:w:";     ///< GUI READABLE WRITABLE
const String AudioProcessor::STANDARD = ":G:S:r:w:";  ///< GUI STORAGE READABLE WRITABLE
const String AudioProcessor::STORAGEONLY = ":S:r:w:"; ///< STORAGE READABLE WRITABLE
uint64 __thread AudioProcessor::tls_timestamp = 0;

/// Constructor for AudioProcessor
AudioProcessor::AudioProcessor (AudioEngine &engine) :
  engine_ (engine)
{
  engine_.processor_count_ += 1;
}

/// The destructor is called when the last std::shared_ptr<> reference drops.
AudioProcessor::~AudioProcessor ()
{
  remove_all_buses();
  engine_.processor_count_ -= 1;
  delete atomic_bits_;
}

/// Convert MIDI note to Hertz according to the current MusicalTuning.
float
AudioProcessor::note_to_freq (int note) const
{
  return MidiNote::note_to_freq (MusicalTuning::OD_12_TET, note, 440);
}

/// Gain access to the Device handle of `this` AudioProcessor.
DeviceP
AudioProcessor::get_device() const
{
  std::weak_ptr<Device> &wptr = const_cast<AudioProcessor*> (this)->device_;
  DeviceP devicep = wptr.lock();
  return devicep;
}

const AudioProcessor::FloatBuffer&
AudioProcessor::zero_buffer()
{
  static const FloatBuffer const_zero_float_buffer {};
  return const_zero_float_buffer;
}

/// Get internal output bus handle.
AudioProcessor::OBus&
AudioProcessor::iobus (OBusId obusid)
{
  const size_t busindex = size_t (obusid) - 1;
  if (ASE_ISLIKELY (busindex < n_obuses()))
    return iobuses_[output_offset_ + busindex].obus;
  static const OBus dummy { "?", "", {} };
  assert_return (busindex < n_obuses(), const_cast<OBus&> (dummy));
  return const_cast<OBus&> (dummy);
}

/// Get internal input bus handle.
AudioProcessor::IBus&
AudioProcessor::iobus (IBusId ibusid)
{
  const size_t busindex = size_t (ibusid) - 1;
  if (ASE_ISLIKELY (busindex < n_ibuses()))
    return iobuses_[busindex].ibus;
  static const IBus dummy { "?", "", {} };
  assert_return (busindex < n_ibuses(), const_cast<IBus&> (dummy));
  return const_cast<IBus&> (dummy);
}

// Release buffers allocated for input/output channels.
void
AudioProcessor::release_iobufs()
{
  disconnect_ibuses();
  disconnect_obuses();
  for (OBusId ob = OBusId (1); size_t (ob) <= n_obuses(); ob = OBusId (size_t (ob) + 1))
    {
      OBus &bus = iobus (ob);
      bus.fbuffer_index = ~0;
      bus.fbuffer_count = 0;
    }
  // trivial destructors (of FloatBuffer) need not be called
  static_assert (std::is_trivially_destructible<decltype (fbuffers_[0])>::value);
  fast_mem_free (fbuffers_);
  fbuffers_ = nullptr;
}

// Allocate and assign output channel buffers.
void
AudioProcessor::assign_iobufs ()
{
  ssize_t ochannel_count = 0;
  for (OBusId ob = OBusId (1); size_t (ob) <= n_obuses(); ob = OBusId (size_t (ob) + 1))
    {
      OBus &bus = iobus (ob);
      bus.fbuffer_index = ochannel_count;
      bus.fbuffer_count = bus.n_channels();
      ochannel_count += bus.fbuffer_count;
    }
  fast_mem_free (fbuffers_);
  if (ochannel_count > 0)
    {
      fbuffers_ = (FloatBuffer*) fast_mem_alloc (ochannel_count * sizeof (FloatBuffer));
      for (ssize_t i = 0; i < ochannel_count; i++)
        new (fbuffers_ + i) FloatBuffer();
    }
  else
    fbuffers_ = nullptr;
}

static __thread CString tls_param_group;

/// Introduce a `ParamInfo.group` to be used for the following add_param() calls.
void
AudioProcessor::start_group (const String &groupname) const
{
  tls_param_group = groupname;
}

/// Helper for add_param() to generate the sequentially next ParamId.
ParamId
AudioProcessor::nextid () const
{
  const uint pmax = pparams_.size();
  const uint last = pparams_.empty() ? 0 : uint (pparams_.back().id);
  return ParamId (MAX (pmax, last) + 1);
}

/// Add a new parameter with unique `ParamInfo.identifier`.
/// The returned `ParamId` is forced to match `id` (and must be unique).
ParamId
AudioProcessor::add_param (Id32 id, const Param &initparam, double value)
{
  assert_return (uint (id) > 0, ParamId (0));
  assert_return (!is_initialized(), {});
  assert_return (initparam.label != "", {});
  if (pparams_.size())
    assert_return (initparam.label != pparams_.back().parameter->label(), {}); // easy CnP error
  Param iparam = initparam;
  if (iparam.group.empty())
    iparam.group = tls_param_group;
  ParameterP parameterp = std::make_shared<Parameter> (iparam);
  ParameterC parameterc = parameterp;
  if (pparams_.size())
    assert_return (parameterc->ident() != pparams_.back().parameter->ident(), {}); // easy CnP error
  PParam pparam { ParamId (id.id), uint (1 + pparams_.size()), parameterc };
  using P = decltype (pparams_);
  std::pair<P::iterator, bool> existing_parameter_position =
    Aux::binary_lookup_insertion_pos (pparams_.begin(), pparams_.end(), PParam::cmp, pparam);
  assert_return (existing_parameter_position.second == false, {});
  pparams_.insert (existing_parameter_position.first, std::move (pparam));
  set_param (pparam.id, value); // forces dirty
  parameterp->initialsync (peek_param_mt (pparam.id));
  return pparam.id;
}

/// Add new range parameter with most `ParamInfo` fields as inlined arguments.
/// The returned `ParamId` is newly assigned and increases with the number of parameters added.
/// The `clabel` is the canonical, non-translated name for this parameter, its
/// hyphenated lower case version is used for serialization.
ParamId
AudioProcessor::add_param (Id32 id, const String &clabel, const String &nickname,
                           double pmin, double pmax, double value,
                           const String &unit, String hints,
                           const String &blurb, const String &description)
{
  assert_return (uint (id) > 0, ParamId (0));
  return add_param (id, Param {
      .label = clabel,
      .nick = nickname,
      .initial = value,
      .unit = unit,
      .extras = MinMaxStep { pmin, pmax, 0 },
      .hints = Parameter::construct_hints (hints, "", pmin, pmax),
      .blurb = blurb,
      .descr = description,
    }, value);
}

/// Variant of AudioProcessor::add_param() with sequential `id` generation.
ParamId
AudioProcessor::add_param (const String &clabel, const String &nickname,
                           double pmin, double pmax, double value,
                           const String &unit, String hints,
                           const String &blurb, const String &description)
{
  return add_param (nextid(), clabel, nickname, pmin, pmax, value, unit, hints, blurb, description);
}

/// Add new choice parameter with most `ParamInfo` fields as inlined arguments.
/// The returned `ParamId` is newly assigned and increases with the number of parameters added.
/// The `clabel` is the canonical, non-translated name for this parameter, its
/// hyphenated lower case version is used for serialization.
ParamId
AudioProcessor::add_param (Id32 id, const String &clabel, const String &nickname,
                           ChoiceS &&centries, double value, String hints,
                           const String &blurb, const String &description)
{
  assert_return (uint (id) > 0, ParamId (0));
  const double pmax = centries.size();
  return add_param (id, Param {
      .label = clabel,
      .nick = nickname,
      .initial = value,
      .extras = centries,
      .hints = Parameter::construct_hints (hints, "choice", 0, pmax),
      .blurb = blurb,
      .descr = description,
    }, value);
}

/// Variant of AudioProcessor::add_param() with sequential `id` generation.
ParamId
AudioProcessor::add_param (const String &clabel, const String &nickname,
                           ChoiceS &&centries, double value, String hints,
                           const String &blurb, const String &description)
{
  return add_param (nextid(), clabel, nickname, std::forward<ChoiceS> (centries), value, hints, blurb, description);
}

/// Add new toggle parameter with most `ParamInfo` fields as inlined arguments.
/// The returned `ParamId` is newly assigned and increases with the number of parameters added.
/// The `clabel` is the canonical, non-translated name for this parameter, its
/// hyphenated lower case version is used for serialization.
ParamId
AudioProcessor::add_param (Id32 id, const String &clabel, const String &nickname,
                           bool boolvalue, String hints,
                           const String &blurb, const String &description)
{
  assert_return (uint (id) > 0, ParamId (0));
  using namespace MakeIcon;
  return add_param (id, Param {
      .label = clabel,
      .nick = nickname,
      .initial = boolvalue,
      .hints = Parameter::construct_hints (hints, "toggle", false, true),
      .blurb = blurb,
      .descr = description,
    }, boolvalue);
}

/// Variant of AudioProcessor::add_param() with sequential `id` generation.
ParamId
AudioProcessor::add_param (const String &clabel, const String &nickname,
                           bool boolvalue, String hints,
                           const String &blurb, const String &description)
{
  return add_param (nextid(), clabel, nickname, boolvalue, hints, blurb, description);
}

/// Return the ParamId for parameter `identifier` or else 0.
auto
AudioProcessor::find_param (const String &identifier) const -> MaybeParamId
{
  auto ident = CString::lookup (identifier);
  if (!ident.empty())
    for (const PParam &pp : pparams_)
      if (pp.parameter->cident == ident)
        return std::make_pair (pp.id, true);
  return std::make_pair (ParamId (0), false);
}

// Non-fastpath implementation of find_param().
const AudioProcessor::PParam*
AudioProcessor::find_pparam_ (ParamId paramid) const
{
  // lookup id with gaps
  const PParam key { paramid };
  auto iter = Aux::binary_lookup (pparams_.begin(), pparams_.end(), PParam::cmp, key);
  const bool found_paramid = iter != pparams_.end();
  if (ISLIKELY (found_paramid))
    return &*iter;
  assert_return (found_paramid == true, nullptr);
  return nullptr;
}

/// Set parameter `id` to `value` within `ParamInfo.get_minmax()`.
bool
AudioProcessor::set_param (Id32 paramid, const double value, bool sendnotify)
{
  const PParam *pparam = find_pparam (ParamId (paramid.id));
  return_unless (pparam, false);
  ParameterC parameter = pparam->parameter;
  double v = value;
  if (parameter)
    {
      const auto [fmin, fmax, stepping] = parameter->range();
      v = CLAMP (v, fmin, fmax);
      if (stepping > 0)
        {
          // round halfway cases down, so:
          // 0 -> -0.5…+0.5 yields -0.5
          // 1 -> -0.5…+0.5 yields +0.5
          constexpr const auto nearintoffset = 0.5 - DOUBLE_EPSILON; // round halfway case down
          v = stepping * uint64_t ((v - fmin) / stepping + nearintoffset);
          v = CLAMP (fmin + v, fmin, fmax);
        }
    }
  const bool need_notify = const_cast<PParam*> (pparam)->assign (v);
  if (need_notify && sendnotify && !pparam->aprop_.expired())
    enotify_enqueue_mt (PARAMCHANGE);
  return need_notify;
}

/// Retrieve supplemental information for parameters, usually to enhance the user interface.
ParameterC
AudioProcessor::parameter (Id32 paramid) const
{
  const PParam *pparam = this->find_pparam (ParamId (paramid.id));
  return ASE_ISLIKELY (pparam) ? pparam->parameter : nullptr;
}

/// Fetch the current parameter value of a AudioProcessor.
/// This function does not modify the parameter `dirty` flag.
/// This function is MT-Safe after proper AudioProcessor initialization.
double
AudioProcessor::peek_param_mt (Id32 paramid) const
{
  const PParam *param = find_pparam (ParamId (paramid.id));
  return ASE_ISLIKELY (param) ? param->peek() : FP_NAN;
}

/// Fetch the current parameter value of a AudioProcessor from any thread.
/// This function is MT-Safe after proper AudioProcessor initialization.
double
AudioProcessor::param_peek_mt (const AudioProcessorP proc, Id32 paramid)
{
  assert_return (proc, FP_NAN);
  assert_return (proc->is_initialized(), FP_NAN);
  return proc->peek_param_mt (paramid);
}

double
AudioProcessor::value_to_normalized (Id32 paramid, double value) const
{
  const PParam *const pparam = find_pparam (paramid);
  assert_return (pparam != nullptr, 0);
  const auto [fmin, fmax, step] = pparam->parameter->range();
  const double normalized = (value - fmin) / (fmax - fmin);
  assert_return (normalized >= 0.0 && normalized <= 1.0, CLAMP (normalized, 0.0, 1.0));
  return normalized;
}

double
AudioProcessor::value_from_normalized (Id32 paramid, double normalized) const
{
  const PParam *const pparam = find_pparam (paramid);
  assert_return (pparam != nullptr, 0);
  const auto [fmin, fmax, step] = pparam->parameter->range();
  const double value = fmin + normalized * (fmax - fmin);
  assert_return (normalized >= 0.0 && normalized <= 1.0, value);
  return value;
}

/// Get param value normalized into 0…1.
double
AudioProcessor::get_normalized (Id32 paramid)
{
  return value_to_normalized (paramid, get_param (paramid));
}

/// Set param value normalized into 0…1.
bool
AudioProcessor::set_normalized (Id32 paramid, double normalized, bool sendnotify)
{
  if (!ASE_ISLIKELY (normalized >= 0.0))
    normalized = 0;
  else if (!ASE_ISLIKELY (normalized <= 1.0))
    normalized = 1.0;
  return set_param (paramid, value_from_normalized (paramid, normalized), sendnotify);
}

/** Format a parameter `paramid` value as text string.
 * Currently, this function may be called from any thread,
 * so `this` must be treated as `const` (it might be used
 * concurrently by a different thread).
 */
String
AudioProcessor::param_value_to_text (Id32 paramid, double value) const
{
  const PParam *pparam = find_pparam (ParamId (paramid.id));
  return pparam && pparam->parameter ? pparam->parameter->value_to_text (value) : "";
}

/** Extract a parameter `paramid` value from a text string.
 * The string might contain unit information or consist only of
 * number characters. Non-recognized characters should be ignored,
 * so a best effort conversion is always undertaken.
 * This function may be called from any thread,
 * so `this` must be treated as `const` (it might be used
 * concurrently by a different thread).
 */
double
AudioProcessor::param_value_from_text (Id32 paramid, const String &text) const
{
  const PParam *pparam = find_pparam (ParamId (paramid.id));
  return pparam && pparam->parameter ? pparam->parameter->value_from_text (text).as_double() : 0.0;
}

/// Prepare `count` bits for atomic notifications.
void
AudioProcessor::atomic_bits_resize (size_t count)
{
  if (atomic_bits_)
    delete atomic_bits_;
  atomic_bits_ = new AtomicBits (count);
}

/// Set the nth atomic notification bit, return if enotify_enqueue_mt() is needed.
bool
AudioProcessor::atomic_bit_notify (size_t nth)
{
  assert_return (atomic_bits_ && nth < atomic_bits_->size() * 64, false);
  const bool last = atomic_bits_->iter (nth).set (1);
  const bool need_wakeup = last == false;
  return need_wakeup;
}

/// Allow iterations over the atomic bits.
AtomicBits::Iter
AudioProcessor::atomic_bits_iter (size_t pos) const
{
  return_unless (atomic_bits_ && pos < atomic_bits_->size() * 64, {});
  return atomic_bits_->iter (pos);
}

/// Check if AudioProcessor has been properly intiialized (so the parameter set is fixed).
bool
AudioProcessor::is_initialized () const
{
  return flags_ & INITIALIZED;
}

/// Retrieve the minimum / maximum values for a parameter.
AudioProcessor::MinMax
AudioProcessor::param_range (Id32 paramid) const
{
  ParameterC parameterc = parameter (paramid);
  return_unless (parameterc, MinMax { FP_NAN, FP_NAN });
  const auto [fmin, fmax, step] = parameterc->range();
  return MinMax { fmin, fmax };
}

String
AudioProcessor::debug_name () const
{
  return typeid_name (*this);
}

/// Mandatory method to setup parameters and I/O busses.
/// See add_param(), add_input_bus() / add_output_bus().
/// This method will be called once per instance after construction.
void
AudioProcessor::initialize (SpeakerArrangement busses)
{
  assert_return (n_ibuses() + n_obuses() == 0);
}

/// Request recreation of the audio engine rendering schedule.
void
AudioProcessor::reschedule ()
{
  engine_.schedule_queue_update();
}

/// Configure if the main output of this module is mixed into the engine output.
void
AudioProcessor::enable_engine_output (bool onoff)
{
  if (onoff)
    assert_return (n_obuses() || has_event_output());
  engine_.enable_output (*this, onoff);
}

/// Prepare the AudioProcessor to receive Event objects during render() via get_event_input().
/// Note, remove_all_buses() will remove the Event input created by this function.
void
AudioProcessor::prepare_event_input ()
{
  if (!estreams_)
    estreams_ = new EventStreams();
  assert_return (estreams_->has_event_input == false);
  estreams_->has_event_input = true;
}

static const MidiEventStream empty_event_stream; // dummy

/// Access the current input EventRange during render(), needs prepare_event_input().
MidiEventRange
AudioProcessor::get_event_input ()
{
  assert_return (estreams_ && estreams_->has_event_input, MidiEventRange (empty_event_stream));
  if (estreams_ && estreams_->oproc && estreams_->oproc->estreams_)
    return MidiEventRange (estreams_->oproc->estreams_->estream);
  return MidiEventRange (empty_event_stream);
}

/// Prepare the AudioProcessor to produce Event objects during render() via get_event_output().
/// Note, remove_all_buses() will remove the Event output created by this function.
void
AudioProcessor::prepare_event_output ()
{
  if (!estreams_)
    estreams_ = new EventStreams();
  assert_return (estreams_->has_event_output == false);
  estreams_->has_event_output = true;
}

/// Access the current output EventStream during render(), needs prepare_event_input().
MidiEventStream&
AudioProcessor::get_event_output ()
{
  assert_return (estreams_ != nullptr, const_cast<MidiEventStream&> (empty_event_stream));
  return estreams_->estream;
}

/// Disconnect event input if a connection is present.
void
AudioProcessor::disconnect_event_input()
{
  if (estreams_ && estreams_->oproc)
    {
      AudioProcessor &oproc = *estreams_->oproc;
      assert_return (oproc.estreams_);
      const bool backlink = Aux::erase_first (oproc.outputs_, [&] (auto &e) { return e.proc == this && e.ibusid == EventStreams::EVENT_ISTREAM; });
      estreams_->oproc = nullptr;
      reschedule();
      assert_return (backlink == true);
      enotify_enqueue_mt (BUSDISCONNECT);
      oproc.enotify_enqueue_mt (BUSDISCONNECT);
    }
}

/// Connect event input to event output of AudioProcessor `oproc`.
void
AudioProcessor::connect_event_input (AudioProcessor &oproc)
{
  assert_return (has_event_input());
  assert_return (oproc.has_event_output());
  if (estreams_ && estreams_->oproc)
    disconnect_event_input();
  estreams_->oproc = &oproc;
  // register backlink
  oproc.outputs_.push_back ({ this, EventStreams::EVENT_ISTREAM });
  reschedule();
  enotify_enqueue_mt (BUSCONNECT);
  oproc.enotify_enqueue_mt (BUSCONNECT);
}

/// Add an input bus with `uilabel` and channels configured via `speakerarrangement`.
IBusId
AudioProcessor::add_input_bus (CString uilabel, SpeakerArrangement speakerarrangement,
                               const String &hints, const String &blurb)
{
  assert_return (!is_initialized(), {});
  assert_return (uilabel != "", {});
  assert_return (uint64_t (speaker_arrangement_channels (speakerarrangement)) > 0, {});
  assert_return (iobuses_.size() < 65535, {});
  if (n_ibuses())
    assert_return (uilabel != iobus (IBusId (n_ibuses())).label, {}); // easy CnP error
  PBus pbus { string_to_identifier (uilabel), uilabel, speakerarrangement };
  pbus.pbus.hints = hints;
  pbus.pbus.blurb = blurb;
  iobuses_.insert (iobuses_.begin() + output_offset_, pbus);
  output_offset_ += 1;
  const IBusId id = IBusId (n_ibuses());
  return id; // 1 + index
}

/// Add an output bus with `uilabel` and channels configured via `speakerarrangement`.
OBusId
AudioProcessor::add_output_bus (CString uilabel, SpeakerArrangement speakerarrangement,
                                const String &hints, const String &blurb)
{
  assert_return (!is_initialized(), {});
  assert_return (uilabel != "", {});
  assert_return (uint64_t (speaker_arrangement_channels (speakerarrangement)) > 0, {});
  assert_return (iobuses_.size() < 65535, {});
  if (n_obuses())
    assert_return (uilabel != iobus (OBusId (n_obuses())).label, {}); // easy CnP error
  PBus pbus { string_to_identifier (uilabel), uilabel, speakerarrangement };
  pbus.pbus.hints = hints;
  pbus.pbus.blurb = blurb;
  iobuses_.push_back (pbus);
  const OBusId id = OBusId (n_obuses());
  return id; // 1 + index
}

/// Return the IBusId for input bus `uilabel` or else 0.
IBusId
AudioProcessor::find_ibus (const String &uilabel) const
{
  auto ident = CString::lookup (uilabel);
  if (!ident.empty())
    for (IBusId ib = IBusId (1); size_t (ib) <= n_ibuses(); ib = IBusId (size_t (ib) + 1))
      if (iobus (ib).ident == ident)
        return ib;
  return IBusId (0);
}

/// Return the OBusId for output bus `uilabel` or else 0.
OBusId
AudioProcessor::find_obus (const String &uilabel) const
{
  auto ident = CString::lookup (uilabel);
  if (!ident.empty())
    for (OBusId ob = OBusId (1); size_t (ob) <= n_obuses(); ob = OBusId (size_t (ob) + 1))
      if (iobus (ob).ident == ident)
        return ob;
  return OBusId (0);
}

/// Retrieve an input channel float buffer.
/// The returned FloatBuffer is not modifyable, for AudioProcessor implementations
/// that need to modify the FloatBuffer of an output channel, see float_buffer(OBusId,uint).
const AudioProcessor::FloatBuffer&
AudioProcessor::float_buffer (IBusId busid, uint channelindex) const
{
  const size_t ibusindex = size_t (busid) - 1;
  assert_return (ibusindex < n_ibuses(), zero_buffer());
  const IBus &ibus = iobus (busid);
  if (ibus.proc)
    {
      const AudioProcessor &oproc = *ibus.proc;
      const OBus &obus = oproc.iobus (ibus.obusid);
      if (ASE_UNLIKELY (channelindex >= obus.fbuffer_count))
        channelindex = obus.fbuffer_count - 1;
      return oproc.fbuffers_[obus.fbuffer_index + channelindex];
    }
  return zero_buffer();
}

/// Retrieve an output channel float buffer.
/// Note that this function reassigns FloatBuffer.buffer pointer to FloatBuffer.fblock,
/// resetting any previous redirections. Assignments to FloatBuffer.buffer
/// enable new redirections until the next call to this function or oblock().
AudioProcessor::FloatBuffer&
AudioProcessor::float_buffer (OBusId busid, uint channelindex, bool resetptr)
{
  const size_t obusindex = size_t (busid) - 1;
  FloatBuffer *const fallback = const_cast<FloatBuffer*> (&zero_buffer());
  assert_return (obusindex < n_obuses(), *fallback);
  const OBus &obus = iobus (busid);
  assert_return (channelindex < obus.fbuffer_count, *fallback);
  FloatBuffer &fbuffer = fbuffers_[obus.fbuffer_index + channelindex];
  if (resetptr)
    fbuffer.buffer = &fbuffer.fblock[0];
  return fbuffer;
}

/// Redirect output buffer of bus `b`, channel `c` to point to `block`, or zeros if `block==nullptr`.
void
AudioProcessor::redirect_oblock (OBusId busid, uint channelindex, const float *block)
{
  const size_t obusindex = size_t (busid) - 1;
  assert_return (obusindex < n_obuses());
  const OBus &obus = iobus (busid);
  assert_return (channelindex < obus.fbuffer_count);
  FloatBuffer &fbuffer = fbuffers_[obus.fbuffer_index + channelindex];
  if (block != nullptr)
    fbuffer.buffer = const_cast<float*> (block);
  else
    fbuffer.buffer = const_float_zeros;
  static_assert (sizeof (const_float_zeros) / sizeof (const_float_zeros[0]) >= AUDIO_BLOCK_MAX_RENDER_SIZE);
}

/// Fill the output buffer of bus `b`, channel `c` with `v`.
void
AudioProcessor::assign_oblock (OBusId b, uint c, float v)
{
  float *const buffer = oblock (b, c);
  floatfill (buffer, v, AUDIO_BLOCK_MAX_RENDER_SIZE);
  // TODO: optimize assign_oblock() via redirect to const value blocks
}

/// Indicator for connected output buses.
/// Not connected output bus buffers do not need to be filled.
bool
AudioProcessor::connected (OBusId obusid) const
{
  const size_t obusindex = size_t (obusid) - 1;
  assert_return (obusindex < n_obuses(), false);
  return iobus (obusid).fbuffer_concounter;
}

/// Remove existing bus configurations, useful at the start of configure().
void
AudioProcessor::remove_all_buses ()
{
  release_iobufs();
  iobuses_.clear();
  output_offset_ = 0;
  if (estreams_)
    {
      assert_return (!estreams_->oproc && outputs_.empty());
      delete estreams_; // must be disconnected beforehand
      estreams_ = nullptr;
      reschedule();
    }
}

/// Reset input bus buffer data.
void
AudioProcessor::disconnect_ibuses()
{
  disconnect (EventStreams::EVENT_ISTREAM);
  if (n_ibuses())
    reschedule();
  for (size_t i = 0; i < n_ibuses(); i++)
    disconnect (IBusId (1 + i));
}

/// Disconnect inputs of all Processors that are connected to outputs of `this`.
void
AudioProcessor::disconnect_obuses()
{
  return_unless (fbuffers_);
  if (outputs_.size())
    reschedule();
  while (outputs_.size())
    {
      const auto o = outputs_.back();
      o.proc->disconnect (o.ibusid);
    }
}

/// Disconnect input `ibusid`.
void
AudioProcessor::disconnect (IBusId ibusid)
{
  if (EventStreams::EVENT_ISTREAM == ibusid)
    return disconnect_event_input();
  const size_t ibusindex = size_t (ibusid) - 1;
  assert_return (ibusindex < n_ibuses());
  IBus &ibus = iobus (ibusid);
  // disconnect
  return_unless (ibus.proc != nullptr);
  AudioProcessor &oproc = *ibus.proc;
  const size_t obusindex = size_t (ibus.obusid) - 1;
  assert_return (obusindex < oproc.n_obuses());
  OBus &obus = oproc.iobus (ibus.obusid);
  assert_return (obus.fbuffer_concounter > 0);
  obus.fbuffer_concounter -= 1; // connection counter
  const bool backlink = Aux::erase_first (oproc.outputs_, [&] (auto &e) { return e.proc == this && e.ibusid == ibusid; });
  ibus.proc = nullptr;
  ibus.obusid = {};
  reschedule();
  assert_return (backlink == true);
  enotify_enqueue_mt (BUSDISCONNECT);
  oproc.enotify_enqueue_mt (BUSDISCONNECT);
}

/// Connect input `ibusid` to output `obusid` of AudioProcessor `prev`.
void
AudioProcessor::connect (IBusId ibusid, AudioProcessor &oproc, OBusId obusid)
{
  const size_t ibusindex = size_t (ibusid) - 1;
  assert_return (ibusindex < n_ibuses());
  const size_t obusindex = size_t (obusid) - 1;
  assert_return (obusindex < oproc.n_obuses());
  disconnect (ibusid);
  IBus &ibus = iobus (ibusid);
  const uint n_ichannels = ibus.n_channels();
  OBus &obus = oproc.iobus (obusid);
  const uint n_ochannels = obus.n_channels();
  // match channels
  assert_return (n_ichannels <= n_ochannels ||
                 (ibus.speakers == SpeakerArrangement::STEREO &&
                  obus.speakers == SpeakerArrangement::MONO));
  // connect
  ibus.proc = &oproc;
  ibus.obusid = obusid;
  // register backlink
  obus.fbuffer_concounter += 1; // conection counter
  oproc.outputs_.push_back ({ this, ibusid });
  reschedule();
  enotify_enqueue_mt (BUSCONNECT);
  oproc.enotify_enqueue_mt (BUSCONNECT);
}

/// Ensure `AudioProcessor::initialize()` has been called, so the parameters are fixed.
void
AudioProcessor::ensure_initialized (DeviceP devicep)
{
  if (!is_initialized())
    {
      assert_return (n_ibuses() + n_obuses() == 0);
      assert_return (get_device() == nullptr);
      device_ = devicep;
      tls_param_group = "";
      initialize (engine_.speaker_arrangement());
      tls_param_group = "";
      flags_ |= INITIALIZED;
      if (n_ibuses() + n_obuses() == 0 &&
          (!estreams_ || (!estreams_->has_event_input && !estreams_->has_event_output)))
        warning ("AudioProcessor::%s: initialize() failed to add input/output busses for: %s", __func__, debug_name());
      assign_iobufs();
      reset_state (engine_.frame_counter());
    }
  const bool has_event_input = estreams_ && estreams_->has_event_input;
  const bool has_event_output = estreams_ && estreams_->has_event_output;
  assert_return (n_ibuses() || n_obuses() || has_event_input || has_event_output);
}

/// Reset all voices, buffers and other internal state
void
AudioProcessor::reset_state (uint64 target_stamp)
{
  if (render_stamp_ != target_stamp)
    {
      if (estreams_)
        estreams_->estream.clear();
      reset (target_stamp);
      render_stamp_ = target_stamp;
    }
}

/// Reset all state variables.
void
AudioProcessor::reset (uint64 target_stamp)
{}

/// Schedule this node and its dependencies for engine rendering.
uint
AudioProcessor::schedule_processor()
{
  uint level = 0;
  if (estreams_ && estreams_->oproc)
    {
      const uint l = schedule_processor (*estreams_->oproc);
      level = std::max (level, l);
    }
  for (size_t i = 0; i < n_ibuses(); i++)
    {
      IBus &ibus = iobus (IBusId (1 + i));
      if (ibus.proc)
        {
          const uint l = schedule_processor (*ibus.proc);
          level = std::max (level, l);
        }
    }
  const uint l = schedule_children();
  level = std::max (level, l);
  engine_.schedule_add (*this, level);
  return level + 1;
}

/** Method called for every audio buffer to be processed.
 * Each connected output bus needs to be filled with `n_frames`,
 * i.e. `n_frames` many floating point samples per channel.
 * Using the AudioSignal::OBusId (see add_output_bus()),
 * the floating point sample buffers can be addressed via the BusConfig structure as:
 * `bus[obusid].channel[nth].buffer`, see FloatBuffer for further details.
 * The AudioSignal::IBusId (see add_input_bus()) can be used
 * correspondingly to retrieve input channel values.
 */
void
AudioProcessor::render (uint32 n_frames)
{}

void
AudioProcessor::render_block (uint64 target_stamp)
{
  return_unless (render_stamp_ < target_stamp);
  return_unless (target_stamp - render_stamp_ <= AUDIO_BLOCK_MAX_RENDER_SIZE);
  if (ASE_UNLIKELY (estreams_) && !ASE_ISLIKELY (estreams_->estream.empty()))
    estreams_->estream.clear();
  render (target_stamp - render_stamp_);
  render_stamp_ = target_stamp;
}

// == Registry ==
struct AudioProcessorRegistry {
  CString           aseid;       ///< Unique identifier for de-/serialization.
  void            (*static_info) (AudioProcessorInfo&) = nullptr;
  AudioProcessorP (*make_shared) (AudioEngine&) = nullptr;
  const AudioProcessorRegistry* next = nullptr;
};

static std::atomic<const AudioProcessorRegistry*> registry_first;

/// Add a new type to the AudioProcessor type registry.
/// New AudioProcessor types must have a unique URI (see `query_info()`) and will be created
/// with the factory function `create()`.
void
AudioProcessor::registry_add (CString aseid, StaticInfo static_info, MakeProcessorP makeproc)
{
  assert_return (string_startswith (aseid, "Ase::"));
  AudioProcessorRegistry *entry = new AudioProcessorRegistry();
  entry->aseid = aseid;
  entry->static_info = static_info;
  entry->make_shared = makeproc;
  // push_front via compare_exchange (*obj, *expected, desired)
  while (!std::atomic_compare_exchange_strong (&registry_first, &entry->next, entry))
    ; // if (*obj==*expected) *obj=desired; else *expected=*obj;
}

DeviceP
AudioProcessor::registry_create (const String &aseid, AudioEngine &engine, const MakeDeviceP &makedevice)
{
  for (const AudioProcessorRegistry *entry = registry_first; entry; entry = entry->next)
    if (entry->aseid == aseid) {
      AudioProcessorP aproc = entry->make_shared (engine);
      if (aproc) {
        DeviceP devicep = makedevice (aseid, entry->static_info, aproc);
        aproc->ensure_initialized (devicep);
        return devicep;
      }
    }
  warning ("AudioProcessor::registry_create: failed to create processor: %s", aseid);
  return nullptr;
}

/// Iterate over the known AudioProcessor types.
void
AudioProcessor::registry_foreach (const std::function<void (const String &aseid, StaticInfo)> &fun)
{
  for (const AudioProcessorRegistry *entry = registry_first; entry; entry = entry->next)
    fun (entry->aseid, entry->static_info);
}

// == AudioProcessor::PParam ==
AudioProcessor::PParam::PParam (ParamId _id, uint order, ParameterC &parameterc) :
  id (_id), order_ (order), parameter (parameterc)
{
  assert_return (parameterc != nullptr);
}

AudioProcessor::PParam::PParam (ParamId _id) :
  id (_id)
{}

AudioProcessor::PParam::PParam (const PParam &src) :
  id (src.id)
{
  *this = src;
}

AudioProcessor::PParam&
AudioProcessor::PParam::operator= (const PParam &src)
{
  id = src.id;
  order_ = src.order_;
  flags_ = src.flags_.load();
  value_ = src.value_.load();
  aprop_ = src.aprop_;
  parameter = src.parameter;
  return *this;
}

// == FloatBuffer ==
/// Check for end-of-buffer overwrites
void
AudioProcessor::FloatBuffer::check ()
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
  // verify cache-line aligned struct layout
  static_assert (0 == (offsetof (FloatBuffer, fblock[0]) & 63));
  static_assert (0 == (offsetof (FloatBuffer, canary0_) & 63));
  static_assert (0 == ((sizeof (buffer) + offsetof (FloatBuffer, buffer)) & 63));
#pragma GCC diagnostic pop
  // verify cache-line aligned runtime layout
  assert_return (0 == (uintptr_t (&buffer[0]) & 63));
  // failing canaries indicate end-of-buffer overwrites
  assert_return (canary0_ == const_canary);
  assert_return (canary1_ == const_canary);
}

// == AudioPropertyImpl ==
class AudioPropertyImpl : public Property, public virtual EmittableImpl {
  DeviceP       device_;
  ParameterC    parameter_;
  const ParamId id_;
  double              inflight_value_ = 0;
  std::atomic<uint64> inflight_counter_ = 0;
public:
  String   ident          () override   { return parameter_->ident(); }
  String   label          () override   { return parameter_->label(); }
  String   nick           () override   { return parameter_->nick(); }
  String   unit           () override   { return parameter_->unit(); }
  String   hints          () override   { return parameter_->hints(); }
  String   group          () override   { return parameter_->group(); }
  String   blurb          () override   { return parameter_->blurb(); }
  String   descr          () override   { return parameter_->descr(); }
  double   get_min        () override   { const auto [fmin, fmax, step] = parameter_->range(); return fmin; }
  double   get_max        () override   { const auto [fmin, fmax, step] = parameter_->range(); return fmax; }
  double   get_step       () override   { const auto [fmin, fmax, step] = parameter_->range(); return step; }
  explicit
  AudioPropertyImpl (DeviceP devp, ParamId id, ParameterC parameter) :
    device_ (devp), parameter_ (parameter), id_ (id)
  {}
  void
  proc_paramchange()
  {
    const AudioProcessorP proc = device_->_audio_processor();
    const double value = inflight_counter_ ? inflight_value_ : AudioProcessor::param_peek_mt (proc, id_);
    ValueR vfields;
    vfields["value"] = value;
    emit_event ("notify", parameter_->ident(), vfields);
  }
  void
  reset () override
  {
    set_value (parameter_->initial());
  }
  Value
  get_value () override
  {
    const AudioProcessorP proc = device_->_audio_processor();
    const double value = inflight_counter_ ? inflight_value_ : AudioProcessor::param_peek_mt (proc, id_);
    const bool ischoice = strstr (parameter_->hints().c_str(), ":choice:") != nullptr;
    if (ischoice)
      return proc->param_value_to_text (id_, value);
    else
      return value;
  }
  bool
  set_value (const Value &value) override
  {
    PropertyP thisp = shared_ptr_cast<Property> (this); // thisp keeps this alive during lambda
    const AudioProcessorP proc = device_->_audio_processor();
    const bool ischoice = strstr (parameter_->hints().c_str(), ":choice:") != nullptr;
    double v;
    if (ischoice && value.index() == Value::STRING)
      v = proc->param_value_from_text (id_, value.as_string());
    else
      v = value.as_double();
    const ParamId pid = id_;
    inflight_value_ = v;
    inflight_counter_++;
    auto lambda = [proc, pid, v, thisp, this] () {
      proc->set_param (pid, v, false);
      inflight_counter_--;
    };
    proc->engine().async_jobs += lambda;
    emit_notify (parameter_->ident());
    return true;
  }
  double
  get_normalized () override
  {
    const AudioProcessorP proc = device_->_audio_processor();
    const double value = inflight_counter_ ? inflight_value_ : AudioProcessor::param_peek_mt (proc, id_);
    const double v = proc->value_to_normalized (id_, value);
    return v;
  }
  bool
  set_normalized (double normalized) override
  {
    const AudioProcessorP proc = device_->_audio_processor();
    const auto [fmin, fmax, step] = parameter_->range();
    const double value = fmin + CLAMP (normalized, 0, 1) * (fmax - fmin);
    return set_value (value);
  }
  String
  get_text () override
  {
    const AudioProcessorP proc = device_->_audio_processor();
    const double value = inflight_counter_ ? inflight_value_ : AudioProcessor::param_peek_mt (proc, id_);
    return proc->param_value_to_text (id_, value);
  }
  bool
  set_text (String vstr) override
  {
    const AudioProcessorP proc = device_->_audio_processor();
    const double v = proc->param_value_from_text (id_, vstr);
    const ParamId pid = id_;
    auto lambda = [proc, pid, v] () {
      proc->set_param (pid, v, false);
    };
    proc->engine().async_jobs += lambda;
    emit_notify (parameter_->ident());
    return true;
  }
  bool
  is_numeric () override
  {
    // TODO: we have yet to implement non-numeric AudioProcessor parameters
    return true;
  }
  ChoiceS
  choices () override
  {
    return parameter_->choices();
  }
};

// == AudioProcessor::access_properties ==
/// Retrieve/create Property handle from `id`.
/// This function is MT-Safe after proper AudioProcessor initialization.
PropertyP
AudioProcessor::access_property (ParamId id) const
{
  assert_return (is_initialized(), {});
  const PParam *pparam = find_pparam (id);
  assert_return (pparam, {});
  DeviceP devp = get_device();
  assert_return (devp, {});
  PropertyP newptr;
  PropertyP prop = weak_ptr_fetch_or_create<Property> (const_cast<PParam*> (pparam)->aprop_, [&] () {
      newptr = std::make_shared<AudioPropertyImpl> (devp, pparam->id, pparam->parameter);
      return newptr;
    });
  if (newptr.get() == prop.get())
    const_cast<AudioProcessor::PParam*> (pparam)->changed (false); // skip initial change notification
  return prop;
}

// == enotify_queue ==
static AudioProcessor        *const enotify_queue_tail = (AudioProcessor*) ptrdiff_t (-1);
static std::atomic<AudioProcessor*> enotify_queue_head { enotify_queue_tail };

/// Queue an AudioProcessor notification
/// This function is MT-Safe after proper AudioProcessor initialization.
void
AudioProcessor::enotify_enqueue_mt (uint32 pushmask)
{
  return_unless (!device_.expired());           // need a means to report notifications
  assert_return (enotify_queue_head != nullptr);     // paranoid
  const uint32 prev = flags_.fetch_or (pushmask & NOTIFYMASK);
  return_unless (prev != (prev | pushmask));    // nothing new
  AudioProcessor *expected = nullptr;
  if (nqueue_next_.compare_exchange_strong (expected, enotify_queue_tail))
    {
      // nqueue_next_ was nullptr, need to insert into queue now
      assert_warn (nqueue_guard_ == nullptr);
      nqueue_guard_ = shared_from_this();
      expected = enotify_queue_head.load(); // must never be nullptr
      do
        nqueue_next_.store (expected);
      while (!enotify_queue_head.compare_exchange_strong (expected, this));
    }
}

/// Dispatch all AudioProcessor notifications (Engine internal)
void
AudioProcessor::enotify_dispatch ()
{
  assert_return (this_thread_is_ase());
  AudioProcessor *head = enotify_queue_head.exchange (enotify_queue_tail);
  while (head != enotify_queue_tail)
    {
      AudioProcessor *current = std::exchange (head, head->nqueue_next_);
      AudioProcessorP procp = std::exchange (current->nqueue_guard_, nullptr);
      AudioProcessor *old_nqueue_next = current->nqueue_next_.exchange (nullptr);
      assert_warn (old_nqueue_next != nullptr);
      const uint32 nflags = NOTIFYMASK & current->flags_.fetch_and (~NOTIFYMASK);
      assert_warn (procp != nullptr);
      DeviceP devicep = current->get_device();
      if (devicep)
        {
          if (nflags & BUSCONNECT)
            devicep->emit_event ("bus", "connect");
          if (nflags & BUSDISCONNECT)
            devicep->emit_event ("bus", "disconnect");
          if (nflags & INSERTION)
            devicep->emit_event ("sub", "insert");
          if (nflags & REMOVAL)
            devicep->emit_event ("sub", "remove");
          if (nflags & PARAMCHANGE)
            for (PParam &pparam : current->pparams_)
              if (ASE_UNLIKELY (pparam.changed()) && pparam.changed (false))
                {
                  PropertyP propi = pparam.aprop_.lock();
                  AudioPropertyImpl *aprop = dynamic_cast<AudioPropertyImpl*> (propi.get());
                  if (aprop)
                    aprop->proc_paramchange();
                }
          if (nflags & PARAMCHANGE)
            devicep->emit_event ("params", "change");
        }
    }
}

/// Check for AudioProcessor notifications (Engine internal)
bool
AudioProcessor::enotify_pending ()
{
  return enotify_queue_head != enotify_queue_tail;
}

} // Ase
