// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PROPERTIES_HH__
#define __ASE_PROPERTIES_HH__

#include <ase/object.hh>

namespace Ase {

using ValueGetter = std::function<void (Value&)>;
using ValueSetter = std::function<bool (const Value&)>;

/// Implementation namespace for Property helpers
namespace Properties {

/// Construct Bool property.
PropertyP Bool   (bool   *v, const String &label, const String &nickname, bool dflt, const String &hints = "", const String &blurb = "", const String &description = "");

/// Construct Enum property.
PropertyP Enum   (String *v, const String &label, const String &nickname, int64 dflt, const String &hints = "", const String &blurb = "", const String &description = "");

/// Construct integer Range property.
PropertyP Range  (int32  *v, const String &label, const String &nickname, int32 pmin, int32 pmax, int32 dflt,
                  const String &unit = "", const String &hints = "", const String &blurb = "", const String &description = "");

/// Construct float Range property.
PropertyP Range  (double *v, const String &label, const String &nickname, double pmin, double pmax, double dflt,
                  const String &unit = "", const String &hints = "", const String &blurb = "", const String &description = "");

/// Construct Text string property.
PropertyP Text   (String *v, const String &label, const String &nickname, const String &hints = "", const String &blurb = "", const String &description = "");

/// Helper for construction of Property lists.
class Bag {
  EventHandler notify_;
public:
  explicit  Bag   (const EventHandler &eventhandler = {});
  PropertyS props;
  String    group;
  Bag&      operator+= (PropertyP p);
};

// == Implementation Helpers ==
struct Initializer {
  std::string ident;
  std::string label;
  std::string nickname;
  std::string unit;
  std::string blurb;
  std::string description;
  std::string groupname;
  std::string hints;
  double pmin = -1.7976931348623157e+308;
  double pmax = +1.7976931348623157e+308;
  double pdef = 0;
};

struct PropertyImpl : public virtual Property, public virtual EmittableImpl {
  virtual ~PropertyImpl ();
};
using PropertyImplP = std::shared_ptr<PropertyImpl>;

/// Construct Property with handlers, emits `Event { .type = "change", .detail = identifier() }`.
PropertyImplP mkprop (const Initializer &initializer, const ValueGetter&, const ValueSetter&);

} // Properties

using PropertyBag = Properties::Bag;

} // Ase

#endif // __ASE_PROPERTIES_HH__
