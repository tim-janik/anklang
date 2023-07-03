// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import './browserified.js';	// provides require() and browserified modules
import './b/basics.js';		// Register basic components first
import { LitComponent, html, css, docs, lit_update_all } from './little.js';

// Global CONFIG
console.bootlog = console.log;
const fallback_config = {
  // runtime defaults and constants
  MAXINT: 2147483647, MAXUINT: 4294967295, mainjs: false,
  dpr_movement: false, // Chrome bug, movementX *should* match screenX units
  files: [], p: '', m: '', norc: false, uiscript: '',
};
Object.assign (globalThis.CONFIG, fallback_config, browser_config());

// Global Util
import * as Util from './util.js';
Object.defineProperty (globalThis, 'Util', { value: Util });

// Import Ase, connecting is done asynchronously
import * as Ase from './aseapi.js';

// Global Vue and Theme
import * as Vue from './vue.js';
Object.defineProperty (globalThis, 'Vue', { value: Vue });
import { create_app } from './b/app.js';

// load Script host
import * as Script from './script.js';

const Jsonapi = {
  jstrigger_counter: 100000 * (10 + (0 | 89 * Math.random())),
  jstrigger_map: {},
  jstrigger_create (fun) {
    const id = 'Jsonapi/Trigger/_' + ++this.jstrigger_counter;
    this.jstrigger_map[id] = fun;
    Ase.Jsonipc.send ('Jsonapi/Trigger/create', [ id ]);
    Ase.Jsonipc.receive (id, fun);
    return id;
  },
  jstrigger_remove (id) {
    if (this.jstrigger_map[id])
      {
	Ase.Jsonipc.send ('Jsonapi/Trigger/remove', [ id ]);
	Ase.Jsonipc.receive (id, null);
	delete this.jstrigger_map[id];
      }
  },
  _init (AseJsonipc) {
    console.assert (AseJsonipc == Ase.Jsonipc);
    Ase.Jsonipc.Jsonapi = Jsonapi;
    Ase.Jsonipc.receive ('Jsonapi/Trigger/killed', this.jstrigger_remove.bind (this));
    Ase.Jsonipc.handle_binary (Util.jsonipc_binary_handler_);
    this._init = undefined;
  },
};

