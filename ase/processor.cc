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

// == Helpers ==
static std::string
canonify_identifier (const std::string &input)
{
  static const std::string validset = Ase::string_set_a2z() + "0123456789" + "_";
  const std::string lowered = Ase::string_tolower (input);
  return Ase::string_canonify (lowered, validset, "_");
}

namespace Ase {

// == ChoiceDetails ==
ChoiceDetails::ChoiceDetails (CString label_, CString subject_) :
  ident (canonify_identifier (label_)), label (label_), subject (subject_)
{
  assert_return (ident.empty() == false);
}

ChoiceDetails::ChoiceDetails (IconStr icon_, CString label_, CString subject_) :
  ident (canonify_identifier (label_)), label (label_), subject (subject_), icon (icon_)
{
  assert_return (ident.empty() == false);
}

// == ChoiceEntries ==
ChoiceEntries&
ChoiceEntries::operator+= (const ChoiceDetails &ce)
{
  push_back (ce);
  return *this;
}

// == ParamInfo ==
static constexpr uint PTAG_FLOATS = 1;
static constexpr uint PTAG_CENTRIES = 2;

ParamInfo::ParamInfo (ParamId pid, uint porder) :
  order (porder), union_tag (0)
{
  memset (&u, 0, sizeof (u));
}

ParamInfo::~ParamInfo()
{
  release();
}

void
ParamInfo::copy_fields (const ParamInfo &src)
{
  ident = src.ident;
  label = src.label;
  nick = src.nick;
  unit = src.unit;
  hints = src.hints;
  group = src.group;
  blurb = src.blurb;
  description = src.description;
  switch (src.union_tag)
    {
    case PTAG_FLOATS:
      set_range (src.u.fmin, src.u.fmax, src.u.fstep);
      break;
    case PTAG_CENTRIES:
      set_choices (*src.u.centries());
      break;
    }
}

void
ParamInfo::release()
{
  const bool destroy = union_tag == PTAG_CENTRIES;
  union_tag = 0;
  if (destroy)
    u.centries()->~ChoiceEntries();
}

/// Clear all ParamInfo fields.
void
ParamInfo::clear ()
{
  ident = "";
  label = "";
  nick = "";
  unit = "";
  hints = "";
  group = "";
  blurb = "";
  description = "";
  release();
}

/// Get parameter stepping or 0 if not quantized.
double
ParamInfo::get_stepping () const
{
  switch (union_tag)
    {
    case PTAG_FLOATS:
      return u.fstep;
    case PTAG_CENTRIES:
      return 1;
    default:
      return 0;
    }
}

/// Get initial parameter value.
double
ParamInfo::get_initial () const
{
  return initial_;
}

/// Get parameter range minimum and maximum.
ParamInfo::MinMax
ParamInfo::get_minmax () const
{
  switch (union_tag)
    {
    case PTAG_FLOATS:
      return { u.fmin, u.fmax };
    case PTAG_CENTRIES:
      return { 0, u.centries()->size() - 1 };
    default:
      return { NAN, NAN };
    }
}

/// Get parameter range properties.
void
ParamInfo::get_range (double &fmin, double &fmax, double &fstep) const
{
  switch (union_tag)
    {
    case PTAG_FLOATS:
      fmin = u.fmin;
      fmax = u.fmax;
      fstep = u.fstep;
      break;
    case PTAG_CENTRIES:
      {
        auto mm = get_minmax ();
        fmin = mm.first;
        fmax = mm.second;
      }
      fstep = 1;
      break;
    default:
      fmin = NAN;
      fmax = NAN;
      fstep = NAN;
      break;
    }
}

/// Assign range properties to parameter.
void
ParamInfo::set_range (double fmin, double fmax, double fstep)
{
  release();
  union_tag = PTAG_FLOATS;
  u.fmin = fmin;
  u.fmax = fmax;
  u.fstep = fstep;
}

/// Get parameter choice list.
const ChoiceEntries&
ParamInfo::get_choices () const
{
  if (union_tag == PTAG_CENTRIES)
    return *u.centries();
  static const ChoiceEntries empty;
  return empty;
}

/// Assign choice list to parameter via vector move.
void
ParamInfo::set_choices (ChoiceEntries &&centries)
{
  release();
  union_tag = PTAG_CENTRIES;
  new (u.centries()) ChoiceEntries (std::move (centries));
}

/// Assign choice list to parameter via deep copy.
void
ParamInfo::set_choices (const ChoiceEntries &centries)
{
  set_choices (ChoiceEntries (centries));
}

// == PBus ==
AudioProcessor::PBus::PBus (const std::string &ident, const std::string &uilabel, SpeakerArrangement sa) :
  ibus (ident, uilabel, sa)
{}

AudioProcessor::IBus::IBus (const std::string &identifier, const std::string &uilabel, SpeakerArrangement sa)
{
  ident = identifier;
  label = uilabel;
  speakers = sa;
  proc = nullptr;
  obusid = {};
  assert_return (ident != "");
}

AudioProcessor::OBus::OBus (const std::string &identifier, const std::string &uilabel, SpeakerArrangement sa)
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
const std::string AudioProcessor::STANDARD = ":G:S:r:w:"; ///< GUI STORAGE READABLE WRITABLE
uint64 __thread AudioProcessor::tls_timestamp = 0;
struct ProcessorRegistryContext {
  AudioEngine *engine = nullptr;
};
static __thread ProcessorRegistryContext *processor_ctor_registry_context = nullptr;

/// Constructor for AudioProcessor
AudioProcessor::AudioProcessor() :
  engine_ (*processor_ctor_registry_context->engine)
{
  assert_return (processor_ctor_registry_context->engine != nullptr);
  processor_ctor_registry_context->engine = nullptr; // consumed
  engine_.processor_count_ += 1;
}

/// The destructor is called when the last std::shared_ptr<> reference drops.
AudioProcessor::~AudioProcessor ()
{
  remove_all_buses();
  engine_.processor_count_ -= 1;
}

/// Convert MIDI note to Hertz according to the current MusicalTuning.
float
AudioProcessor::note_to_freq (int note) const
{
  return MidiNote::note_to_freq (MusicalTuning::OD_12_TET, note, 440);
}

/// Create the `Ase::ProcessorIface` for `this`.
DeviceImplP
AudioProcessor::device_impl () const
{
  assert_return (is_initialized(), {});
  return DeviceImpl::make_shared (*const_cast<AudioProcessor*> (this));
}

/// Gain access to `this` through the `Ase::ProcessorIface` interface.
DeviceImplP
AudioProcessor::get_device (bool create) const
{
  std::weak_ptr<DeviceImpl> &wptr = const_cast<AudioProcessor*> (this)->device_;
  DeviceImplP devicep = wptr.lock();
  return_unless (!devicep && create, devicep);
  DeviceImplP nprocp = device_impl();
  assert_return (nprocp != nullptr, nullptr);
  { // TODO: C++20 has: std::atomic<std::weak_ptr<C>>::compare_exchange
    static std::mutex mutex;
    std::lock_guard<std::mutex> locker (mutex);
    devicep = wptr.lock();
    if (!devicep)
      wptr = devicep = nprocp;
  }
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
AudioProcessor::start_group (const std::string &groupname) const
{
  tls_param_group = groupname;
}

static CString
construct_hints (std::string hints, double pmin, double pmax, const std::string &more = "")
{
  if (hints.empty())
    hints = AudioProcessor::STANDARD;
  if (hints.back() != ':')
    hints = hints + ":";
  for (const auto &s : string_split (more))
    if (!s.empty() && "" == feature_toggle_find (hints, s, ""))
      hints += s + ":";
  if (hints[0] != ':')
    hints = ":" + hints;
  if (pmax > 0 && pmax == -pmin)
    hints += "bidir:";
  return hints;
}

/// Helper for add_param() to generate the sequentially next ParamId.
ParamId
AudioProcessor::nextid () const
{
  const uint pmax = params_.size();
  const uint last = params_.empty() ? 0 : uint (params_.back().id);
  return ParamId (MAX (pmax, last) + 1);
}

/// Add a new parameter with unique `ParamInfo.identifier`.
/// The returned `ParamId` is forced to match `id` (and must be unique).
ParamId
AudioProcessor::add_param (Id32 id, const ParamInfo &infotmpl, double value)
{
  assert_return (uint (id) > 0, ParamId (0));
  assert_return (!is_initialized(), {});
  assert_return (infotmpl.label != "", {});
  if (params_.size())
    assert_return (infotmpl.label != params_.back().info->label, {}); // easy CnP error
  PParam param { ParamId (id.id), uint (1 + params_.size()), infotmpl };
  if (param.info->ident == "")
    param.info->ident = canonify_identifier (param.info->label);
  if (params_.size())
    assert_return (param.info->ident != params_.back().info->ident, {}); // easy CnP error
  if (param.info->group.empty())
    param.info->group = tls_param_group;
  using P = decltype (params_);
  std::pair<P::iterator, bool> existing_parameter_position =
    Aux::binary_lookup_insertion_pos (params_.begin(), params_.end(), PParam::cmp, param);
  assert_return (existing_parameter_position.second == false, {});
  params_.insert (existing_parameter_position.first, std::move (param));
  set_param (param.id, value); // forces dirty
  param.info->initial_ = peek_param_mt (param.id);
  return param.id;
}

/// Add new range parameter with most `ParamInfo` fields as inlined arguments.
/// The returned `ParamId` is newly assigned and increases with the number of parameters added.
/// The `clabel` is the canonical, non-translated name for this parameter, its
/// hyphenated lower case version is used for serialization.
ParamId
AudioProcessor::add_param (Id32 id, const std::string &clabel, const std::string &nickname,
                           double pmin, double pmax, double value,
                           const std::string &unit, std::string hints,
                           const std::string &blurb, const std::string &description)
{
  assert_return (uint (id) > 0, ParamId (0));
  ParamInfo info;
  info.ident = canonify_identifier (clabel);
  info.label = clabel;
  info.nick = nickname;
  info.hints = construct_hints (hints, pmin, pmax);
  info.unit = unit;
  info.blurb = blurb;
  info.description = description;
  info.set_range (pmin, pmax);
  return add_param (id, info, value);
}

/// Variant of AudioProcessor::add_param() with sequential `id` generation.
ParamId
AudioProcessor::add_param (const std::string &clabel, const std::string &nickname,
                           double pmin, double pmax, double value,
                           const std::string &unit, std::string hints,
                           const std::string &blurb, const std::string &description)
{
  return add_param (nextid(), clabel, nickname, pmin, pmax, value, unit, hints, blurb, description);
}

/// Add new choice parameter with most `ParamInfo` fields as inlined arguments.
/// The returned `ParamId` is newly assigned and increases with the number of parameters added.
/// The `clabel` is the canonical, non-translated name for this parameter, its
/// hyphenated lower case version is used for serialization.
ParamId
AudioProcessor::add_param (Id32 id, const std::string &clabel, const std::string &nickname,
                           ChoiceEntries &&centries, double value, std::string hints,
                           const std::string &blurb, const std::string &description)
{
  assert_return (uint (id) > 0, ParamId (0));
  ParamInfo info;
  info.ident = canonify_identifier (clabel);
  info.label = clabel;
  info.nick = nickname;
  info.blurb = blurb;
  info.description = description;
  const double pmax = centries.size();
  info.set_choices (std::move (centries));
  info.hints = construct_hints (hints, 0, pmax, "choice");
  return add_param (id, info, value);
}

/// Variant of AudioProcessor::add_param() with sequential `id` generation.
ParamId
AudioProcessor::add_param (const std::string &clabel, const std::string &nickname,
                           ChoiceEntries &&centries, double value, std::string hints,
                           const std::string &blurb, const std::string &description)
{
  return add_param (nextid(), clabel, nickname, std::forward<ChoiceEntries> (centries), value, hints, blurb, description);
}

/// Add new toggle parameter with most `ParamInfo` fields as inlined arguments.
/// The returned `ParamId` is newly assigned and increases with the number of parameters added.
/// The `clabel` is the canonical, non-translated name for this parameter, its
/// hyphenated lower case version is used for serialization.
ParamId
AudioProcessor::add_param (Id32 id, const std::string &clabel, const std::string &nickname,
                           bool boolvalue, std::string hints,
                           const std::string &blurb, const std::string &description)
{
  assert_return (uint (id) > 0, ParamId (0));
  ParamInfo info;
  info.ident = canonify_identifier (clabel);
  info.label = clabel;
  info.nick = nickname;
  info.blurb = blurb;
  info.description = description;
  static const ChoiceEntries centries { { "Off" }, { "On" } };
  info.set_choices (centries);
  info.hints = construct_hints (hints, false, true, "toggle");
  const auto rid = add_param (id, info, boolvalue);
  assert_return (uint (rid) == id.id, rid);
  const PParam *param = find_pparam (rid);
  assert_return (param && param->peek() == (boolvalue ? 1.0 : 0.0), rid);
  return rid;
}

/// Variant of AudioProcessor::add_param() with sequential `id` generation.
ParamId
AudioProcessor::add_param (const std::string &clabel, const std::string &nickname,
                           bool boolvalue, std::string hints,
                           const std::string &blurb, const std::string &description)
{
  return add_param (nextid(), clabel, nickname, boolvalue, hints, blurb, description);
}

/// Return the ParamId for parameter `identifier` or else 0.
auto
AudioProcessor::find_param (const String &identifier) const -> MaybeParamId
{
  auto ident = CString::lookup (identifier);
  if (!ident.empty())
    for (const PParam &p : params_)
      if (p.info->ident == ident)
        return std::make_pair (p.id, true);
  return std::make_pair (ParamId (0), false);
}

// Non-fastpath implementation of find_param().
const AudioProcessor::PParam*
AudioProcessor::find_pparam_ (ParamId paramid) const
{
  // lookup id with gaps
  const PParam key { paramid };
  auto iter = Aux::binary_lookup (params_.begin(), params_.end(), PParam::cmp, key);
  const bool found_paramid = iter != params_.end();
  if (ISLIKELY (found_paramid))
    return &*iter;
  assert_return (found_paramid == true, nullptr);
  return nullptr;
}

/// Set parameter `id` to `value` within `ParamInfo.get_minmax()`.
void
AudioProcessor::set_param (Id32 paramid, const double value)
{
  const PParam *pparam = find_pparam (ParamId (paramid.id));
  return_unless (pparam);
  const ParamInfo *info = pparam->info.get();
  double v = value;
  if (info)
    {
      const auto mm = info->get_minmax();
      v = CLAMP (v, mm.first, mm.second);
      const double stepping = info->get_stepping();
      if (stepping > 0)
        {
          // round halfway cases down, so:
          // 0 -> -0.5…+0.5 yields -0.5
          // 1 -> -0.5…+0.5 yields +0.5
          constexpr const auto nearintoffset = 0.5 - DOUBLE_EPSILON; // round halfway case down
          v = stepping * uint64_t ((v - mm.first) / stepping + nearintoffset);
          v = CLAMP (mm.first + v, mm.first, mm.second);
        }
    }
  if (const_cast<PParam*> (pparam)->assign (v) &&
      !pparam->info->aprop_.expired())
    enotify_enqueue_mt (PARAMCHANGE);
}

/// Retrieve supplemental information for parameters, usually to enhance the user interface.
ParamInfoP
AudioProcessor::param_info (Id32 paramid) const
{
  const PParam *param = this->find_pparam (ParamId (paramid.id));
  return ASE_ISLIKELY (param) ? param->info : nullptr;
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
  const PParam *const param = find_pparam (paramid);
  assert_return (param != nullptr, 0);
  const auto mm = param->info->get_minmax();
  const double normalized = (value - mm.first) / (mm.second - mm.first);
  assert_return (normalized >= 0.0 && normalized <= 1.0, CLAMP (normalized, 0.0, 1.0));
  return normalized;
}

double
AudioProcessor::value_from_normalized (Id32 paramid, double normalized) const
{
  const PParam *const param = find_pparam (paramid);
  assert_return (param != nullptr, 0);
  const auto mm = param->info->get_minmax();
  const double value = mm.first + normalized * (mm.second - mm.first);
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
void
AudioProcessor::set_normalized (Id32 paramid, double normalized)
{
  if (!ASE_ISLIKELY (normalized >= 0.0))
    normalized = 0;
  else if (!ASE_ISLIKELY (normalized <= 1.0))
    normalized = 1.0;
  set_param (paramid, value_from_normalized (paramid, normalized));
}

/** Format a parameter `paramid` value as text string.
 * Currently, this function may be called from any thread,
 * so `this` must be treated as `const` (it might be used
 * concurrently by a different thread).
 */
std::string
AudioProcessor::param_value_to_text (Id32 paramid, double value) const
{
  const PParam *pparam = find_pparam (ParamId (paramid.id));
  if (!pparam || !pparam->info)
    return "";
  const ParamInfo &info = *pparam->info;
  std::string unit = pparam->info->unit;
  int fdigits = 2;
  if (unit == "Hz" && fabs (value) >= 1000)
    {
      unit = "kHz";
      value /= 1000;
    }
  if (fabs (value) < 10)
    fdigits = 2;
  else if (fabs (value) < 100)
    fdigits = 1;
  else if (fabs (value) < 1000)
    fdigits = 0;
  else
    fdigits = 0;
  const bool need_sign = info.get_minmax().first < 0;
  std::string s = need_sign ? string_format ("%+.*f", fdigits, value) : string_format ("%.*f", fdigits, value);
  if (!unit.empty())
    s += " " + unit;
  return s;
}

/** Extract a parameter `paramid` value from a text string.
 * The string might contain unit information or consist only of
 * number characters. Non-recognized characters should be ignored,
 * so a best effort conversion is always undertaken.
 * Currently, this function may be called from any thread,
 * so `this` must be treated as `const` (it might be used
 * concurrently by a different thread).
 */
double
AudioProcessor::param_value_from_text (Id32 paramid, const std::string &text) const
{
  return string_to_double (text);
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
  ParamInfo *info = param_info (paramid).get();
  return info ? info->get_minmax() : MinMax { FP_NAN, FP_NAN };
}

String
AudioProcessor::debug_name () const
{
  AudioProcessorInfo info;
  const_cast<AudioProcessor*> (this)->query_info (info);
  return info.label.empty() ? info.uri : info.label;
}

/// Mandatory method that provides unique URI, display label and registration information.
/// Depending on the host, this method may be called often or only once per subclass type.
/// The field `uri` must be a globally unique URI (or UUID), that is can be used as unique
/// identifier for reliable (de-)serialization.
void
AudioProcessor::query_info (AudioProcessorInfo &info) const
{}

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
                               const std::string &hints, const std::string &blurb)
{
  assert_return (!is_initialized(), {});
  assert_return (uilabel != "", {});
  assert_return (uint64_t (speaker_arrangement_channels (speakerarrangement)) > 0, {});
  assert_return (iobuses_.size() < 65535, {});
  if (n_ibuses())
    assert_return (uilabel != iobus (IBusId (n_ibuses())).label, {}); // easy CnP error
  PBus pbus { canonify_identifier (uilabel), uilabel, speakerarrangement };
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
                                const std::string &hints, const std::string &blurb)
{
  assert_return (!is_initialized(), {});
  assert_return (uilabel != "", {});
  assert_return (uint64_t (speaker_arrangement_channels (speakerarrangement)) > 0, {});
  assert_return (iobuses_.size() < 65535, {});
  if (n_obuses())
    assert_return (uilabel != iobus (OBusId (n_obuses())).label, {}); // easy CnP error
  PBus pbus { canonify_identifier (uilabel), uilabel, speakerarrangement };
  pbus.pbus.hints = hints;
  pbus.pbus.blurb = blurb;
  iobuses_.push_back (pbus);
  const OBusId id = OBusId (n_obuses());
  return id; // 1 + index
}

/// Return the IBusId for input bus `uilabel` or else 0.
IBusId
AudioProcessor::find_ibus (const std::string &uilabel) const
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
AudioProcessor::find_obus (const std::string &uilabel) const
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

/// Redirect output buffer of bus `b`, channel `c` to point to `block`.
void
AudioProcessor::redirect_oblock (OBusId busid, uint channelindex, const float *block)
{
  const size_t obusindex = size_t (busid) - 1;
  assert_return (obusindex < n_obuses());
  const OBus &obus = iobus (busid);
  assert_return (channelindex < obus.fbuffer_count);
  FloatBuffer &fbuffer = fbuffers_[obus.fbuffer_index + channelindex];
  assert_return (block != nullptr);
  fbuffer.buffer = const_cast<float*> (block);
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
AudioProcessor::ensure_initialized()
{
  if (!is_initialized())
    {
      assert_return (n_ibuses() + n_obuses() == 0);
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
  if (ASE_UNLIKELY (estreams_) && !ASE_ISLIKELY (estreams_->estream.empty()))
    estreams_->estream.clear();
  render (target_stamp - render_stamp_);
  render_stamp_ = target_stamp;
}

// == RegistryEntry ==
struct RegistryId::Entry : AudioProcessorInfo {
  Entry *next = nullptr;
  const AudioProcessor::MakeProcessor create = nullptr;
  const CString file = "";
  const int line = 0;
  Entry (Entry *hnext, const AudioProcessor::MakeProcessor &make, CString f, int l) :
    next (hnext), create (make), file (f), line (l)
  {}
};

static std::atomic<RegistryId::Entry*> processor_registry_entries = nullptr;
using RegistryTable = std::unordered_map<CString, RegistryId::Entry*>;
static std::recursive_mutex processor_registry_mutex;
static Persistent<RegistryTable> processor_registry_table;
static const RegistryId::Entry dummy_registry_entry { 0, 0, "", 0 };

/// Add a new type to the AudioProcessor type registry.
/// New AudioProcessor types must have a unique URI (see `query_info()`) and will be created
/// with the factory function `create()`.
RegistryId
AudioProcessor::registry_enroll (MakeProcessor create, const char *bfile, int bline)
{
  assert_return (create != nullptr, { dummy_registry_entry });
  auto *entry = new RegistryId::Entry { nullptr, create, bfile ? bfile : "", bline };
  // push_front
  while (!std::atomic_compare_exchange_strong (&processor_registry_entries,
                                               &entry->next,
                                               entry))
    ; // when failing, compare_exchange automatically does *&arg2 = *&arg1
  return { *entry };
}

// Ensure all registration entries have been examined
void
AudioProcessor::registry_init()
{
  static AudioEngine &regengine = make_audio_engine (48000, SpeakerArrangement::STEREO);
  while (processor_registry_entries)
    {
      std::lock_guard<std::recursive_mutex> rlocker (processor_registry_mutex);
      RegistryId::Entry *entry = nullptr;
      // pop_all
      while (!std::atomic_compare_exchange_strong (&processor_registry_entries, &entry, (RegistryId::Entry*) nullptr))
        ; // when failing, compare_exchange automatically does *&arg2 = *&arg1
      // register all
      while (entry)
        {
          ProcessorRegistryContext *const saved = processor_ctor_registry_context;
          ProcessorRegistryContext context { &regengine };
          processor_ctor_registry_context = &context;
          AudioProcessorP testproc = entry->create (nullptr);
          processor_ctor_registry_context = saved;
          if (testproc)
            {
              testproc->query_info (*entry);
              testproc = nullptr;
              if (entry->uri.empty())
                warning ("invalid empty URI for AudioProcessor: %s:%d", entry->file, entry->line);
              else
                {
                  if (processor_registry_table->find (entry->uri) != processor_registry_table->end())
                    warning ("duplicate AudioProcessor URI: %s", entry->uri);
                  else
                    (*processor_registry_table)[entry->uri] = entry;
                }
            }
          RegistryId::Entry *const old = entry;
          entry = old->next;
          // unlisted entries are left dangling for registry_create(RegistryId,std::any)
        }
    }
  while (regengine.ipc_pending())
    regengine.ipc_dispatch(); // empty any work queues
}

/// Create a new AudioProcessor object of the type specified by `uuiduri`.
AudioProcessorP
AudioProcessor::registry_create (AudioEngine &engine, const std::string &uuiduri)
{
  registry_init();
  RegistryId::Entry *entry = nullptr;
  { // lock scope
    std::lock_guard<std::recursive_mutex> rlocker (processor_registry_mutex);
    auto it = processor_registry_table->find (uuiduri);
    if (it != processor_registry_table->end())
      entry = it->second;
  }
  AudioProcessorP procp;
  if (entry)
    {
      ProcessorRegistryContext *const saved = processor_ctor_registry_context;
      ProcessorRegistryContext context { &engine };
      processor_ctor_registry_context = &context;
      procp = entry->create (nullptr);
      processor_ctor_registry_context = saved;
      if (procp)
        procp->ensure_initialized();
    }
  return procp;
}

AudioProcessorP
AudioProcessor::registry_create (AudioEngine &engine, RegistryId regitry_id, const std::any &any)
{
  assert_return (regitry_id.entry.create != nullptr, {});
  ProcessorRegistryContext *const saved = processor_ctor_registry_context;
  ProcessorRegistryContext context { &engine };
  processor_ctor_registry_context = &context;
  AudioProcessorP procp = regitry_id.entry.create (&any);
  processor_ctor_registry_context = saved;
  if (procp)
    procp->ensure_initialized();
  return procp;
}

/// List the registry entries of all known AudioProcessor types.
AudioProcessorInfoS
AudioProcessor::registry_list()
{
  registry_init();
  RegistryList rlist;
  std::lock_guard<std::recursive_mutex> rlocker (processor_registry_mutex);
  rlist.reserve (processor_registry_table->size());
  for (std::pair<CString, RegistryId::Entry*> el : *processor_registry_table)
    rlist.push_back (*el.second);
  return rlist;
}

// == AudioProcessor::PParam ==
AudioProcessor::PParam::PParam (ParamId _id, uint order, const ParamInfo &pinfo) :
  id (_id), info (std::make_shared<ParamInfo> (_id, order))
{
  info->copy_fields (pinfo);
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
  flags_ = src.flags_.load();
  value_ = src.value_.load();
  info = src.info;
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
  DeviceImplP   device_;
  ParamInfoP    info_;
  const ParamId id_;
public:
  String   identifier     () override   { return info_->ident; }
  String   label          () override   { return info_->label; }
  String   nick           () override   { return info_->nick; }
  String   unit           () override   { return info_->unit; }
  String   hints          () override   { return info_->hints; }
  String   group          () override   { return info_->group; }
  String   blurb          () override   { return info_->blurb; }
  String   description    () override   { return info_->description; }
  double   get_min        () override   { double mi, ma, st; info_->get_range (mi, ma, st); return mi; }
  double   get_max        () override   { double mi, ma, st; info_->get_range (mi, ma, st); return ma; }
  double   get_step       () override   { double mi, ma, st; info_->get_range (mi, ma, st); return st; }
  explicit
  AudioPropertyImpl (DeviceImplP devp, ParamId id, ParamInfoP param_) :
    device_ (devp), info_ (param_), id_ (id)
  {}
  void
  reset () override
  {
    set_value (info_->get_initial());
  }
  Value
  get_value () override
  {
    const AudioProcessorP proc = device_->audio_processor();
    return AudioProcessor::param_peek_mt (proc, id_);
  }
  bool
  set_value (const Value &value) override
  {
    const AudioProcessorP proc = device_->audio_processor();
    const ParamId pid = id_;
    const double v = value.as_double();
    auto lambda = [proc, pid, v] () {
      proc->set_param (pid, v);
    };
    proc->engine().async_jobs += lambda;
    return true;
  }
  double
  get_normalized () override
  {
    const AudioProcessorP proc = device_->audio_processor();
    return proc->value_to_normalized (id_, AudioProcessor::param_peek_mt (proc, id_));
  }
  bool
  set_normalized (double v) override
  {
    const AudioProcessorP proc = device_->audio_processor();
    const ParamId pid = id_;
    auto lambda = [proc, pid, v] () {
      proc->set_normalized (pid, v);
    };
    proc->engine().async_jobs += lambda;
    return true;
  }
  String
  get_text () override
  {
    const AudioProcessorP proc = device_->audio_processor();
    const double value = AudioProcessor::param_peek_mt (proc, id_);
    return proc->param_value_to_text (id_, value);
  }
  bool
  set_text (String vstr) override
  {
    const AudioProcessorP proc = device_->audio_processor();
    const double v = proc->param_value_from_text (id_, vstr);
    const ParamId pid = id_;
    auto lambda = [proc, pid, v] () {
      proc->set_param (pid, v);
    };
    proc->engine().async_jobs += lambda;
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
    ChoiceS cs;
    const auto ce = info_->get_choices();
    cs.reserve (ce.size());
    for (const auto &e : ce)
      {
        Choice c;
        c.ident = e.ident;
        c.label = e.label;
        c.subject = e.subject;
        c.icon = e.icon;
        cs.push_back (c);
      }
    return cs;
  }
};

// == AudioProcessor::access_properties ==
/// Retrieve/create Property handle from `id`.
/// This function is MT-Safe after proper AudioProcessor initialization.
PropertyP
AudioProcessor::access_property (ParamId id) const
{
  assert_return (is_initialized(), {});
  const PParam *param = find_pparam (id);
  assert_return (param, {});
  DeviceImplP devp = get_device();
  assert_return (devp, {});
  PropertyP newptr;
  PropertyP prop = weak_ptr_fetch_or_create<Property> (param->info->aprop_, [&] () {
      newptr = std::make_shared<AudioPropertyImpl> (devp, param->id, param->info);
      return newptr;
    });
  if (newptr.get() == prop.get())
    const_cast<AudioProcessor::PParam*> (param)->changed (false); // skip initial change notification
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
      DeviceImplP devicep = current->get_device (false);
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
            for (const PParam &p : current->params_)
              if (ASE_UNLIKELY (p.changed()) && const_cast<PParam&> (p).changed (false))
                {
                  PropertyP propi = p.info->aprop_.lock();
                  AudioPropertyImpl *aprop = dynamic_cast<AudioPropertyImpl*> (propi.get());
                  if (aprop)
                    aprop->emit_event ("notify", p.info->ident);
                }
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
