// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include <ase/project.hh>
#include <ase/jsonapi.hh>
#include <ase/platform.hh>
#include <ase/path.hh>
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

static const std::map<String,Change> method_checks = {
  { "Project.create_track", [] (LoopProject &lp) { lp.call ("create_track", lp.ref ("Project")); } },
  { "Project.group_undo", [] (LoopProject &lp) {
    lp.call ("group_undo", lp.ref ("Project"), "\"Rename Clips\"");
    lp.call ("set/name", lp.ref ("MidiClip"), "\"Renamed MIDI\"");
    lp.call ("set/name", lp.ref ("AudioClip"), "\"Renamed Audio\"");
    lp.call ("ungroup_undo", lp.ref ("Project"));
  } },
  { "Track.create_midi_clip", [] (LoopProject &lp) {
    lp.call ("create_midi_clip", lp.ref ("Track"), "\"New MIDI Clip\",1,2");
  } },
  { "Track.create_audio_clip", [] (LoopProject &lp) {
    lp.call ("create_audio_clip", lp.ref ("Track"), "\"New Audio Clip\",8,2");
  } },
  { "Track.insert_wave_clip", [] (LoopProject &lp) {
    const String sample = anklang_runpath (RPath::SAMPLEDIR, "freepats-vorbis/Tone/000_Acoustic_Grand_Piano_acpiano_0.ogg");
    const String file = Path::join (lp.project->project_dir (true), "piano\"\\.ogg");
    TASSERT (Path::copy_file (sample, file));
    rapidjson::Document doc;
    const String path_json = Jsonipc::jsonvalue_to_string (Jsonipc::to_json (file, doc.GetAllocator()));
    lp.call ("insert_wave_clip", lp.ref ("Track"), "\"Wave Clip\"," + path_json + ",8");
  } },
  { "Track.create_plugin", [] (LoopProject &lp) { lp.call ("create_plugin", lp.ref ("Track"), "\"4osc\""); } },
  { "Gadget.remove_self/Track", [] (LoopProject &lp) { lp.call ("remove_self", lp.ref ("Track")); } },
  { "Gadget.remove_self/MidiClip", [] (LoopProject &lp) { lp.call ("remove_self", lp.ref ("MidiClip")); } },
  { "Gadget.remove_self/AudioClip", [] (LoopProject &lp) { lp.call ("remove_self", lp.ref ("AudioClip")); } },
  { "Gadget.remove_self/Plugin", [] (LoopProject &lp) { lp.call ("remove_self", lp.ref ("Plugin")); } },
};