// Bootup, called after onload
async function bootup () {
  if (window.__Electron__)
    {
      window.Electron = Object.assign ({}, // setup extensible window.Electron context
				       await window.__Electron__.call ("electron_versions"),
				       { call: window.__Electron__.call });
      if (window.Electron.config.quitstartup)
	window.addEventListener ("deviceorientation", event => {
	  window.close();
	  // Ase.server.shutdown();
	  // window.Electron.call ('exit', 123);
	});
      // Shift+Ctrl+I for devTools
      document.addEventListener ("keydown", (event) => {
	if (event.shiftKey && event.ctrlKey && event.keyCode == 73)
	  window.Electron.call ('toggle_dev_tools');
      });
      if (window.Electron.config.files)
	console.log ("LOAD:", window.Electron.config.files);
      if (window.Electron.config.files.length)
	CONFIG.files.push (...window.Electron.config.files);
    }
  // Reload page on Websocket connection loss
  const url = window.location.href.replace ('http', 'ws');
  const reconnect = async () => {
    const timeout = ms => new Promise (resolve => setTimeout (resolve, ms));
    let polltime = 150;
    while (polltime) {
      fetch ('/').then (() => location.reload()).catch (() => 0);
      await timeout (polltime);
      polltime += 150; // backoff
    }
  };
  const want_reconnect = __DEV__ ? reconnect : undefined;
  if (want_reconnect)
    console.log ("__DEV__: watching:", url);
  // prepare remote GC
  Ase.Jsonipc.finalization_registration = jsonapi_finalization_registration;
  // connect to Ase.server
  let error;
  try {
    const cururl = new URL (window.location);
    const connected = await Ase.Jsonipc.open (url,
					      cururl.searchParams.get ('subprotocol') || undefined,
					      { onclose: want_reconnect });
    const initresult = connected ? await Ase.Jsonipc.send ("Jsonapi/initialize", []) : null;
    if (initresult instanceof Ase.Server)
      {
	Ase.server.__resolve__ (initresult);
	Object.defineProperty (globalThis, 'Ase', { value: Ase });
	Jsonapi._init (Ase.Jsonipc);
	Ase.Emittable.prototype.on = function (eventselector, fun) {
	  const triggerid = Jsonapi.jstrigger_create (fun);
	  const discon = () => Jsonapi.jstrigger_remove (triggerid);
	  const prom = this.js_trigger (eventselector, triggerid);
	  void (prom);
	  return discon;
	};
      }
    else if (!connected)
      error = "failed to connect to Jsonipc endpoint";
    else
      error = 'not an Ase.Server: ' + String (initresult);
  } catch (ex) {
    error = String (ex);
  }
  if (error)
    throw (new Error ('Ase: failed to connect to AnklangSynthEngine: ' + error));
  console.bootlog ("ASE: connect: server", await Ase.server.get_version());

  // create main App instance
  const app = await create_app();
  console.assert (app === App);

  // Ensure APP rerenders when the browser window changes
  const rerender_all = () => {
    lit_update_all();
    const vue_force_update = vm => {
      vm.$forceUpdate();
      for (const cv of vm.$children)
	vue_force_update (cv);
    };
    if (window.Shell)
      vue_force_update (Shell);
  };
  window.addEventListener ('resize', rerender_all);
  document.fonts.addEventListener ("loadingdone", rerender_all);
  const dpr_rerender_all = () => {
    if (dpr_rerender_all.media)
      dpr_rerender_all.media.removeListener (dpr_rerender_all);
    const mquery_string = `(resolution: ${window.devicePixelRatio}dppx)`;
    dpr_rerender_all.media = matchMedia (mquery_string);
    dpr_rerender_all.media.addListener (dpr_rerender_all);
    rerender_all();
  };
  dpr_rerender_all();

  // ensure App has an AseProject
  await App.load_project_checked ((await Ase.server.last_project()) || '');

  // mount in DOM and create component hierarchy
  await document.fonts.ready; // Fonts - wait for fonts before Vue components are mounted and compute sizes
  App.mount ('#b-app');
  console.assert (app === globalThis.App);
  console.bootlog (`Vue App mounted...`);

  // Load external plugins
  if (CONFIG.mainjs)
    {
      console.bootlog ("Loading LADSPA plugins...");
      await Ase.server.load_ladspa();
    }

  // Load Anklang Scripts
  if (true) {
    for (const url of ['/Builtin/Controller', '/Builtin/Scripts', '/User/Controller', '/User/Scripts']) {
      const crawler = await Ase.server.url_crawler (url);
      const centries = await crawler.list_entries();
      for (const e of centries)
	if (e.type == Ase.ResourceType.FILE && e.label.endsWith ('.js'))
	  Script.load_script (url + '/' + e.label);
    }
  }

  // UI-Script
  if (CONFIG.mainjs && CONFIG.uiscript)
    {
      console.bootlog ("Loading '" + CONFIG.uiscript + "'...");
      window.uiscript = await import (CONFIG.uiscript);
      try {
        await window.uiscript.run();
      } catch (e) {
        console.error (e.stack ? e.stack : e.message ? e.message : e, '\n',
                       'UIScript failed, aborting...');
	window.Electron.call ('exit', 123);
      }
    }

  // Load command line files
  if (CONFIG.files.length)
    {
      console.bootlog ("Loading", CONFIG.files.length, "command line files...");
      for (let arg of CONFIG.files)
        {
	  const error = await App.load_project_checked (arg);
	  if (error != Ase.Error.NONE)
	    console.error ('Failed to load:', arg + ":", error);
	}
    }

  // Clear temporary files if *not* in script mode
  if (CONFIG.mainjs && !CONFIG.uiscript)
    {
      const ms = 2000;
      console.bootlog ("Will clean Ase cachedirs in", ms + "ms...");
      setTimeout (async () => {
        await Ase.server.purge_stale_cachedirs();
        // debug ("Cleaned Ase cachedirs...");
      }, ms);
    }

  // Test integrity
  if (__DEV__)
    await self_tests();
}

