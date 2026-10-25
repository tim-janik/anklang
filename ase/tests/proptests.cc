// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <ase/project.hh>
#include <ase/jsonapi.hh>
#include <ase/testing.hh>

namespace { // Anon
using namespace Ase;

// Properties that fail the loop, with the reason. A listed property that passes fails the test.
static const std::map<String,String> exempt_properties = {
  { "Project.is_playing",  "Transport state, playback has no undo." },
  { "Track.muted",         "Tracktion's AudioTrack binds IDs::mute without the undo manager." },
  { "Track.solo",          "Tracktion's AudioTrack binds IDs::solo without the undo manager." },
};

using Values = std::map<String,String>;
// Undo can recreate wrappers; compare values by position and notifications by live instance.
struct Snapshot { Values values, properties; };
static const String test_note = "{\"note_number\":60,\"start_beat\":0.25,\"length_beats\":0.5,\"velocity\":90}";

static void
normalize_refs (Jsonipc::JsonValue &value)
{
  if (value.IsArray())
    for (auto &element : value.GetArray())
      normalize_refs (element);
  else if (value.IsObject())
    {
      for (auto &member : value.GetObject())
        if (member.name == "$id")
          member.value.SetInt (0);
        else
          normalize_refs (member.value);
    }
}

// A fresh project whose objects are read and written by name through Jsonipc, like the UI does.
struct LoopProject {
  struct Object { String label, self; StringS props, read_only; bool assign; };
  Jsonipc::InstanceMap imap;
  Jsonipc::IpcDispatcher dispatcher;
  ProjectImplP project = ProjectImpl::create ("PropertyLoop");
  ClipP audio_clip;
  std::vector<Object> objects;
  std::set<String> notified;
  std::map<String,String> property_labels;
  std::vector<Emittable::Connection> connections;
  LoopProject()
  {
    TrackP track = project->create_track();
    ClipP midi_clip = track->create_midi_clip ("MidiClip", 1, 2);
    audio_clip = track->create_audio_clip ("AudioClip", 4, 2);
    PluginP plugin = track->create_plugin ("4osc");
    PluginP text_plugin = track->create_plugin ("text");
    PluginP insert_plugin = track->create_plugin ("insert");
    add ("Project", ProjectP (project));
    add ("MasterTrack", project->master_track());
    add ("Track", track);
    add ("MidiClip", midi_clip);
    add ("AudioClip", audio_clip);
    add ("Plugin", plugin);
    add ("TextPlugin", text_plugin);
    add ("InsertPlugin", insert_plugin);
    for (TrackP t : project->all_tracks())
      {
        add (t->is_control_track() && t->name() == "Chord" ? "ChordTrack" : "", t);
        for (ClipP c : t->launcher_clips())
          add ("", c);
        for (PluginP p : t->list_plugins())
          add ("", p);
      }
    call ("change_batch", ref ("MidiClip"), "[" + test_note + "],\"Initial Note\"");
    project->clear_undo();
  }
  ~LoopProject()
  {
    project->discard();
  }
  // Add `obj` with its get/set pairs, properties of objects without label are only compared.
  template<class T> const Object&
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
        return o;
    Jsonipc::InstanceMap::Wrapper *wrapper = Jsonipc::InstanceMap::scope_lookup_wrapper (ref);
    if (label.empty())
      label = Jsonipc::short_name (wrapper->classname()) + "#" + std::to_string (objects.size());
    const StringS names = wrapper->list_methods();
    StringS props;
    for (const String &name : names)
      if (string_startswith (name, "get/") && std::count (names.begin(), names.end(), "set/" + name.substr (4)))
        props.push_back (name.substr (4));
    connections.push_back (obj->on_event ("notify", [this, self] (const Event &event) {
      notified.insert (self + "." + event.detail());
    }));
    objects.push_back ({ label, self, props, obj->read_only_properties(), assign });
    return objects.back();
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
  void
  read (Snapshot &snapshot, const String &label, const String &self, const String &prop, const String &method, bool notify = false)
  {
    const String key = self + "." + prop, value = call (method, self);
    property_labels[key] = label + "." + prop;
    if (notify)
      snapshot.properties[key] = value;
    rapidjson::Document doc;
    doc.Parse (value.c_str());
    normalize_refs (doc);
    snapshot.values[label + "." + prop] = Jsonipc::jsonvalue_to_string (doc);
  }
  template<class T> String
  read_object (Snapshot &snapshot, const String &label, const std::shared_ptr<T> &obj)
  {
    const Object &o = add ("", obj);
    for (const auto &p : o.props)
      read (snapshot, label, o.self, p, "get/" + p, true);
    return o.self;
  }
  Snapshot
  snapshot()
  {
    Snapshot values;
    read_object (values, "Project", ProjectP (project));
    read (values, "Project", ref ("Project"), "all_tracks", "all_tracks");
    size_t ti = 0;
    for (const auto &track : project->all_tracks())
      {
        const String label = string_format ("Track[%u]", uint (ti++));
        const String self = read_object (values, label, track);
        read (values, label, self, "list_plugins", "list_plugins");
        size_t ci = 0, pi = 0;
        for (const auto &clip : track->launcher_clips())
          {
            const String clabel = string_format ("%s.Clip[%u]", label, uint (ci++));
            const String cself = read_object (values, clabel, clip);
            read (values, clabel, cself, "notes", "list_all_notes", true);
          }
        for (const auto &plugin : track->list_plugins())
          read_object (values, string_format ("%s.Plugin[%u]", label, uint (pi++)), plugin);
      }
    read_object (values, "AudioClip", audio_clip);
    return values;
  }
};

using Change = std::function<void (LoopProject&)>;
static const std::map<String,Change> property_changes = {
  { "MidiClip.all_notes", [] (LoopProject &lp) {
    lp.call ("set/all_notes", lp.ref ("MidiClip"), "[{\"note_number\":67,\"length_beats\":1,\"velocity\":45}]");
  } },
  { "MidiClip.position", [] (LoopProject &lp) {
    lp.call ("set/position", lp.ref ("MidiClip"), "{\"time\":{\"start\":1.25,\"end\":3.25},\"offset\":0.5}");
  } },
  { "AudioClip.position", [] (LoopProject &lp) {
    lp.call ("set/position", lp.ref ("AudioClip"), "{\"time\":{\"start\":4.25,\"end\":6.25},\"offset\":0.5}");
  } },
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
    if (!b.count (k) || b.at (k) != v)
      keys.push_back (k);
  for (const auto &[k, v] : b)
    if (!a.count (k))
      keys.push_back (k);
  return keys;
}

static StringS
check_undo (LoopProject &lp, const Snapshot &before, const Snapshot &after)
{
  StringS problems;
  auto require_notifies = [&] (const char *step, const Snapshot &a, const Snapshot &b) {
    for (const auto &[key, value] : a.properties)
      if (b.properties.count (key) && b.properties.at (key) != value && !lp.notified.count (key))
        problems.push_back (string_format ("%s: %s changed without notify", step, lp.property_labels.at (key)));
  };
  if (before.values == after.values)
    problems.push_back ("set: no effect");
  if (!lp.project->can_undo())
    problems.push_back ("set: adds no undo step");
  require_notifies ("set", before, after);
  lp.notified.clear();
  lp.project->undo();
  if (lp.project->can_undo())
    problems.push_back ("undo: more than one undo step");
  const Snapshot undone = lp.snapshot();
  for (const auto &k : changed (before.values, undone.values))
    problems.push_back ("undo: " + k + " not restored");
  require_notifies ("undo", after, undone);
  lp.notified.clear();
  lp.project->redo();
  if (lp.project->can_redo())
    problems.push_back ("redo: more than one redo step");
  const Snapshot redone = lp.snapshot();
  for (const auto &k : changed (after.values, redone.values))
    problems.push_back ("redo: " + k + " not re-applied");
  require_notifies ("redo", undone, redone);
  return problems;
}

// Assign the property `key` in a fresh project, undo and redo, return the problems.
static StringS
check_property (const String &key, int dir = +1)
{
  LoopProject lp;
  const String label = key.substr (0, key.find ('.')), prop = key.substr (key.find ('.') + 1);
  const Snapshot before = lp.snapshot();
  const String target = lp.ref (label) + "." + prop;
  const String old = before.properties.at (target), value = nudge (old, dir);
  lp.notified.clear();
  if (property_changes.count (key))
    property_changes.at (key) (lp);
  else
    lp.call ("set/" + prop, lp.ref (label), value);
  const Snapshot after = lp.snapshot();
  if (after.properties.at (target) == old && dir > 0)
    {
      StringS problems = check_property (key, -1);
      for (const auto &k : changed (before.values, after.values))
        problems.push_back (string_format ("set %s: changes %s, the property stays %s", value, k, old));
      return problems;
    }
  StringS problems = check_undo (lp, before, after);
  if (after.properties.at (target) == old)
    problems.push_back ("set: no effect, still " + old);
  return problems;
}

static void
property_loop()
{
  StringS failures, checks;
  {
    LoopProject lp;
    for (const auto &o : lp.objects)
      {
        for (const auto &p : o.read_only)
          if (!std::count (o.props.begin(), o.props.end(), p))
            failures.push_back (o.label + "." + p + ": read-only, but there is no such property");
        for (const auto &p : o.props)
          {
            const String key = o.label + "." + p;
            // Read-only properties are only compared.
            if (o.assign && !std::count (o.read_only.begin(), o.read_only.end(), p))
              {
                if (property_changes.count (key) || !nudge (lp.call ("get/" + p, o.self), +1).empty())
                  checks.push_back (key);
                else
                  failures.push_back (key + ": add a test lambda to property_changes");
              }
          }
      }
  }
  for (const auto &[key, reason] : exempt_properties)
    if (!std::count (checks.begin(), checks.end(), key))
      failures.push_back (key + ": not assigned, remove it from exempt_properties");
  for (const auto &[key, change] : property_changes)
    if (!std::count (checks.begin(), checks.end(), key))
      failures.push_back (key + ": not assigned, remove it from property_changes");
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
