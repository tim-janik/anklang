// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DEFS_HH__
#define __ASE_DEFS_HH__

#include <ase/cxxaux.hh>

namespace Ase {

// = Constants ==
static constexpr uint64_t U64MAX = +18446744073709551615ull;  // 2^64-1
static constexpr int64_t  I63MAX = +9223372036854775807;      // 2^63-1
static constexpr int64_t  I63MIN = -9223372036854775807 - 1;  // -2^63
static constexpr uint32_t U32MAX = +4294967295u;              // 2^32-1
static constexpr int32_t  I31MAX = +2147483647;               // 2^31-1
static constexpr int32_t  I31MIN = -2147483648;               // -2^31
static constexpr float    M23MAX = 16777216;                  // 2^(1+23); IEEE-754 Float Mantissa maximum
static constexpr float    F32EPS = 5.9604644775390625e-08;    // 2^-24, round-off error at 1.0
static constexpr float    F32MAX = 3.40282347e+38;            // 7f7fffff, 2^128 * (1 - F32EPS)
static constexpr double   M52MAX = 9007199254740992;          // 2^(1+52); IEEE-754 Double Mantissa maximum
static constexpr double   D64MAX = 1.7976931348623157e+308;   // 0x7fefffff ffffffff, IEEE-754 Double Maximum
static constexpr int64_t  AUDIO_BLOCK_MAX_RENDER_SIZE = 2048;

// == Forward Declarations ==
enum class Error;

// == Struct Forward Declarations ==
ASE_STRUCT_DECLS (AudioProcessorInfo);
ASE_STRUCT_DECLS (Choice);
ASE_STRUCT_DECLS (ClapParamUpdate);
ASE_STRUCT_DECLS (ClipNote);
ASE_STRUCT_DECLS (DeviceInfo);
ASE_STRUCT_DECLS (DriverEntry);
ASE_STRUCT_DECLS (Parameter);
ASE_STRUCT_DECLS (Resource);
ASE_STRUCT_DECLS (TelemetryField);
ASE_STRUCT_DECLS (TelemetrySegment);
ASE_STRUCT_DECLS (UserNote);

// == Class Forward Declarations ==
ASE_CLASS_DECLS (AudioChain);
ASE_CLASS_DECLS (AudioCombo);
ASE_CLASS_DECLS (AudioCombo);
ASE_CLASS_DECLS (AudioEngineThread);
ASE_CLASS_DECLS (AudioProcessor);
ASE_CLASS_DECLS (ClapDeviceImpl);
ASE_CLASS_DECLS (ClapPluginHandle);
ASE_CLASS_DECLS (Clip);
ASE_CLASS_DECLS (ClipImpl);
ASE_CLASS_DECLS (Device);
ASE_CLASS_DECLS (DeviceImpl);
ASE_CLASS_DECLS (Emittable);
ASE_CLASS_DECLS (FileCrawler);
ASE_CLASS_DECLS (Gadget);
ASE_CLASS_DECLS (GadgetImpl);
ASE_CLASS_DECLS (LV2DeviceImpl);
ASE_CLASS_DECLS (Monitor);
ASE_CLASS_DECLS (NativeDevice);
ASE_CLASS_DECLS (NativeDeviceImpl);
ASE_CLASS_DECLS (Object);
ASE_CLASS_DECLS (Preference);
ASE_CLASS_DECLS (Project);
ASE_CLASS_DECLS (ProjectImpl);
ASE_CLASS_DECLS (Property);
ASE_CLASS_DECLS (PropertyImpl);
ASE_CLASS_DECLS (ResourceCrawler);
ASE_CLASS_DECLS (Server);
ASE_CLASS_DECLS (ServerImpl);
ASE_CLASS_DECLS (SharedBase);
ASE_CLASS_DECLS (StreamReader);
ASE_CLASS_DECLS (StreamWriter);
ASE_CLASS_DECLS (Track);
ASE_CLASS_DECLS (TrackImpl);

class AudioEngine;
class CustomDataContainer;

using CallbackS = std::vector<std::function<void()>>;
using DCallbackS = std::vector<std::function<void(double)>>;

struct Event;
class EventConnection;
using EventConnectionW = std::weak_ptr<EventConnection>;
using EventConnectionP = std::shared_ptr<EventConnection>;
using EventHandler = std::function<void (const Event&)>;

class SharedBase;
using InstanceP = std::shared_ptr<SharedBase>;

struct Value;
using ValueP = std::shared_ptr<Value>;
// ValueS derives std::vector<ValueP>

class WritNode;
using WritNodeS = std::vector<WritNode>;

// == Serializable ==
/// Interface for serializable objects with Reflink support.
class Serializable : public virtual VirtualBase {
  friend WritNode;
protected:
  virtual void serialize (WritNode &xs) = 0; ///< Serialize members and childern
  // see serialize.cc
};

// == IconString ==
struct IconString : String {};

} // Ase

#endif // __ASE_DEFS_HH__