const jsonapi_finalization_registry = new FinalizationRegistry (jsonapi_finalization_gc);
const jsonapi_finalization_garbage = new Set();

/// Jsonipc handler for object creation
function jsonapi_finalization_registration (object) {
  jsonapi_finalization_garbage.delete (object.$id);
  jsonapi_finalization_registry.register (object, { $id: object.$id, classname: object.constructor.name });
}

/// Jsonipc handler for IDs of GC-ed objects.
async function jsonapi_finalization_gc (gcinfo)
{
  const { $id, classname } = gcinfo;
  jsonapi_finalization_garbage.add ($id);
  console.log ("GC: $id=" + $id, "(" + classname + ")", "(size=" + jsonapi_finalization_garbage.size + ")", jsonapi_finalization_gc.inflight ? "(remote gc inflight)" : "");
  if (jsonapi_finalization_gc.inflight)
    return; // avoid duplicate Jsonapi/renew-gc requests
  let start = Ase.Jsonipc.send ('Jsonapi/renew-gc', []);
  jsonapi_finalization_gc.inflight = true;
  start = await start;
  jsonapi_finalization_gc.inflight = false;
  if (!start)
    return; // may indicate another request pending
  const ids = Array.from (jsonapi_finalization_garbage);
  jsonapi_finalization_garbage.clear();
  return Ase.Jsonipc.send ('Jsonapi/report-gc', [ ids ]);
}

// browser_config() - detect browser oddities, called during early boot
function browser_config() {
  const gecko = navigator.userAgent.indexOf ('Gecko/') >= 0;
  const chrome = navigator.userAgent.indexOf ('Chrome/') >= 0;
  // Add CSS helper: var(--device-pixel-ratio)
  document.body.parentNode.style.setProperty ('--device-pixel-ratio', window.devicePixelRatio);
  // Add helpers for browser specific CSS (needs document.body to exists)
  document.body.parentElement.toggleAttribute ('gecko', gecko);
  document.body.parentElement.toggleAttribute ('chrome', chrome);
  // Firefox sliders cannot be tragged when tabindex=-1
  const slidertabindex = gecko ? "0" : "-1";
  // Chrome Bug: https://bugs.chromium.org/p/chromium/issues/detail?id=1092358 https://github.com/w3c/pointerlock/issues/42
  const chrome_major = parseInt (( /\bChrome\/([0-9]+)\./.exec (navigator.userAgent) || [0,0] )[1]);
  const dpr_movement = gecko || (chrome_major >= 37 && chrome_major <= 100); // Chrome-101 fixes #1092358
  if (dpr_movement)
    console.bootlog ("Detected Chrome bug #1092358...");
  // Prevent Web Browser menus interfering with the UI
  window.addEventListener ("contextmenu", e => {
    if (!(e.target.tagName == "INPUT" || e.target.tagName == "TEXTAREA"))
      e.preventDefault();
  });
  return {
    gecko, chrome,
    dpr_movement,
    slidertabindex,
  };
}

// test basic functionality
async function self_tests() {
  // test Ase::Value and server
  const ukey = "_AseTest" + Util.hash53 ("" + Math.random() + Math.random() + Math.random());
  const obj = { 5: 5, a: [false, true, 2, "3", { a: [], o: {}, f: -0.17 }], mm: ["STRING", 7e42, null] };
  Ase.server.set_data (ukey, obj);
  const r = await Ase.server.get_data (ukey);
  console.assert (Util.equals_recursively (obj, r));
  Ase.server.set_data (ukey, undefined);
}

// Boot after onload
if (document.readyState !== 'complete')
  await new Promise (r => window.addEventListener ('load', r, { once: true }));
bootup();
