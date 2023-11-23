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
AudioProcessor::IOBus::IOBus (IOTag io_tag, const String &ident, const String &uilabel, SpeakerArrangement sa) :
  BusInfo (ident, uilabel, "", "", sa), iotag (io_tag)
{
  switch (iotag)
    {
    case IBUS:
      assert_return (ibus == IBUS);
      obusid = {};
      oproc = nullptr;
      break;
    case OBUS:
      assert_return (obus == OBUS);
      fbuffer_concounter = 0;
      fbuffer_count = 0;
      fbuffer_index = ~0;
      break;
    }
  assert_return (ident != "");
}

// == AudioParams ==
AudioParams::~AudioParams()
{
  clear();
}

/// Clear all fields.
void
AudioParams::clear()
{
  changed = false;
  count = 0;
  delete[] ids;
  ids = nullptr;
  delete[] values;
  values = nullptr;
  delete[] bits;
  bits = nullptr;
  const ParameterC *old_parameters = nullptr;
  std::swap (old_parameters, parameters);
  std::weak_ptr<Property> *old_wprops = nullptr;
  std::swap (old_wprops, wprops);
  delete[] old_wprops;
  delete[] old_parameters; // call ~ParameterC *after* *this is cleaned up
}

/// Clear and install a new set of parameters.
void
AudioParams::install (const AudioParams::Map &params)
{
  assert_return (this_thread_is_ase());
  clear();
  count = params.size();
  if (count == 0)
    return;
  // ids
  uint32_t *new_ids = new uint32_t[count] ();
  size_t i = 0;
  for (auto [id,p] : params)
    new_ids[i++] = id;
  ids = new_ids;
  assert_return (i == count);
  // wprops
  wprops = new std::weak_ptr<Property>[count] ();
  // parameters
  ParameterC *new_parameters = new ParameterC[count] ();
  i = 0;
  for (auto [id,p] : params)
    new_parameters[i++] = p;
  parameters = new_parameters;
  assert_return (i == count);
  // values
  values = new double[count] ();                // value-initialized per ISO C++03 5.3.4[expr.new]/15
  for (size_t i = 0; i < count; i++)
    values[i] = parameters[i]->initial().as_double();
  // bits
  const size_t u = (count + 64-1) / 64;         // number of uint64_t bit fields
  bits = new std::atomic<uint64_t>[u] ();       // a bit array causes vastly fewer cache misses
}

// == AudioProcessor ==
const String AudioProcessor::GUIONLY = ":G:r:w:";     ///< GUI READABLE WRITABLE
const String AudioProcessor::STANDARD = ":G:S:r:w:";  ///< GUI STORAGE READABLE WRITABLE
const String AudioProcessor::STORAGEONLY = ":S:r:w:"; ///< STORAGE READABLE WRITABLE
uint64 __thread AudioProcessor::tls_timestamp = 0;

/// Constructor for AudioProcessor
AudioProcessor::AudioProcessor (const ProcessorSetup &psetup) :
  engine_ (psetup.engine), aseid_ (psetup.aseid)
{
  engine_.processor_count_ += 1;
}

