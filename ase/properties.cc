// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "properties.hh"
#include "jsonipc/jsonipc.hh"
#include "strings.hh"
#include "utils.hh"
#include "regex.hh"
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
  emit_notify (identifier());
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
    connections.push_back (p->on_event (eventselector, eventhandler));
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

// == guess_nick ==
using String3 = std::tuple<String,String,String>;

// Fast version of Re::search (R"(\d)")
static ssize_t
search_first_digit (const String &s)
{
  for (size_t i = 0; i < s.size(); ++i)
    if (isdigit (s[i]))
      return i;
  return -1;
}

// Fast version of Re::search (R"(\d\d?\b)")
static ssize_t
search_last_digits (const String &s)
{
  for (size_t i = 0; i < s.size(); ++i)
    if (isdigit (s[i])) {
      if (isdigit (s[i+1]) && !isalnum (s[i+2]))
        return i;
      else if (!isalnum (s[i+1]))
        return i;
    }
  return -1;
}

// Exract up to 3 useful letters or words from label
static String3
make_nick3 (const String &label)
{
  // split words
  const StringS words = Re::findall (R"(\b\w+)", label); // TODO: allow Re caching

  // single word nick, give precedence to digits
  if (words.size() == 1) {
    const ssize_t d = search_first_digit (words[0]);
    if (d > 0 && isdigit (words[0][d + 1]))                     // A11
      return { words[0].substr (0, 1), words[0].substr (d, 2), "" };
    if (d > 0)                                                  // Aa1
      return { words[0].substr (0, 2), words[0].substr (d, 1), "" };
    else                                                        // Aaa
      return { words[0].substr (0, 3), "", "" };
  }

  // two word nick, give precedence to second word digits
  if (words.size() == 2) {
    const ssize_t e = search_last_digits (words[1]);
    if (e >= 0 && isdigit (words[1][e+1]))                      // A22
      return { words[0].substr (0, 1), words[1].substr (e, 2), "" };
    if (e > 0)                                                  // AB2
      return { words[0].substr (0, 1), words[1].substr (0, 1), words[1].substr (e, 1) };
    if (e == 0)                                                 // Aa2
      return { words[0].substr (0, 2), words[1].substr (e, 1), "" };
    const ssize_t d = search_first_digit (words[0]);
    if (d > 0)                                                  // A1B
      return { words[0].substr (0, 1), words[0].substr (d, 1), words[1].substr (0, 1) };
    if (words[1].size() > 1)                                    // ABb
      return { words[0].substr (0, 1), words[1].substr (0, 2), "" };
    else                                                        // AaB
      return { words[0].substr (0, 2), words[1].substr (0, 1), "" };
  }

  // 3+ word nick
  if (words.size() >= 3) {
    ssize_t i, e = -1; // digit pos in last possible word
    for (i = words.size() - 1; i > 1; i--) {
      e = search_last_digits (words[i]);
      if (e >= 0)
        break;
    }
    if (e >= 0 && isdigit (words[i][e + 1]))                    // A66
      return { words[0].substr (0, 1), words[i].substr (e, 2), "" };
    if (e >= 0 && i + 1 < words.size())                         // A6G
      return { words[0].substr (0, 1), words[i].substr (e, 1), words[i+1].substr (0, 1) };
    if (e > 0)                                                  // AF6
      return { words[0].substr (0, 1), words[i].substr (0, 1), words[i].substr (e, 1) };
    if (e == 0 && i >= 3)                                       // AE6
      return { words[0].substr (0, 1), words[i-1].substr (0, 1), words[i].substr (e, 1) };
    if (e == 0 && i >= 2)                                       // AB6
      return { words[0].substr (0, 1), words[1].substr (0, 1), words[i].substr (e, 1) };
    if (e == 0)                                                 // Aa6
      return { words[0].substr (0, 2), words[i].substr (e, 1), "" };
    if (words.back().size() >= 2)                               // AFf
      return { words[0].substr (0, 1), words.back().substr (0, 2), "" };
    else                                                        // AEF
      return { words[0].substr (0, 1), words[words.size()-1].substr (0, 1), words.back().substr (0, 1) };
  }

  // pathological name
  return { words[0].substr (0, 3), "", "" };
}

// Re::sub (R"(([a-zA-Z])([0-9]))", "$1 $2", s)
static String
spaced_nums (String s)
{
  for (ssize_t i = s.size() - 1; i > 0; i--)
    if (isdigit (s[i]) && !isdigit (s[i-1]) && !isspace (s[i-1]))
      s.insert (s.begin() + i, ' ');
  return s;
}

/// Create a few letter nick name from a multi word property label.
String
property_guess_nick (const String &property_label)
{
  // seperate numbers from words, increases word count
  String string = spaced_nums (property_label);

  // use various letter extractions to construct nick portions
  const auto& [a, b, c] = make_nick3 (string);

  // combine from right to left to increase word variance
  String nick;
  if (c.size() > 0)
    nick = a.substr (0, 1) + b.substr (0, 1) + c.substr (0, 1);
  else if (b.size() > 0)
    nick = a.substr (0, 1) + b.substr (0, 2);
  else
    nick = a.substr (0, 3);
  return nick;
}

} // Ase
