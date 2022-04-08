// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Note, as of March 2022, Firefox lacks ES Worker Module support: https://bugzilla.mozilla.org/show_bug.cgi?id=1247687

// == utilities ==
// Log message to console with prefix
globalThis.name = globalThis.name || 'host.js';
globalThis.log = (...args) => console.log (globalThis.name + ':', ...args);
Object.defineProperty (globalThis, 'log', { value: globalThis.log });

// Await and reassign all object fields.
async function object_await_values (obj) {
  for (const key of Object.keys (obj))
    obj[key] = await obj[key];
  return obj;
}

// Send remote call, handle reply via _promises
async function host_call (fun, ...args) {
  const callid = _counter++;
  postMessage ({ fun, args, callid: callid });
  return new Promise (r => _promises.set (callid, r));
}
let _counter = 1;

// Handle incoming messages by resolving _promises entries
async function host_receive (event) {
  const d = event.data;
  const resolve = d.resultid > 0 ? _promises.get (d.resultid) : null;
  // handle call: ScriptHost funid -call-> WorkerHost
  if (d.callid > 0 && d.funid) {
    const fun = host_globals.registry.get (d.funid);
    if (!fun)
      throw new Error (globalThis.name + ': host.js: Attempt to call unregistered funid: ' + d.funid);
    const result = await fun (...(d.args ? d.args : []));
    postMessage ({ resultid: d.callid, result });
    return;
  }
  // handle result: ScriptHost -result-> WorkerHost
  if (resolve) {
    _promises.delete (d.resultid);
    resolve (d.result);
    return;
  }
  // complain
  log ('host.js:', 'Unknown message event:', event);
}
addEventListener ('message', host_receive);
const _promises = new Map();

// Relay uncaught promise rejections to ScriptHost as Error
function unhandled_rejection (event) {
  if (event.type === 'unhandledrejection') {
    event.toString = () => 'Promise rejection' + ': ' + (event.reason || event.message);
    throw event;
  } // avoid .preventDefault to preserve DevTools stacktrace
}
globalThis.onunhandledrejection = unhandled_rejection;

// Log uncaught exceptions, leave handling to ScriptHost
function unhandled_error (event, filename, lineno, colno, error) {
  if (error?.type === 'unhandledrejection')
    return; // thrown by unhandled_rejection()
  if (!error && filename && lineno)
    console.error (filename.replace (/.*\//, '') + ':' + lineno + ':', error || event);
  else
    console.error (globalThis.name + ':', error || event);
}
globalThis.onerror = unhandled_error;

// Allow releasing of remotely held refhandle references
function refhandle_finalization_handler (held_value) {
  host_call ('release_refhandle', held_value);
}
const refhandle_finalization = new FinalizationRegistry (refhandle_finalization_handler);

/// Ase::Clip proxy
class WorkerClip {
  constructor (handle) {
    this._handle = handle;
    refhandle_finalization.register (this, handle, this);
  }
  async list_all_notes () {
    return host_call ('clip_list_all_notes', this._handle);
  }
  async list_selected_notes () {
    const notes = await this.list_all_notes();
    return notes.filter (n => n.selected);
  }
  async insert_note (channel, key, tick, duration, velocity = 127, fine_tune = 0, selected = false) {
    return host_call ('clip_insert_note', this._handle, channel, key, tick, duration, velocity, fine_tune, selected);
  }
}

/// Global `host` instance for scripts.
class WorkerHost {
  /// Create handle for the currently selected clip.
  async piano_roll_clip () {
    return new WorkerClip (await host_call ('piano_roll_clip_refhandle'));
  }
  /// Retrieve the file name of the current user script.
  script_name() { return host_globals.script_name; }
  /// Retrieve the UUID set via #use_api().
  script_uuid() { return host_globals.script_uuid; }
  /// Retrieve numeric API level supported by this runtime.
  api_level() { return host_globals.api_level; }
  /// Request use of API level `api_level` and provide UUID for controllers.
  async use_api (api_level, script_uuid) {
    if (!(api_level <= this.api_level()))
      throw ('Unknown API level (supported=' + this.api_level() + '):', api_level);
    if (script_uuid && !host_globals.script_uuid) {
      host_globals.script_uuid = script_uuid;
      log ("UUID:", script_uuid);
      await host_call ('set_uuid', script_uuid);
    }
  }
  /// Register a script function.
  async register (category, label, fun, blurb, params = {}) {
    const funid = _counter++ + '@' + host_globals.script_uuid;
    host_globals.registry.set (funid, fun);
    await host_call ('register', category, label, funid, blurb, params);
  }
}

const string_to_identifier = s => s.toLowerCase().replace (/[^a-z_0-9]/g, '_').replace (/^([0-9])/, '_$1');

// setup global functions / constructors
globalThis.Text = (label, nick, dflt = "", blurb = "", description = "", hints = ':G:r:w:') => ({ label, nick, dflt, hints: hints + ':text:', blurb, description });
globalThis.Choices = (label, nick, choices, dflt = "", blurb = "", description = "", hints = ':G:r:w:') => ({ label, nick, choices, dflt, hints: hints + 'text:choice:', blurb, description });
globalThis.Choice = (ident, label = "", blurb = "") => ({ ident: string_to_identifier (ident), label: label || ident, blurb, });
globalThis.Bool = (label, nick, dflt = false, blurb = "", description = "", hints = ':G:r:w:') => ({ label, nick, dflt, hints: hints + ':bool:', blurb, description });
globalThis.Range = (label, nick, min, max, dflt, unit = "", blurb = "", description = "", hints = ':G:r:w:') =>
  ({ label, nick, min, max, dflt, unit, hints: hints + ':range:', blurb, description });

// setup global `host` and its members
globalThis.host = new WorkerHost();
Object.defineProperty (globalThis, 'host', { value: host });
const host_globals = {
  api_level:	host_call ('api_level'),
  script_name:	host_call ('script_name'),
  script_uuid: undefined,
  registry:	new Map(),
};
await object_await_values (host_globals);

// == User Script ==
let user_module = import (host.script_name());
user_module = await user_module;
user_module;
