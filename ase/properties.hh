// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_PROPERTIES_HH__
#define __ASE_PROPERTIES_HH__

#include <ase/object.hh>
#include <ase/memory.hh>
#include <ase/jsonapi.hh>

namespace Ase {

/// A named ID used to group parameters.
struct GroupId : CString {
  using CString::CString;
  using CString::operator=;
  using CString::operator==;
  using CString::operator!=;
};

/// Implementation namespace for Property helpers
namespace Properties {

struct PropertyImpl;
using ValueGetter = std::function<void (Value&)>;
using ValueSetter = std::function<bool (const Value&)>;
using ValueLister = std::function<ChoiceS (PropertyImpl&)>;

/// Helper for property hint construction.
String construct_hints (const String &hints, const String &more, double pmin = 0, double pmax = 0);

/// Construct Bool property.
PropertyP        Bool   (const String &ident, bool *v, const String &label, const String &nickname, bool dflt, const String &hints = "", const String &blurb = "", const String &description = "");
inline PropertyP Bool   (bool *v, const String &label, const String &nickname, bool dflt, const String &hints = "", const String &blurb = "", const String &description = "")
{ return Bool (label, v, label, nickname, dflt, hints, blurb, description); }

/// Construct integer Range property.
PropertyP        Range  (const String &ident, int32  *v, const String &label, const String &nickname, int32 pmin, int32 pmax, int32 dflt,
                         const String &unit = "", const String &hints = "", const String &blurb = "", const String &description = "");
inline PropertyP Range  (int32  *v, const String &label, const String &nickname, int32 pmin, int32 pmax, int32 dflt,
                         const String &unit = "", const String &hints = "", const String &blurb = "", const String &description = "")
{ return Range (label, v, label, nickname, pmin, pmax, dflt, unit, hints, blurb, description); }

/// Construct float Range property.
PropertyP        Range  (const String &ident, float *v, const String &label, const String &nickname, double pmin, double pmax, double dflt,
                         const String &unit = "", const String &hints = "", const String &blurb = "", const String &description = "");
inline PropertyP Range  (float *v, const String &label, const String &nickname, double pmin, double pmax, double dflt,
                         const String &unit = "", const String &hints = "", const String &blurb = "", const String &description = "")
{ return Range (label, v, label, nickname, pmin, pmax, dflt, unit, hints, blurb, description); }

/// Construct double Range property.
PropertyP        Range  (const String &ident, double *v, const String &label, const String &nickname, double pmin, double pmax, double dflt,
                         const String &unit = "", const String &hints = "", const String &blurb = "", const String &description = "");
inline PropertyP Range  (double *v, const String &label, const String &nickname, double pmin, double pmax, double dflt,
                         const String &unit = "", const String &hints = "", const String &blurb = "", const String &description = "")
{ return Range (label, v, label, nickname, pmin, pmax, dflt, unit, hints, blurb, description); }

/// Construct Text string property.
PropertyP        Text   (const String &ident, String *v, const String &label, const String &nickname, const String &hints = "", const String &blurb = "", const String &description = "");
inline PropertyP Text   (String *v, const String &label, const String &nickname, const String &hints = "", const String &blurb = "", const String &description = "")
{ return Text (label, v, label, nickname, hints, blurb, description); }

/// Construct Choice property.
PropertyP        Text   (const String &ident, String *v, const String &label, const String &nickname, const ValueLister &vl, const String &hints = "", const String &blurb = "", const String &description = "");
inline PropertyP Text   (String *v, const String &label, const String &nickname, const ValueLister &vl, const String &hints = "", const String &blurb = "", const String &description = "")
{ return Text (label, v, label, nickname, vl, hints, blurb, description); }

/// Construct Enum property.
template<typename E, REQUIRES< std::is_enum<E>::value > = true> inline PropertyP
Enum (const String &ident, E *v, const String &label, const String &nickname, const String &hints = "", const String &blurb = "", const String &description = "")
{
  using EnumType = Jsonipc::Enum<E>;
  ASE_ASSERT_RETURN (v, nullptr);
  auto setter = [v] (const Value &val) {
    E e = *v;
    if (val.index() == Value::STRING)
      e = EnumType::get_value (val.as_string(), e);
    else if (val.index() == Value::INT64)
      e = E (val.as_int());
    ASE_RETURN_UNLESS (e != *v, false);
    *v = e;
    return true;
  };
  auto getter = [v] (Value &val) {
    if (EnumType::has_names())
      {
        const String &name = EnumType::get_name (*v);
        if (!name.empty())
          {
            val = name;
            return;
          }
      }
    val = int64 (*v);
  };
  auto lister = [] (PropertyImpl &prop) {
    ChoiceS choices;
    for (const auto &evalue : EnumType::list_values())
      {
        Choice choice (evalue.second, evalue.second);
        choices.push_back (choice);
      }
    return choices;
  };
  return mkprop ({ .ident = ident, .label = label, .nickname = nickname, .blurb = blurb, .description = description,
                   .hints = construct_hints (hints, "bool"), }, getter, setter, lister);
}

/// Helper for construction of Property lists.
class Bag {
public:
  using ConnectionS = std::vector<EventConnectionP>;
  Bag&        operator+= (PropertyP p);
  void        on_events  (const String &eventselector, const EventHandler &eventhandler);
  ConnectionS connections;
  GroupId     group;
  PropertyS   props;
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

struct PropertyImpl : public EmittableImpl, public virtual Property {
  virtual ~PropertyImpl ();
};
using PropertyImplP = std::shared_ptr<PropertyImpl>;

/// Construct Property with handlers, emits `Event { .type = "change", .detail = identifier() }`.
PropertyImplP mkprop (const Initializer &initializer, const ValueGetter&, const ValueSetter&, const ValueLister&);

} // Properties

using PropertyBag = Properties::Bag;

} // Ase

#endif // __ASE_PROPERTIES_HH__
