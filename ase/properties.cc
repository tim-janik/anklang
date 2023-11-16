// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "properties.hh"
#include "jsonipc/jsonipc.hh"
#include "strings.hh"
#include "utils.hh"
#include "main.hh"
#include "path.hh"
#include "serialize.hh"
#include "internal.hh"

#define PDEBUG(...)     Ase::debug ("prefs", __VA_ARGS__)

namespace Ase {

// == Property ==
Property::~Property()
{}

// == PropertyImpl ==
PropertyImpl::PropertyImpl (const Param &param, const PropertyGetter &getter,
                            const PropertySetter &setter, const PropertyLister &lister) :
  getter_ (getter), setter_ (setter), lister_ (lister)
{
  parameter_ = std::make_shared<Parameter> (param);
}

// == PropertyBag ==
void
PropertyBag::operator+= (const Prop &prop) const
{
  add_ (prop, group);
}

// == Preference ==
using PrefsValueCallbackList = CallbackList<CString,const Value&>;
struct PrefsValue {
  ParameterC parameter;
  Value      value;
  std::shared_ptr<PrefsValueCallbackList> callbacks;
};
using PrefsMap = std::unordered_map<CString,PrefsValue>;

static std::shared_ptr<CallbackList<CStringS>> prefs_callbacks = CallbackList<CStringS>::make_shared();
static CStringS notify_preference_queue;

static PrefsMap&
prefs_map()
{
  static PrefsMap *const prefsmap = []() { return new PrefsMap(); } ();
  return *prefsmap;
}

static bool preferences_autosave = false;
static uint timerid_maybe_save_preferences = 0;

static void
maybe_save_preferences()
{
  main_loop->clear_source (&timerid_maybe_save_preferences);
  if (preferences_autosave && notify_preference_queue.empty())
    Preference::save_preferences();
}

static void
notify_preference_listeners ()
{
  CStringS changed_prefs (notify_preference_queue.begin(), notify_preference_queue.end()); // converts CStringS to StringS
  notify_preference_queue.clear(); // clear, queue taken over
  return_unless (!changed_prefs.empty());
  std::sort (changed_prefs.begin(), changed_prefs.end()); // sort and dedup
  changed_prefs.erase (std::unique (changed_prefs.begin(), changed_prefs.end()), changed_prefs.end());
  // emit "notify" on individual preferences
  PrefsMap &prefsmap = prefs_map();
  for (auto cident : changed_prefs) {
    PrefsValue &pv = prefsmap[cident];
    (*pv.callbacks) (cident, pv.value);
  }
  // notify preference list listeners
  const auto callbacklist = prefs_callbacks; // keep reference around invocation
  (*callbacklist) (changed_prefs);
  if (preferences_autosave)
    main_loop->exec_once (577, &timerid_maybe_save_preferences, maybe_save_preferences);
  else
    main_loop->clear_source (&timerid_maybe_save_preferences);
}

static void
queue_notify_preference_listeners (const CString &cident)
{
  return_unless (main_loop != nullptr);
  // enqueue idle handler for accumulated preference change notifications
  const bool need_enqueue = notify_preference_queue.empty();
  notify_preference_queue.push_back (cident);
  if (need_enqueue)
    main_loop->exec_now (notify_preference_listeners);
}

Preference::Preference (ParameterC parameter)
{
  parameter_ = parameter;
  PrefsMap &prefsmap = prefs_map(); // Preference must already be registered
  auto it = prefsmap.find (parameter_->cident);
  assert_return (it != prefsmap.end());
  PrefsValue &pv = it->second;
  sigh_ = pv.callbacks->add_delcb ([this] (const String &ident, const Value &value) { emit_event ("notify", ident); });
}

Preference::Preference (const Param &param, const StringValueF &cb)
{
  assert_return (param.ident.empty() == false);
  parameter_ = std::make_shared<Parameter> (param);
  PrefsMap &prefsmap = prefs_map();
  PrefsValue &pv = prefsmap[parameter_->cident];
  assert_return (pv.parameter == nullptr); // catch duplicate registration
  pv.parameter = parameter_;
  pv.value = pv.parameter->initial();
  pv.callbacks = PrefsValueCallbackList::make_shared();
  sigh_ = pv.callbacks->add_delcb ([this] (const String &ident, const Value &value) { emit_event ("notify", ident); });
  queue_notify_preference_listeners (parameter_->cident);
  if (cb) {
    Connection connection = on_event ("notify", [this,cb] (const Event &event) { cb (this->parameter_->cident, this->get_value()); });
    connection_ = new Connection (connection);
  }
}

Preference::~Preference()
{
  if (connection_) {
    connection_->disconnect();
    delete connection_;
    connection_ = nullptr;
  }
  if (sigh_)
    sigh_(); // delete signal handler callback
}

Value
Preference::get_value ()
{
  PrefsMap &prefsmap = prefs_map();
  PrefsValue &pv = prefsmap[parameter_->cident];
  return pv.value;
}

bool
Preference::set_value (const Value &v)
{
  PrefsMap &prefsmap = prefs_map();
  PrefsValue &pv = prefsmap[parameter_->cident];
  Value next = parameter_->constrain (v);
  const bool changed = next == pv.value;
  pv.value = std::move (next);
  queue_notify_preference_listeners (parameter_->cident); // delayed
  return changed;
}

Value
Preference::get (const String &ident)
{
  const CString cident = CString::lookup (ident);
  return_unless (!cident.empty(), {});
  PrefsMap &prefsmap = prefs_map();
  auto it = prefsmap.find (cident);
  return_unless (it != prefsmap.end(), {});
  PrefsValue &pv = it->second;
  return pv.value;
}

PreferenceP
Preference::find (const String &ident)
{
  const CString cident = CString::lookup (ident);
  return_unless (!cident.empty(), {});
  PrefsMap &prefsmap = prefs_map();
  auto it = prefsmap.find (cident);
  return_unless (it != prefsmap.end(), {});
  PrefsValue &pv = it->second;
  return Preference::make_shared (pv.parameter);
}

CStringS
Preference::list ()
{
  CStringS strings;
  for (const auto &e : prefs_map())
    strings.push_back (e.first);
  std::sort (strings.begin(), strings.end());
  return strings;
}

Preference::DelCb
Preference::listen (const std::function<void(const CStringS&)> &func)
{
  return prefs_callbacks->add_delcb (func);
}

static String
pathname_anklangrc()
{
  static const String anklangrc = Path::join (Path::config_home(), "anklang", "anklangrc.json");
  return anklangrc;
}

void
Preference::load_preferences (bool autosave)
{
  const String jsontext = Path::stringread (pathname_anklangrc());
  ValueR precord;
  json_parse (jsontext, precord);
  for (ValueField vf : precord) {
    PreferenceP pref = find (vf.name);
    PDEBUG ("%s: %s %s=%s\n", __func__, pref ? "loading" : "ignoring", vf.name, vf.value->repr());
    if (pref)
      pref->set_value (*vf.value);
  }
  preferences_autosave = autosave;
}

void
Preference::save_preferences ()
{
  ValueR precord;
  for (auto ident : list())
    precord[ident] = get (ident);
  const String new_jsontext = json_stringify (precord, Writ::RELAXED | Writ::SKIP_EMPTYSTRING) + "\n";
  const String cur_jsontext = Path::stringread (pathname_anklangrc());
  if (new_jsontext != cur_jsontext) {
    PDEBUG ("%s: %s\n", __func__, precord.repr());
    Path::stringwrite (pathname_anklangrc(), new_jsontext, true);
  }
}

} // Ase
