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
function host_receive (event) {
  const d = event.data;
  const resolve = d.resultid > 0 ? _promises.get (d.resultid) : null;
  if (resolve) {
    _promises.delete (d.resultid);
    resolve (d.result);
  } else
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

/// Global `host` instance for scripts.
class WorkerHost {
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
  async register (label, fun, blurb) {
    const funid = _counter++;
    host_globals.registry.set (funid, fun);
    await host_call ('register', label, host_globals.script_uuid + '+' + funid, blurb);
  }
}

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
let user_module = import ('./scripts/' + host.script_name());
user_module = await user_module;
user_module;
