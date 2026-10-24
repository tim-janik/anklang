// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <ase/project.hh>
#include <ase/jsonapi.hh>
#include <ase/testing.hh>

namespace { // Anon
using namespace Ase;

// Read-only properties, the loop doesn't assign them.
static const std::set<String> read_only_properties = {
  "AudioClip.end_beat",
  "AudioClip.start_beat",
  "MidiClip.end_beat",
  "MidiClip.start_beat",
};

// Properties that fail the loop, with the reason. A listed property that passes fails the test.
static const std::map<String,String> exempt_properties = {
  { "MasterTrack.muted",   "Tracktion's MasterTrack has no mute, Track::setMute() does nothing." },
  { "MasterTrack.name",    "Tracktion's MasterTrack::getName() always returns \"Master\"." },
  { "MasterTrack.solo",    "Tracktion's MasterTrack has no solo, Track::setSolo() does nothing." },
  { "MidiClip.pan",        "Tracktion's MidiClip has no pan, ClipImpl::pan() ignores the value." },
  { "Plugin.name",         "Tracktion plugins have a fixed name, PluginImpl::name() ignores the value." },
  { "Project.is_playing",  "Transport state, playback has no undo." },
  { "Track.muted",         "Tracktion's AudioTrack binds IDs::mute without the undo manager." },
  { "Track.solo",          "Tracktion's AudioTrack binds IDs::solo without the undo manager." },
};

using Values = std::map<String,String>;

// A fresh project whose objects are read and written by name through Jsonipc, like the UI does.
struct LoopProject {
  struct Object { String label, self; StringS props; bool assign; };
  Jsonipc::InstanceMap imap;
  Jsonipc::IpcDispatcher dispatcher;
  ProjectImplP project = ProjectImpl::create ("PropertyLoop");
  std::vector<Object> objects;
  std::set<String> notified; // e.g. "Track.volume_db"
  std::vector<Emittable::Connection> connections;
  LoopProject()
  {
    TrackP track = project->create_track();
    ClipP midi_clip = track->create_midi_clip ("MidiClip", 1, 2);
    ClipP audio_clip = track->create_audio_clip ("AudioClip", 4, 2);
    PluginP plugin = track->create_plugin ("4osc");
    project->clear_undo();
    add ("Project", ProjectP (project));
    add ("MasterTrack", project->master_track());
    add ("Track", track);
    add ("MidiClip", midi_clip);
    add ("AudioClip", audio_clip);
    add ("Plugin", plugin);
    for (TrackP t : project->all_tracks())
      {
        add ("", t);
        for (ClipP c : t->launcher_clips())
          add ("", c);
        for (PluginP p : t->list_plugins())
          add ("", p);
      }
  }
  ~LoopProject()
  {
    project->discard();
  }
  // Add `obj` with its get/set pairs, properties of objects without label are only compared.
  template<class T> void
  add (String label, const std::shared_ptr<T> &obj)
  {
    TASSERT (obj);
    const bool assign = !label.empty();
    Jsonipc::Scope scope (imap);
    rapidjson::Document doc;
    const Jsonipc::JsonValue ref = Jsonipc::to_json (obj, doc.GetAllocator());
    const String self = Jsonipc::jsonvalue_to_string (ref);
    for (const auto &o : objects)
      if (o.self == self)
        return;
    Jsonipc::InstanceMap::Wrapper *wrapper = Jsonipc::InstanceMap::scope_lookup_wrapper (ref);
    if (label.empty())
      label = Jsonipc::short_name (wrapper->classname()) + "#" + std::to_string (objects.size());
    const StringS names = wrapper->list_methods();
    StringS props;
    for (const String &name : names)
      if (string_startswith (name, "get/") && std::count (names.begin(), names.end(), "set/" + name.substr (4)))
        props.push_back (name.substr (4));
    connections.push_back (obj->on_event ("notify", [this, label] (const Event &event) {
      notified.insert (label + "." + event.detail());
    }));
    objects.push_back ({ label, self, props, assign });
  }
  // Call a method by name, e.g. {"id":1,"method":"get/bpm","params":[{"$id":1}]}
  String
  call (const String &method, const String &self, const String &arg = "")
  {
    Jsonipc::Scope scope (imap);
    const String msg = "{\"id\":1,\"method\":\"" + method + "\",\"params\":[" + self + (arg.empty() ? "" : "," + arg) + "]}";
    const String reply = dispatcher.dispatch_message (msg);
    rapidjson::Document doc;
    doc.Parse (reply.c_str());
    if (doc.HasParseError() || !doc.HasMember ("result"))
      printerr ("%s: %s\n", msg, reply);
    TASSERT (!doc.HasParseError() && doc.HasMember ("result"));
    return Jsonipc::jsonvalue_to_string (doc["result"]);
  }
  String
  ref (const String &label) const
  {
    for (const auto &o : objects)
      if (o.label == label)
        return o.self;
    return "";
  }
  Values
  snapshot()
  {
    Values values;
    for (const auto &o : objects)
      for (const auto &p : o.props)
        values[o.label + "." + p] = call ("get/" + p, o.self);
    return values;
  }
};

// Another value of the same JSON type, "" for lists and objects.
static String
nudge (const String &v, int dir)
{
  if (v == "true")
    return "false";
  if (v == "false")
    return "true";
  if (!v.empty() && v[0] == '"')
    return v.substr (0, v.size() - 1) + "x\"";
  if (!v.empty() && (isdigit (v[0]) || v[0] == '-'))
    return string_format ("%.17g", string_to_double (v) + dir);
  return "";
}

static StringS
changed (const Values &a, const Values &b)
{
  StringS keys;
  for (const auto &[k, v] : a)
    if (b.at (k) != v)
      keys.push_back (k);
  return keys;
}

// Assign the property `key` in a fresh project, undo and redo, return the problems.
static StringS
check_property (const String &key, int dir = +1)
{
  LoopProject lp;
  const String label = key.substr (0, key.find ('.')), prop = key.substr (key.find ('.') + 1);
  StringS problems;
  auto require_notifies = [&] (const char *step, const Values &a, const Values &b) {
    for (const auto &k : changed (a, b))
      if (!lp.notified.count (k))
        problems.push_back (string_format ("%s: %s changed without notify", step, k));
  };
  const Values before = lp.snapshot();
  const String old = before.at (key), value = nudge (old, dir);
  lp.notified.clear();
  lp.call ("set/" + prop, lp.ref (label), value);
  const Values after = lp.snapshot();
  if (after.at (key) == old && dir > 0)
    {
      // e.g. at its maximum, try the other way in a fresh project
      problems = check_property (key, -1);
      for (const auto &k : changed (before, after))
        problems.push_back (string_format ("set %s: changes %s, the property stays %s", value, k, old));
      return problems;
    }
  if (after.at (key) == old)
    problems.push_back ("set: no effect, still " + old);
  if (!lp.project->can_undo())
    problems.push_back ("set: adds no undo step");
  require_notifies ("set", before, after);
  lp.notified.clear();
  lp.project->undo();
  const Values undone = lp.snapshot();
  for (const auto &k : changed (before, undone))
    problems.push_back (string_format ("undo: %s is %s, not %s", k, undone.at (k), before.at (k)));
  require_notifies ("undo", after, undone);
  lp.notified.clear();
  lp.project->redo();
  const Values redone = lp.snapshot();
  for (const auto &k : changed (after, redone))
    problems.push_back (string_format ("redo: %s is %s, not %s", k, redone.at (k), after.at (k)));
  require_notifies ("redo", undone, redone);
  return problems;
}

static void
property_loop()
{
  StringS failures, keys, checks;
  {
    LoopProject lp;
    const Values values = lp.snapshot();
    for (const auto &o : lp.objects)
      for (const auto &p : o.props)
        if (o.assign)
          {
            const String key = o.label + "." + p;
            keys.push_back (key);
            // read-only properties, lists and objects are only compared
            if (!read_only_properties.count (key) && !nudge (values.at (key), +1).empty())
              checks.push_back (key);
          }
  }
  for (const auto &key : read_only_properties)
    if (!std::count (keys.begin(), keys.end(), key))
      failures.push_back (key + ": unknown, remove it from read_only_properties");
  for (const auto &[key, reason] : exempt_properties)
    if (!std::count (checks.begin(), checks.end(), key))
      failures.push_back (key + ": not assigned, remove it from exempt_properties");
  for (const auto &key : checks)
    {
      const StringS problems = check_property (key);
      if (!exempt_properties.count (key))
        for (const auto &p : problems)
          failures.push_back (key + ": " + p);
      else if (problems.empty())
        failures.push_back (key + ": passes, remove it from exempt_properties");
    }
  for (const auto &f : failures)
    printerr ("%s\n", f);
  TASSERT (failures.empty());
}
TEST_ADD (property_loop);

} // Anon
