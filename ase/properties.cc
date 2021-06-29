// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "properties.hh"
#include "jsonipc/jsonipc.hh"
#include "strings.hh"
#include "utils.hh"
#include "internal.hh"

namespace Ase {

const String STORAGE = ":r:w:S:";
const String STANDARD = ":r:w:S:G:";

static String
canonify_identifier (const std::string &input)
{
  static const String validset = string_set_a2z() + "0123456789" + "_";
  const String lowered = string_tolower (input);
  String str = string_canonify (lowered, validset, "_");
  if (str.size() && str[0] >= '0' && str[0] <= '9')
    str = '_' + str;
  return str;
}

Property::~Property()
{}

namespace Properties {

String
construct_hints (const String &hints, const String &more, double pmin, double pmax)
{
  String h = hints;
  if (h.empty())
    h = STANDARD;
  if (h.back() != ':')
    h = h + ":";
  for (const auto &s : string_split (more))
    if (!s.empty() && "" == string_option_find (h, s, ""))
      h += s + ":";
  if (h[0] != ':')
    h = ":" + h;
  if (pmax > 0 && pmax == -pmin)
    h += "bidir:";
  return h;
}

PropertyImpl::~PropertyImpl()
{}

// == LambdaPropertyImpl ==
struct LambdaPropertyImpl : public virtual PropertyImpl {
  virtual ~LambdaPropertyImpl () {}
  Initializer d;
  const ValueGetter getter_;
  const ValueSetter setter_;
  const ValueLister lister_;
  void    notify         ();
  String  identifier     () override		{ return d.ident; }
  String  label          () override		{ return d.label; }
  String  nick           () override		{ return d.nickname; }
  String  unit           () override		{ return d.unit; }
  String  hints          () override		{ return d.hints; }
  String  group          () override		{ return d.groupname; }
  String  blurb          () override		{ return d.blurb; }
  String  description    () override		{ return d.description; }
  ChoiceS choices        () override		{ return lister_ ? lister_ (*static_cast<PropertyImpl*> (this)) : ChoiceS{}; }
  double  get_min        () override		{ return d.pmin; }
  double  get_max        () override		{ return d.pmax; }
  double  get_step       () override		{ return 0.0; }
  bool    is_numeric     () override;
  void    reset          () override;
  Value   get_value      () override;
  bool    set_value      (const Value &v) override;
  double  get_normalized () override;
  bool    set_normalized (double v) override;
  String  get_text       () override;
  bool    set_text       (String v) override;
  LambdaPropertyImpl (const Properties::Initializer &initializer, const ValueGetter &g, const ValueSetter &s, const ValueLister &l) :
    d (initializer), getter_ (g), setter_ (s), lister_ (l)
  {
    d.ident = canonify_identifier (d.ident.empty() ? d.label : d.ident);
    assert_return (initializer.ident.size());
  }
};
using LambdaPropertyImplP = std::shared_ptr<LambdaPropertyImpl>;
JSONIPC_INHERIT (LambdaPropertyImpl, Property);

void
LambdaPropertyImpl::notify()
{
  emit_event ("change", identifier());
}

Value
LambdaPropertyImpl::get_value ()
{
  Value val;
  getter_ (val);
  return val;
}

bool
LambdaPropertyImpl::set_value (const Value &val)
{
  const bool changed = setter_ (val);
  if (changed)
    notify();
  return changed;
}

double
LambdaPropertyImpl::get_normalized () // TODO: implement
{
  Value val;
  getter_ (val);
  return val.as_double();
}

bool
LambdaPropertyImpl::set_normalized (double v) // TODO: implement
{
  Value val { v };
  const bool changed = setter_ (val);
  if (changed)
    notify();
  return changed;
}

String
LambdaPropertyImpl::get_text ()
{
  Value val;
  getter_ (val);
  return val.as_string();
}

bool
LambdaPropertyImpl::set_text (String v)
{
  Value val { v };
  const bool changed = setter_ (val);
  if (changed)
    notify();
  return changed;
}

bool
LambdaPropertyImpl::is_numeric ()
{
  Value val;
  getter_ (val);
  switch (val.index())
    {
    case Value::BOOL:           return true;
    case Value::INT64:          return true;
    case Value::DOUBLE:         return true;
    case Value::ARRAY:
    case Value::RECORD:
    case Value::STRING:
    case Value::INSTANCE:
    case Value::NONE: ; // std::monostate
    }
  return false;
}

void
LambdaPropertyImpl::reset ()
{
  Value val;
  getter_ (val);
  bool changed = false;
  switch (val.index())
    {
    case Value::BOOL:
      if (false != std::get<bool> (val))
        {
          val = false;
          changed = true;
        }
      break;
    case Value::INT64:
      if (0 != std::get<int64> (val))
        {
          val = int64 (0);
          changed = true;
        }
      break;
    case Value::DOUBLE:
      if (0.0 != std::get<double> (val))
        {
          val = 0.0;
          changed = true;
        }
      break;
    case Value::STRING:
      if (!std::get<String> (val).empty())
        {
          val = "";
          changed = true;
        }
      break;
    case Value::ARRAY:
      if (!std::get<ValueS> (val).empty())
        {
          std::get<ValueS> (val) = {};
          changed = true;
        }
      break;
    case Value::RECORD:
      if (!std::get<ValueR> (val).empty())
        {
          std::get<ValueR> (val) = {};
          changed = true;
        }
      break;
    case Value::INSTANCE:
      if (std::get<InstanceP> (val))
        {
          std::get<InstanceP> (val) = nullptr;
          changed = true;
        }
      break;
    case Value::NONE: ; // std::monostate
    }
  if (changed)
    changed = setter_ (val);
  if (changed)
    notify();
}

PropertyImplP
mkprop (const Initializer &initializer, const ValueGetter &getter, const ValueSetter &setter, const ValueLister &lister)
{
  return std::make_shared<Properties::LambdaPropertyImpl> (initializer, getter, setter, lister);
}

// == Bag ==
Bag&
Bag::operator+= (PropertyP p)
{
  if (!group.empty() && p->group().empty())
    {
      LambdaPropertyImpl *simple = dynamic_cast<LambdaPropertyImpl*> (p.get());
      if (simple)
        simple->d.groupname = group;
    }
  props.push_back (p);
  return *this;
}

void
Bag::on_events (const String &eventselector, const EventHandler &eventhandler)
{
  for (auto p : props)
    connections.push_back (p->on_event ("change", eventhandler));
}

} // Properties

template<class V> static PropertyP
ptrprop (const Properties::Initializer &initializer, V *p, const Properties::ValueLister &lister = {})
{
  assert_return (p, nullptr);
  auto setter = [p] (const Value &val) {
    V v = {};
    if_constexpr (std::is_floating_point<V>::value)
      v = val.as_double();
    else if_constexpr (std::is_integral<V>::value)
      v = val.as_int();
    else if_constexpr (std::is_base_of<::std::string, V>::value)
      v = val.as_string();
    else
      static_assert (sizeof (V) < 0, "Property type unimplemented");
    return_unless (v != *p, false);
    *p = v;
    return true;
  };
  auto getter = [p] (Value &val) { val = *p; };
  return mkprop (initializer, getter, setter, lister);
}

// == Property constructors ==
PropertyP
Properties::Bool (const String &ident, bool *v, const String &label, const String &nickname, bool dflt, const String &hints, const String &blurb, const String &description)
{
  return ptrprop ({ .ident = ident, .label = label, .nickname = nickname, .blurb = blurb, .description = description,
                    .hints = construct_hints (hints, "bool"), .pdef = double (dflt) }, v);
}

PropertyP
Properties::Range (const String &ident, int32 *v, const String &label, const String &nickname, int32 pmin, int32 pmax, int32 dflt,
                   const String &unit, const String &hints, const String &blurb, const String &description)
{
  return ptrprop ({ .ident = ident, .label = label, .nickname = nickname, .blurb = blurb, .description = description,
                    .hints = construct_hints (hints, "range"),
                    .pmin = double (pmin), .pmax = double (pmax), .pdef = double (dflt) }, v);
}

PropertyP
Properties::Range (const String &ident, float *v, const String &label, const String &nickname, double pmin, double pmax, double dflt,
                   const String &unit, const String &hints, const String &blurb, const String &description)
{
  return ptrprop ({ .ident = ident, .label = label, .nickname = nickname, .blurb = blurb, .description = description,
                    .hints = construct_hints (hints, "range"), .pmin = pmin, .pmax = pmax, .pdef = dflt }, v);
}

PropertyP
Properties::Range (const String &ident, double *v, const String &label, const String &nickname, double pmin, double pmax, double dflt,
                   const String &unit, const String &hints, const String &blurb, const String &description)
{
  return ptrprop ({ .ident = ident, .label = label, .nickname = nickname, .blurb = blurb, .description = description,
                    .hints = construct_hints (hints, "range"), .pmin = pmin, .pmax = pmax, .pdef = dflt }, v);
}

PropertyP
Properties::Text (const String &ident, String *v, const String &label, const String &nickname, const String &hints, const String &blurb, const String &description)
{
  return ptrprop ({ .ident = ident, .label = label, .nickname = nickname, .blurb = blurb, .description = description,
                    .hints = construct_hints (hints, "text") }, v);
}

PropertyP
Properties::Text (const String &ident, String *v, const String &label, const String &nickname, const ValueLister &vl, const String &hints, const String &blurb, const String &description)
{
  PropertyP propp;
  propp = ptrprop ({ .ident = ident, .label = label, .nickname = nickname, .blurb = blurb, .description = description,
                     .hints = construct_hints (hints, "text:choice") }, v, vl);
  return propp;
}

} // Ase
