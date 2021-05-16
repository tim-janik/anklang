// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DEFS_HH__
#define __ASE_DEFS_HH__

#include <ase/cxxaux.hh>

namespace Ase {

enum class Error;

// == Struct Forward Declarations ==
ASE_STRUCT_DECLS (AudioProcessorInfo);
ASE_STRUCT_DECLS (Choice);
ASE_STRUCT_DECLS (DeviceInfo);
ASE_STRUCT_DECLS (DriverEntry);
ASE_STRUCT_DECLS (ParamInfo);
ASE_STRUCT_DECLS (Resource);

// == Class Forward Declarations ==
ASE_CLASS_DECLS (AudioChain);
ASE_CLASS_DECLS (AudioCombo);
ASE_CLASS_DECLS (AudioCombo);
ASE_CLASS_DECLS (AudioEngineThread);
ASE_CLASS_DECLS (AudioProcessor);
ASE_CLASS_DECLS (AudioProcessor);
ASE_CLASS_DECLS (Clip);
ASE_CLASS_DECLS (Device);
ASE_CLASS_DECLS (DeviceImpl);
ASE_CLASS_DECLS (FileCrawler);
ASE_CLASS_DECLS (Gadget);
ASE_CLASS_DECLS (Monitor);
ASE_CLASS_DECLS (Object);
ASE_CLASS_DECLS (Project);
ASE_CLASS_DECLS (ProjectImpl);
ASE_CLASS_DECLS (Property);
ASE_CLASS_DECLS (ResourceCrawler);
ASE_CLASS_DECLS (Server);
ASE_CLASS_DECLS (ServerImpl);
ASE_CLASS_DECLS (SharedBase);
ASE_CLASS_DECLS (Track);
ASE_CLASS_DECLS (TrackImpl);

class AudioEngine;

struct Event;
class EventConnection;
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

} // Ase

#endif // __ASE_DEFS_HH__
