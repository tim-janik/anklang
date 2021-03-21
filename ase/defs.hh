// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_DEFS_HH__
#define __ASE_DEFS_HH__

#include <ase/cxxaux.hh>

namespace Ase {

// == Struct Forward Declarations ==
ASE_STRUCT_DECLS (Choice);
ASE_STRUCT_DECLS (DeviceInfo);
ASE_STRUCT_DECLS (DriverEntry);

// == Class Forward Declarations ==
ASE_CLASS_DECLS (AudioEngineThread);
ASE_CLASS_DECLS (AudioProcessor);
ASE_CLASS_DECLS (Clip);
ASE_CLASS_DECLS (Device);
ASE_CLASS_DECLS (Gadget);
ASE_CLASS_DECLS (Monitor);
ASE_CLASS_DECLS (Object);
ASE_CLASS_DECLS (Project);
ASE_CLASS_DECLS (Property);
ASE_CLASS_DECLS (Server);
ASE_CLASS_DECLS (SharedBase);
ASE_CLASS_DECLS (Track);

using StringS = std::vector<std::string>;

struct Event;
class EventConnection;
using EventHandler = std::function<void (const Event&)>;

class SharedBase;
using InstanceP = std::shared_ptr<SharedBase>;

struct Value;
using ValueP = std::shared_ptr<Value>;
// ValueS derives std::vector<ValueP>

} // Ase

#endif // __ASE_DEFS_HH__