static const std::map<String,String> exempt_methods = {
  { "Clip.change_batch",           "Special note-editing flow, deferred to its planned rewrite in #141." },
  { "Gadget.remove_self/Track",    "Audio clip handles cannot be reacquired on undo." },
  { "Gadget.remove_self/AudioClip", "Audio clips have no API list, so restored clips cannot be reacquired." },
  { "Gadget.remove_self/Plugin",   "Undo reuses the plugin's invalidated API handle." },
  { "Emittable.emit_event",        "Event delivery, not project data." },
  { "Emittable.emit_notify",       "Event delivery, not project data." },
  { "Emittable.js_trigger",        "Event subscription, not project data." },
  { "Gadget.type_nick",            "Type query." },
  { "Gadget.list_properties",      "Property metadata query." },
  { "Gadget.access_property",      "Property handle query." },
  { "Gadget.access_properties",    "Property handle query." },
  { "Gadget.get_value",            "Property value query." },
  { "Gadget.set_value",            "Uses Property handles; these objects expose no writable Property handles yet." },
  { "Gadget.read_only_properties", "Property metadata query." },
  { "Gadget.set_data",             "Session data, intentionally outside the Edit and undo." },
  { "Gadget.get_data",             "Session data query." },
  { "Device.device_info",          "Device metadata query." },
  { "Plugin.plugin_type",          "Plugin type query." },
  { "Clip.list_all_notes",         "Query, compared as notes after every change." },
  { "Clip.telemetry",              "Telemetry memory location query." },
  { "Track.is_master",             "Track type query." },
  { "Track.is_control_track",      "Track type query." },
  { "Track.list_plugins",          "Query, compared after every change." },
  { "Track.access_device",         "Device handle query, currently returns nullptr." },
  { "Track.create_monitor",        "Unimplemented, currently returns nullptr." },
  { "Track.telemetry",             "Telemetry memory location query." },
  { "Monitor.get_output",          "Output device query." },
  { "Monitor.get_ochannel",        "Output channel query." },
  { "Monitor.get_mix_freq",        "Sample rate query." },
  { "Monitor.get_frame_duration",  "Frame duration query." },
  { "Project.discard",             "Project lifetime, not an undoable edit." },
  { "Project.start_playback",      "Transport state, playback has no undo." },
  { "Project.pause_playback",      "Transport state, playback has no undo." },
  { "Project.stop_playback",       "Transport state, playback has no undo." },
  { "Project.master_track",        "Master track handle query." },
  { "Project.save_project",        "File I/O, covered by step 7." },
  { "Project.saved_filename",      "File name query." },
  { "Project.load_project",        "File I/O, covered by step 7." },
  { "Project.telemetry",           "Telemetry memory location query." },
  { "Project.group_undo",          "Clip setters and change_batch start their own transactions, splitting explicit undo groups." },
  { "Project.ungroup_undo",        "Undo history control, not a project mutation." },
  { "Project.undo",                "Undo history control, exercised by the loop." },
  { "Project.can_undo",            "Undo history query, exercised by the loop." },
  { "Project.redo",                "Undo history control, exercised by the loop." },
  { "Project.can_redo",            "Undo history query, exercised by the loop." },
  { "Project.match_serialized",    "Serialized state query." },
};

template<class T, class Base> static void
list_methods (const String &cls, std::set<String> &methods)
{
  const StringS inherited = Jsonipc::Class<Base>::list_methods();
  for (const auto &name : Jsonipc::Class<T>::list_methods())
    if (name.find ('/') == name.npos && !std::count (inherited.begin(), inherited.end(), name))
      methods.insert (cls + "." + name);
}

static void
method_loop()
{
  std::set<String> methods;
  // Server, Property and ResourceCrawler hold no project data and are exempt as classes.
  list_methods<Emittable, SharedBase> ("Emittable", methods);
  list_methods<Object, Emittable> ("Object", methods);
  list_methods<Gadget, Object> ("Gadget", methods);
  list_methods<Device, Gadget> ("Device", methods);
  list_methods<Project, Device> ("Project", methods);
  list_methods<Track, Device> ("Track", methods);
  list_methods<Clip, Gadget> ("Clip", methods);
  list_methods<Plugin, Device> ("Plugin", methods);
  list_methods<Monitor, Gadget> ("Monitor", methods);
  StringS failures;
  for (const auto &key : methods)
    if (!method_checks.count (key) && !exempt_methods.count (key) &&
        !std::any_of (method_checks.begin(), method_checks.end(), [&] (const auto &entry) {
          return string_startswith (entry.first, key + "/");
        }))
      failures.push_back (key + ": add a test lambda to method_checks or an exempt_methods line with a reason");
  for (const auto &[key, reason] : exempt_methods)
    if ((!methods.count (key) && !method_checks.count (key)) || reason.empty())
      failures.push_back (key + ": remove the stale exemption or give a reason");
  for (const auto &[key, change] : method_checks)
    {
      if (!methods.count (key.substr (0, key.find ('/'))) || !change)
        {
          failures.push_back (key + ": no such method or no test lambda");
          continue;
        }
      LoopProject lp;
      const Snapshot before = lp.snapshot();
      lp.notified.clear();
      change (lp);
      const Snapshot after = lp.snapshot();
      const StringS problems = check_undo (lp, before, after);
      if (!exempt_methods.count (key))
        for (const auto &problem : problems)
          failures.push_back (key + ": " + problem);
      else if (problems.empty())
        failures.push_back (key + ": passes, remove it from exempt_methods");
    }
  for (const auto &failure : failures)
    printerr ("%s\n", failure);
  TASSERT (failures.empty());
}
TEST_ADD (method_loop);

} // Anon
