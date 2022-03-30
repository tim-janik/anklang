// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

const API_LEVEL = 22.03;
const log = (...args) => console.log ('script.js:', ...args);

/** A ScriptHost object represents a Worker script in the Main thread.
 *
 * A ScriptHost object (defined in `script.js`) runs in the Main Javascript
 * thread. It starts a Worker via `host.js` which creates a WorkerHost object
 * in the Worker thread. The ScriptHost<->WorkerHost objects bridge needed
 * API from the Main thread to the Worker thread.
 * The WorkerHost then loads and calls into a user provided ES Module, the
 * actual user script which communicates via the WorkerHost global variable
 * `host`. See also #WorkerHost.
 */
class ScriptHost {
  constructor (script_name) {
    console.assert (script_name);
    this.script_name = script_name;
    this.script_uuid = undefined;
    this.worker = new Worker ('/host.js', { type: 'module', name: this.script_name });
    this.worker.onerror = this.onerror.bind (this);
    this.worker.onmessage = this.onmessage.bind (this);
    this.registry = new Map();
  }
  terminate () {
    this.worker.terminate();
  }
  kill (...args) {
    console.error ('TERMINATE "' + this.script_name + '":', ...args);
    this.terminate();
  }
  onerror (err) {
    let loc = '';
    if (err.filename && err.lineno)
      loc = err.filename.replace (/.*\//, '') + ':' + err.lineno + ': ';
    this.kill (err.message ? loc + err.message : loc + err);
    this.terminate();
  }
  onmessage (msg) {
    const d = msg.data, fun = d.fun ? '_' + d.fun : null;
    if (d.callid > 0 && typeof this[fun] === 'function') {
      const result = this[fun] (...(d.args ? d.args : []));
      this.worker.postMessage ({ resultid: d.callid, result });
    } else
      console.error ('script.js:', this.script_name + ':', 'Unknown message:', msg);
  }
  // remote call methods start with _
  _api_level () {
    return API_LEVEL;
  }
  _script_name () {
    return this.script_name;
  }
  _set_uuid (uuid) {
    this.script_uuid = uuid;
  }
  _register (label, funid, blurb) {
    if (this.registry.get (funid))
      return this.kill (`register: duplicate id: ${funid}`);
    log ('_register: fun=' + funid + ':', label);
  }
}

window.run_script = s => {
  const host = new ScriptHost (s);
  return host;
};
