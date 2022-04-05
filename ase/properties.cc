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

static Value
call_getter (const ValueGetter &g)
{
  Value v;
  g(v);
  return v;
}

// == LambdaPropertyImpl ==
struct LambdaPropertyImpl : public virtual PropertyImpl {
  virtual ~LambdaPropertyImpl () {}
  Initializer d;
  const ValueGetter getter_;
  const ValueSetter setter_;
  const ValueLister lister_;
  const Value       vdefault_;
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
    d (initializer), getter_ (g), setter_ (s), lister_ (l), vdefault_ (call_getter (g))
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
  setter_ (vdefault_);
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
  using namespace Properties;
  assert_return (p, nullptr);
  return mkprop (initializer, Getter (p), Setter (p), lister);
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
Properties::Range (const String &ident, const ValueGetter &getter, const ValueSetter &setter, const String &label, const String &nickname, double pmin, double pmax, double dflt,
                   const String &unit, const String &hints, const String &blurb, const String &description)
{
  const Initializer initializer = {
    .ident = ident, .label = label, .nickname = nickname, .blurb = blurb, .description = description,
    .hints = construct_hints (hints, "range"), .pmin = pmin, .pmax = pmax, .pdef = dflt,
  };
  return mkprop (initializer, getter, setter, {});
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
