// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "parameter.hh"
#include "levenshtein.hh"
#include "unicode.hh"
#include "regex.hh"
#include "mathutils.hh"
#include "internal.hh"

namespace Ase {

// == Param ==
Param::ExtraVals::ExtraVals (const MinMaxStep &range)
{
  using Base = std::variant<MinMaxStep,ChoiceS,ChoicesFunc>;
  Base::operator= (range);
}
Param::ExtraVals::ExtraVals (const std::initializer_list<Choice> &choices)
{
  *this = ChoiceS (choices);
}

Param::ExtraVals::ExtraVals (const ChoiceS &choices)
{
  using Base = std::variant<MinMaxStep,ChoiceS,ChoicesFunc>;
  Base::operator= (choices);
}

Param::ExtraVals::ExtraVals (const ChoicesFunc &choicesfunc)
{
  using Base = std::variant<MinMaxStep,ChoiceS,ChoicesFunc>;
  Base::operator= (choicesfunc);
}

// == Parameter ==
String
Parameter::construct_hints (const String &hints, const String &more, double pmin, double pmax)
{
  String h = hints;
  if (h.empty())
    h = STANDARD;
  if (h[0] != ':')
    h = ":" + h;
  if (h.back() != ':')
    h = h + ":";
  if (pmax > 0 && pmax == -pmin && "" == string_option_find (h, "bidir", ""))
    h += "bidir:";
  if (pmin != pmax && "" == string_option_find (h, "range", ""))
    h += "range:";
  for (const auto &s : string_split (more, ":"))
    if (!s.empty() && "" == string_option_find (h, s, ""))
      h += s + ":";
  if (h.back() != ':')
    h = h + ":";
  return h;
}

String
Parameter::nick () const
{
  String nick = fetch ("nick");
  if (nick.empty())
    nick = parameter_guess_nick (label());
  return nick;
}

bool
Parameter::has (const String &key) const
{
  return key == "ident" || kvpairs_search (details_, key) >= 0;
}

String
Parameter::fetch (const String &key) const
{
  return_unless (key != "ident", cident);
  const ssize_t i = kvpairs_search (details_, key);
  return i >= 0 ? details_[i].data() + key.size() + 1 : "";
}

void
Parameter::store (const String &key, const String &value)
{
  assert_return (key.size() > 0);
  if (key == "ident") {
    cident = value;
    return;
  }
  const ssize_t i = kvpairs_search (details_, key);
  const std::string kv = key + '=' + value;
  if (i >= 0)
    details_[i] = kv;
  else
    details_.push_back (kv);
}

MinMaxStep
Parameter::range () const
{
  if (const auto mms = std::get_if<MinMaxStep> (&extras_))
    return *mms;
  const ChoiceS cs = choices();
  if (cs.size())
    return { 0, cs.size() -1, 1 };
  return { NAN, NAN, NAN };
}

ChoiceS
Parameter::choices () const
{
  if (const auto cs = std::get_if<ChoiceS> (&extras_))
    return *cs;
  if (const auto cf = std::get_if<ChoicesFunc> (&extras_))
    return (*cf) (cident);
  return {};
}

bool
Parameter::is_numeric () const
{
  auto [i, a, s] = range();
  return i != a;
}

void
Parameter::initialsync (const Value &v)
{
  initial_ = v;
}

bool
Parameter::has_hint (const String &hint) const
{
  const String hints_ = hints();
  size_t pos = 0;
  while ((pos = hints_.find (hint, pos)) != std::string::npos) {
    if ((pos == 0 || hints_[pos-1] == ':') && (pos + hint.size() == hints_.size() || hints_[pos + hint.size()] == ':'))
      return true;
    pos += hint.size();
  }
  return false;
}

double
Parameter::normalize (double val) const
{
  const auto [fmin, fmax, step] = range();
  if (std::abs (fmax - fmin) < F32EPS)
    return 0;
  const double normalized = (val - fmin) / (fmax - fmin);
  assert_return (normalized >= 0.0 && normalized <= 1.0, normalized);
  return normalized;
}

double
Parameter::rescale (double t) const
{
  const auto [fmin, fmax, step] = range();
  const double value = fmin + t * (fmax - fmin);
  assert_return (t >= 0.0 && t <= 1.0, value);
  return value;
}

size_t
Parameter::match_choice (const ChoiceS &choices, const String &text)
{
  for (size_t i = 0; i < choices.size(); i++)
    if (text == choices[i].ident)
      return i;
  size_t selected = 0;
  const String ltext = string_tolower (text);
  float best = F32MAX;
  for (size_t i = 0; i < choices.size(); i++) {
    const size_t maxdist = std::max (choices[i].ident.size(), ltext.size());
    const float dist = damerau_levenshtein_restricted (string_tolower (choices[i].ident), ltext) / maxdist;
    if (dist < best) {
      best = dist;
      selected = i;
    }
  }
  return selected;
}

Value
Parameter::constrain (const Value &value) const
{
  // choices
  if (is_choice()) {
    const ChoiceS choices = this->choices();
    if (value.is_numeric()) {
      int64_t i = value.as_int();
      if (i >= 0 && i < choices.size())
        return choices[i].ident;
    }
    const size_t selected = value.is_string() ? match_choice (choices, value.as_string()) : 0;
    return choices.size() ? choices[selected].ident : initial_;
  }
  // text
  if (is_text())
    return value.as_string();
  // numeric
  return dconstrain (value);
}

double
Parameter::dconstrain (const Value &value) const
{
  // choices
  if (is_choice()) {
    const ChoiceS choices = this->choices();
    if (value.is_numeric()) {
      int64_t i = value.as_int();
      if (i >= 0 && i < choices.size())
        return i;
    }
    const size_t selected = value.is_string() ? match_choice (choices, value.as_string()) : 0;
    return choices.size() ? selected : initial_.is_numeric() ? initial_.as_double() : 0;
  }
  // numeric
  double val = value.as_double();
  const auto [fmin, fmax, step] = range();
  if (std::abs (fmax - fmin) < F32EPS)
    return fmin;
  val = std::max (val, fmin);
  val = std::min (val, fmax);
  if (step > F32EPS && has_hint ("stepped")) {
    // round halfway cases down, so:
    // 0 -> -0.5…+0.5 yields -0.5
    // 1 -> -0.5…+0.5 yields +0.5
    constexpr const auto nearintoffset = 0.5 - DOUBLE_EPSILON; // round halfway case down
    const double t = std::floor ((val - fmin) / step + nearintoffset);
    val = fmin + t * step;
    val = std::min (val, fmax);
  }
  return val;
}

static MinMaxStep
minmaxstep_from_initialval (const Param::InitialVal &iv, bool *isbool)
{
  MinMaxStep range;
  std::visit ([&] (auto &&arg)
  {
    using T = std::decay_t<decltype (arg)>;
    if constexpr (std::is_same_v<T, bool>)
      range = { 0, 1, 1 };
    else if constexpr (std::is_same_v<T, int8_t>)
      range = { -128, 127, 1 };
    else if constexpr (std::is_same_v<T, uint8_t>)
      range = { 0, 255, 1 };
    else if constexpr (std::is_same_v<T, int16_t>)
      range = { -32768, 32767, 1 };
    else if constexpr (std::is_same_v<T, uint16_t>)
      range = { 0, 65536, 1 };
    else if constexpr (std::is_same_v<T, int32_t>)
      range = { -2147483648, 2147483647, 1 };
    else if constexpr (std::is_same_v<T, uint32_t>)
      range = { 0, 4294967295, 1 };
    else if constexpr (std::is_same_v<T, int64_t>)
      range = { -9223372036854775807-1, 9223372036854775807, 1 };
    else if constexpr (std::is_same_v<T, uint64_t>)
      range = { 0, 18446744073709551615ull, 1 };
    else if constexpr (std::is_same_v<T, float>)
      range = { -F32MAX, F32MAX, 0 };
    else if constexpr (std::is_same_v<T, double>)
      range = { -D64MAX, D64MAX, 0 };
    else if constexpr (std::is_same_v<T, const char*>)
      range = { 0, 0, 0 }; // strings have no numeric range
    else if constexpr (std::is_same_v<T, std::string>)
      range = { 0, 0, 0 }; // strings have no numeric range
    else
      static_assert (sizeof (T) < 0, "unimplemented InitialVal type");
    if (isbool)
      *isbool = std::is_same_v<T, bool>;
  }, iv);
  return range;
}

static Value
value_from_initialval (const Param::InitialVal &iv)
{
  Value value;
  std::visit ([&value] (auto &&arg)
  {
    using T = std::decay_t<decltype (arg)>;
    if constexpr (std::is_same_v<T, bool>)
      value = arg;
    else if constexpr (std::is_same_v<T, int8_t> ||
                       std::is_same_v<T, uint8_t> ||
                       std::is_same_v<T, int16_t> ||
                       std::is_same_v<T, uint16_t> ||
                       std::is_same_v<T, int32_t>)
      value = int32_t (arg);
    else if constexpr (std::is_same_v<T, uint32_t>)
      value = arg;
    else if constexpr (std::is_same_v<T, int64_t> ||
                       std::is_same_v<T, uint64_t>)
      value = int64_t (arg);
    else if constexpr (std::is_same_v<T, float> ||
                       std::is_same_v<T, double>)
      value = arg;
    else if constexpr (std::is_same_v<T, const char*>)
      value = arg;
    else if constexpr (std::is_same_v<T, std::string>)
      value = arg;
    else
      static_assert (sizeof (T) < 0, "unimplemented InitialVal type");
  }, iv);
  return value;
}

Parameter::Parameter (const Param &initparam)
{
  const Param &p = initparam;
  cident = !p.ident.empty() ? string_to_ncname (p.ident) : string_to_ncname (p.label, '_');
  details_ = p.details;
  const auto choicesfuncp = std::get_if<ChoicesFunc> (&p.extras);
  MinMaxStep range;
  if (const auto rangep = std::get_if<MinMaxStep> (&p.extras))
    range = *rangep;
  const auto [fmin, fmax, step] = range;
  if (!p.label.empty())
    store ("label", p.label);
  if (!p.nick.empty())
    store ("nick", p.nick);
  if (!p.unit.empty())
    store ("unit", p.unit);
  if (!p.blurb.empty())
    store ("blurb", p.blurb);
  if (!p.descr.empty())
    store ("descr", p.descr);
  if (!p.group.empty())
    store ("group", p.group);
  const auto choicesp = std::get_if<ChoiceS> (&p.extras);
  bool isbool = false;
  if (choicesfuncp)
    extras_ = *choicesfuncp;
  else if (choicesp)
    extras_ = *choicesp;
  else if (fmin != fmax)
    extras_ = range;
  else
    extras_ = minmaxstep_from_initialval (p.initial, &isbool);
  initial_ = value_from_initialval (p.initial);
  String hints = p.hints.empty() ? STANDARD : p.hints;
  String choice = choicesp || choicesfuncp ? "choice" : "";
  String text = choicesfuncp || initial_.is_string() ? "text" : "";
  String dynamic = choicesfuncp ? "dynamic" : "";
  String stepped = isbool ? "stepped" : "";
  store ("hints", construct_hints (p.hints, text + ":" + choice + ":" + dynamic + ":" + stepped, fmin, fmax));
}

String
Parameter::value_to_text (const Value &value) const
{
  if (is_choice())
    return constrain (value).as_string();
  const Value::Type type = value.index();
  if (type != Value::BOOL && type != Value::INT64 && type != Value::DOUBLE)
    return value.as_string();
  double val = value.as_double();
  String unit = this->unit();
  if (unit == "Hz" && fabs (val) >= 1000)
    {
      unit = "kHz";
      val /= 1000;
    }
  int fdigits = 2; // fractional digits
  if (fabs (val) < 10)
    fdigits = 2;
  else if (fabs (val) < 100)
    fdigits = 1;
  else if (fabs (val) < 1000)
    fdigits = 0;
  else
    fdigits = 0;
  const auto [fmin, fmax, step] = range();
  const bool need_sign = fmin < 0;
  String s = need_sign ? string_format ("%+.*f", fdigits, val) : string_format ("%.*f", fdigits, val);
  if (fdigits == 0 && fabs (val) == 100 && unit == "%")
    s += "."; // use '100. %' for fixed with of percent numbers
  if (!unit.empty())
    s += " " + unit;
  return s;
}

Value
Parameter::value_from_text (const String &text) const
{
  if (is_choice()) {
    const ChoiceS choices = this->choices();
    return int64_t (match_choice (choices, text));
  }
  if (is_text())
    return constrain (text).as_string();
  return constrain (string_to_double (text));
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
    if (e >= 0 && isdigit (words[i][e + 1]))                    // A77
      return { words[0].substr (0, 1), words[i].substr (e, 2), "" };
    if (e >= 0 && i + 1 < words.size())                         // A7G
      return { words[0].substr (0, 1), words[i].substr (e, 1), words[i+1].substr (0, 1) };
    if (e > 0)                                                  // AF7
      return { words[0].substr (0, 1), words[i].substr (0, 1), words[i].substr (e, 1) };
    if (e == 0 && i >= 3)                                       // AE7
      return { words[0].substr (0, 1), words[i-1].substr (0, 1), words[i].substr (e, 1) };
    if (e == 0 && i >= 2)                                       // AB7
      return { words[0].substr (0, 1), words[1].substr (0, 1), words[i].substr (e, 1) };
    if (e == 0)                                                 // Aa7
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

/// Create a few letter nick name from a multi word parameter label.
String
parameter_guess_nick (const String &parameter_label)
{
  // seperate numbers from words, increases word count
  String string = spaced_nums (parameter_label);

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
