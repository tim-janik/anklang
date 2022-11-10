---
title:      ANKLANG Development Details
author:     The Anklang Project <[anklang.testbit.eu](https://anklang.testbit.eu/)>
abstract:   API documentation and development internals of the Anklang project.
keywords:   Anklang, Linux, ALSA, JavaScript, Scripting, IPC
---

# ANKLANG Development Details

Technically, Anklang consists of a user interface front-end based on web technologies (HTML, SCSS, JS, Vue)
and a synthesis engine backend written in C++.
The synthesis engine can load various audio rendering plugins which are executed in audio rendering worker
threads.
The main synthesis engine thread coordinates synchronization and interafces between the engine and the UI
via an IPC interface over a web-socket that uses remote method calls and event delivery marshalled as JSON messages.

## Jsonipc

Jsonipc is a header-only IPC layer that marshals C++ calls to JSON messages defined in
[jsonipc/jsonipc.hh](https://github.com/tim-janik/anklang/blob/master/jsonipc/jsonipc.hh).
The needed registration code is very straight forward to write manually, but can also be
auto-genrated by using
[jsonipc/cxxjip.py](https://github.com/tim-janik/anklang/blob/master/jsonipc/cxxjip.py) which
parses the exported API using
[CastXML](https://github.com/CastXML/CastXML).

The Anklang API for remote method calls is defined in
[api.hh](https://github.com/tim-janik/anklang/blob/master/ase/api.hh).
Each class with its methods, struct with its fields and enum with its values is registered
as a Jsonipc interface using conscise C++ code that utilizes templates to derive the needed type information.

The corresponding Javascript code to use `api.hh` via
[async](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Statements/async_function)
remote method calls is generated via `Jsonipc::ClassPrinter::to_string()` by
`AnklangSynthEngine --js-api`.

* [√] `shared_ptr<Class> from_json()` - lookup by id in InstanceMap or use `Scope::make_shared` for Serializable.
* [√] `to_json (const shared_ptr<Class> &p)` - marshal Serializable or {id} from InstanceMap.
* [√] `Class* from_json()` - return `&*shared_ptr<Class>`
* [√] `to_json (Class *r)` - supports Serializable or `Class->shared_from_this()` wrapping.
* [√] `Class& from_json()` - return `*shared_ptr<Class>`, throws on `nullptr`. **!!!**
* [√] `to_json (const Class &v)` - return `to_json<Class*>()`
* [√] No uses are made of copy-ctor implementations.
* [√] Need virtual ID serialization API on InstanceMap.
* [√] Add `jsonvalue_as_string()` for debugging purposes.

### Callback Handling

Javascript can register/unregister remote Callbacks with *create* and *remove*.
C++ sends events to inform about a remote Callback being *called* or unregistered *killed*.

```cxx
void 	Jsonapi/Trigger/create (id);     // JS->C++
void 	Jsonapi/Trigger/remove (id);     // JS->C++
void 	Jsonapi/Trigger/_<id>  ([...]);  // C++->JS
void 	Jsonapi/Trigger/killed (id);     // C++->JS
```

## Serialization

Building on [Jsonipc](#jsonipc), a small serializaiton framework provided by
[ase/serialize.hh](https://github.com/tim-janik/anklang/blob/master/ase/serialize.hh)
is used to marshal values, structs, enums and classes to/from JSON.
This is used to store preferences and project data.
The intended usage is as follows:

```cxx
  std::string jsontext = Ase::json_stringify (somevalue);
  bool success = Ase::json_parse (jsontext, somevalue);
  // The JSON root will be of type 'object' if somevalue is a class instance
  std::string s;                                          // s contains:
  s = json_stringify (true);                              // true
  s = json_stringify (-0.17);                             // -0.17
  s = json_stringify (32768);                             // 32768
  s = json_stringify (Ase::Error::IO);                    // "Ase.Error.IO"
  s = json_stringify (String ("STRing"));                 // "STRing"
  s = json_stringify (ValueS ({ true, 5, "HI" }));        // [true,5,"HI"]
  s = json_stringify (ValueR ({ {"a", 1}, {"b", "B"} })); // {"a":1,"b":"B"}
```

In the above examples, `Ase::Error::IO` can be serialized because it is registered as
`Jsonipc::Enum<Ase::Error>` with its enum values. The same works for serializable
classes registered through `Jsonipc::Serializable<SomeClass>`.

[_] Serialization of class instances will have to depend on the Scope/InstanceMap, so
instance pointers in copyable classes registered as `Jsonipc::Serializable<>` can be
marshalled into a JsonValue (as `{$id,$class}` pair), then be resolved into an
InstanceP stored in an Ase::Value and from there be marshalled into an persistent
relative object link for project data storage.