/// The destructor is called when the last std::shared_ptr<> reference drops.
AudioProcessor::~AudioProcessor ()
{
  remove_all_buses();
  engine_.processor_count_ -= 1;
  delete atomic_bits_;
  MidiEventVector *t0events = nullptr;
  t0events = t0events_.exchange (t0events);
  delete t0events;
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
AudioProcessor::IOBus&
AudioProcessor::iobus (OBusId obusid)
{
  static const uint64 dummy_iobusmem[sizeof (AudioProcessor::IOBus)] = { 0, };
  auto &dummy_iobus = const_cast<IOBus&> (*reinterpret_cast<const IOBus*> (dummy_iobusmem));
  const size_t busindex = size_t (obusid) - 1;
  assert_return (busindex < n_obuses(), dummy_iobus);
  IOBus &bus = iobuses_[output_offset_ + busindex];
  assert_warn (bus.obus == IOBus::OBUS);
  return bus;
}

/// Get internal input bus handle.
AudioProcessor::IOBus&
AudioProcessor::iobus (IBusId ibusid)
{
  static const uint64 dummy_iobusmem[sizeof (AudioProcessor::IOBus)] = { 0, };
  auto &dummy_iobus = const_cast<IOBus&> (*reinterpret_cast<const IOBus*> (dummy_iobusmem));
  const size_t busindex = size_t (ibusid) - 1;
  assert_return (busindex < n_ibuses(), dummy_iobus);
  IOBus &bus = iobuses_[busindex];
  assert_warn (bus.ibus == IOBus::IBUS);
  return bus;
}

// Release buffers allocated for input/output channels.
void
AudioProcessor::release_iobufs()
{
  disconnect_ibuses();
  disconnect_obuses();
  for (OBusId ob = OBusId (1); size_t (ob) <= n_obuses(); ob = OBusId (size_t (ob) + 1))
    {
      IOBus &bus = iobus (ob);
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
      IOBus &bus = iobus (ob);
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

/// Reset list of parameters, enqueues parameter value initializaiton events.
void
AudioProcessor::install_params (const AudioParams::Map &params)
{
  assert_return (this_thread_is_ase());
  params_.install (params);
  modify_t0events ([&] (std::vector<MidiEvent> &t0events) {
    for (size_t i = 0; i < params_.count; i++)
      t0events.push_back (make_param_value (params_.ids[i], params_.parameters[i]->initial().as_double()));
  });
}

/// Return the ParamId for parameter `identifier` or else 0.
auto
AudioProcessor::find_param (const String &identifier) const -> MaybeParamId
{
  auto ident = CString::lookup (identifier);
  if (ident.empty())
    return { ParamId (0), false };
  for (size_t idx = 0; idx < params_.count; idx++)
    if (params_.parameters[idx]->cident == ident)
      return { ParamId (params_.ids[idx]), true };
  return std::make_pair (ParamId (0), false);
}

/// Set parameter `id` to `value` within `ParamInfo.get_minmax()`.
bool
AudioProcessor::send_param (Id32 paramid, const double value)
{
  assert_return (this_thread_is_ase(), false); // main_loop thread
  const ssize_t idx = params_.index (paramid.id);
  if (idx < 0) [[unlikely]]
    return false;
  ParameterC parameter = params_.parameters[idx];
  double v = value;
  if (parameter)
    v = parameter->dconstrain (value);
  modify_t0events ([&] (std::vector<MidiEvent> &t0events) {
    for (auto &ev : t0events)
      if (ev.type == MidiEvent::PARAM_VALUE &&
          ev.param == params_.ids[idx]) {
        ev.pvalue = v;
        return; // re-assigned previous send_param event
      }
    t0events.push_back (make_param_value (params_.ids[idx], v));
  });
  return true;
}

/// Retrieve supplemental information for parameters, usually to enhance the user interface.
ParameterC
AudioProcessor::parameter (Id32 paramid) const
{
  const ssize_t idx = params_.index (paramid.id);
  return idx < 0 ? nullptr : params_.parameters[idx];
}

/// Fetch the current parameter value of an AudioProcessor.
/// This function does not modify the parameter `dirty` flag.
/// This function is MT-Safe after proper AudioProcessor initialization.
double
AudioProcessor::peek_param_mt (Id32 paramid) const
{
  const ssize_t idx = params_.index (paramid.id);
  if (idx < 0) [[unlikely]]
    return false;
  return params_.values[idx];
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
  ParameterC parameterp = parameter (paramid);
  assert_return (parameterp != nullptr, 0);
  const auto [fmin, fmax, step] = parameterp->range();
  const double normalized = (value - fmin) / (fmax - fmin);
  assert_return (normalized >= 0.0 && normalized <= 1.0, CLAMP (normalized, 0.0, 1.0));
  return normalized;
}

double
AudioProcessor::value_from_normalized (Id32 paramid, double normalized) const
{
  ParameterC parameterp = parameter (paramid);
  assert_return (parameterp != nullptr, 0);
  const auto [fmin, fmax, step] = parameterp->range();
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
AudioProcessor::set_normalized (Id32 paramid, double normalized)
{
  if (!ASE_ISLIKELY (normalized >= 0.0))
    normalized = 0;
  else if (!ASE_ISLIKELY (normalized <= 1.0))
    normalized = 1.0;
  return send_param (paramid, value_from_normalized (paramid, normalized));
}

/** Format a parameter `paramid` value as text string.
 * Currently, this function may be called from any thread,
 * so `this` must be treated as `const` (it might be used
 * concurrently by a different thread).
 */
String
AudioProcessor::param_value_to_text (uint32_t paramid, double value) const
{
  ParameterC parameterp = parameter (paramid);
  return parameterp ? parameterp->value_to_text (value) : "";
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
AudioProcessor::param_value_from_text (uint32_t paramid, const String &text) const
{
  ParameterC parameterp = parameter (paramid);
  return parameterp ? parameterp->value_from_text (text).as_double() : 0.0;
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
  IOBus bus { IOBus::IBUS, string_to_identifier (uilabel), uilabel, speakerarrangement };
  bus.hints = hints;
  bus.blurb = blurb;
  iobuses_.insert (iobuses_.begin() + output_offset_, bus);
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
  IOBus bus { IOBus::OBUS, string_to_identifier (uilabel), uilabel, speakerarrangement };
  bus.hints = hints;
  bus.blurb = blurb;
  iobuses_.push_back (bus);
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
  const IOBus &ibus = iobus (busid);
  if (ibus.oproc)
    {
      const AudioProcessor &oproc = *ibus.oproc;
      const IOBus &obus = oproc.iobus (ibus.obusid);
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
AudioProcessor::float_buffer (OBusId obusid, uint channelindex, bool resetptr)
{
  const size_t obusindex = size_t (obusid) - 1;
  FloatBuffer *const fallback = const_cast<FloatBuffer*> (&zero_buffer());
  assert_return (obusindex < n_obuses(), *fallback);
  const IOBus &obus = iobus (obusid);
  assert_return (channelindex < obus.fbuffer_count, *fallback);
  FloatBuffer &fbuffer = fbuffers_[obus.fbuffer_index + channelindex];
  if (resetptr)
    fbuffer.buffer = &fbuffer.fblock[0];
  return fbuffer;
}

/// Redirect output buffer of bus `b`, channel `c` to point to `block`, or zeros if `block==nullptr`.
void
AudioProcessor::redirect_oblock (OBusId obusid, uint channelindex, const float *block)
{
  const size_t obusindex = size_t (obusid) - 1;
  assert_return (obusindex < n_obuses());
  const IOBus &obus = iobus (obusid);
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
  IOBus &ibus = iobus (ibusid);
  // disconnect
  return_unless (ibus.oproc != nullptr);
  AudioProcessor &oproc = *ibus.oproc;
  const size_t obusindex = size_t (ibus.obusid) - 1;
  assert_return (obusindex < oproc.n_obuses());
  IOBus &obus = oproc.iobus (ibus.obusid);
  assert_return (obus.fbuffer_concounter > 0);
  obus.fbuffer_concounter -= 1; // connection counter
  const bool backlink = Aux::erase_first (oproc.outputs_, [&] (auto &e) { return e.proc == this && e.ibusid == ibusid; });
  ibus.oproc = nullptr;
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
  IOBus &ibus = iobus (ibusid);
  const uint n_ichannels = ibus.n_channels();
  IOBus &obus = oproc.iobus (obusid);
  const uint n_ochannels = obus.n_channels();
  // match channels
  assert_return (n_ichannels <= n_ochannels ||
                 (ibus.speakers == SpeakerArrangement::STEREO &&
                  obus.speakers == SpeakerArrangement::MONO));
  // connect
  ibus.oproc = &oproc;
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
      initialize (engine_.speaker_arrangement());
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
        estreams_->midi_event_output.clear();
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
      IOBus &ibus = iobus (IBusId (1 + i));
      if (ibus.oproc)
        {
          const uint l = schedule_processor (*ibus.oproc);
          level = std::max (level, l);
        }
    }
  const uint l = schedule_children();
  level = std::max (level, l);
  engine_.schedule_add (*this, level);
  return level + 1;
}

struct AudioProcessor::RenderContext {
  MidiEventVector *render_events = nullptr;
};

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
  RenderContext rc;
  if (ASE_UNLIKELY (estreams_))
    estreams_->midi_event_output.clear();
  rc.render_events = t0events_.exchange (rc.render_events); // fetch t0events_ for rendering
  render_context_ = &rc;
  render (target_stamp - render_stamp_);
  render_context_ = nullptr;
  render_stamp_ = target_stamp;
  if (rc.render_events) // delete in main_thread
    main_rt_jobs += RtCall (call_delete<MidiEventVector>, rc.render_events);
  if (params_.changed) {
    params_.changed = false;
    enotify_enqueue_mt (PARAMCHANGE);
  }
}

/// Access the current MidiEvent inputs during render(), needs prepare_event_input().
AudioProcessor::MidiEventInput
AudioProcessor::midi_event_input()
{
  MidiEventInput::VectorArray mev_array{};
  size_t n = 0;
  if (estreams_ && estreams_->oproc && estreams_->oproc->estreams_)
    mev_array[n++] = &estreams_->oproc->estreams_->midi_event_output.vector();
  if (render_context_->render_events)
    mev_array[n++] = render_context_->render_events;
  return MidiEventInput (mev_array);
}

static const MidiEventOutput empty_event_output; // dummy

/// Access the current output EventStream during render(), needs prepare_event_output().
MidiEventOutput&
AudioProcessor::midi_event_output()
{
  assert_return (estreams_ != nullptr, const_cast<MidiEventOutput&> (empty_event_output));
  return estreams_->midi_event_output;
}

// == Registry ==
struct AudioProcessorRegistry {
  CString        aseid;       ///< Unique identifier for de-/serialization.
  void         (*static_info) (AudioProcessorInfo&) = nullptr;
  AudioProcessor::MakeProcessorP make_shared = nullptr;
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
AudioProcessor::registry_create (CString aseid, AudioEngine &engine, const MakeDeviceP &makedevice)
{
  for (const AudioProcessorRegistry *entry = registry_first; entry; entry = entry->next)
    if (entry->aseid == aseid) {
      AudioProcessorP aproc = entry->make_shared (aseid, engine);
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
  DeviceP        device_;
  ParameterC     parameter_;
  const uint32_t id_;
  double         inflight_value_ = 0;
  uint64_t       inflight_stamp_ = 0;
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
  AudioPropertyImpl (DeviceP devp, uint32_t id, ParameterC parameter) :
    device_ (devp), parameter_ (parameter), id_ (id)
  {}
  void
  proc_paramchange()
  {
    const AudioProcessorP proc = device_->_audio_processor();
    const double value = inflight_stamp_ >  proc->engine().frame_counter() ? inflight_value_ : AudioProcessor::param_peek_mt (proc, id_);
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
    const double value = inflight_stamp_ >  proc->engine().frame_counter() ? inflight_value_ : AudioProcessor::param_peek_mt (proc, id_);
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
    double v;
    if (value.index() == Value::STRING && parameter_->is_choice())
      v = proc->param_value_from_text (id_, value.as_string());
    else
      v = value.as_double();
    proc->send_param (id_, v);
    inflight_value_ = v;
    inflight_stamp_ = proc->engine().frame_counter();
    inflight_stamp_ += 2 * proc->engine().block_size(); // wait until after the *next* job queue has been processed
    emit_notify (parameter_->ident());
    return true;
  }
  double
  get_normalized () override
  {
    const AudioProcessorP proc = device_->_audio_processor();
    const double value = inflight_stamp_ >  proc->engine().frame_counter() ? inflight_value_ : AudioProcessor::param_peek_mt (proc, id_);
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
    const double value = inflight_stamp_ >  proc->engine().frame_counter() ? inflight_value_ : AudioProcessor::param_peek_mt (proc, id_);
    return proc->param_value_to_text (id_, value);
  }
  bool
  set_text (String vstr) override
  {
    const AudioProcessorP proc = device_->_audio_processor();
    const double v = proc->param_value_from_text (id_, vstr);
    return set_value (v);
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
/// Retrieve (or create) Property handles for all properties.
/// This function must be called from the main-thread.
PropertyS
AudioProcessor::access_properties () const
{
  DeviceP devp = get_device();
  assert_return (devp, {});
  PropertyS props;
  props.reserve (params_.count);
  for (size_t idx = 0; idx < params_.count; idx++) {
    const uint32_t id = params_.ids[idx];
    ParameterC parameterp = params_.parameters[idx];
    assert_return (parameterp != nullptr, {});
    PropertyP prop = weak_ptr_fetch_or_create<Property> (params_.wprops[idx], [&] () {
      return std::make_shared<AudioPropertyImpl> (devp, id, parameterp);
    });
    props.push_back (prop);
  }
  return props;
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
            for (size_t blockoffset = 0; blockoffset < current->params_.count; blockoffset += 64)
              if (current->params_.bits[blockoffset >> 6]) {
                const uint64_t bitmask = std::atomic_fetch_and (&current->params_.bits[blockoffset >> 6], 0);
                const size_t bound = std::min (uint64_t (current->params_.count), blockoffset | uint64_t (64-1));
                for (size_t idx = blockoffset; idx < bound; idx++)
                  if (bitmask & uint64_t (1) << (idx & (64-1))) {
                    PropertyP propi = current->params_.wprops[idx].lock();
                    AudioPropertyImpl *aprop = dynamic_cast<AudioPropertyImpl*> (propi.get());
                    if (aprop)
                      aprop->proc_paramchange();
                  }
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
