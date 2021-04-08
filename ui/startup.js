// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
"use strict";

import './browserified.js';	// provides require() and browserified modules

// Hook into onload
if (document.readyState == 'complete')
  throw Error ("startup.js: script loaded too late");
window.addEventListener ('load', bootup);
console.bootlog = console.log;

// Global CONFIG
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
import { AppClass } from './b/app.js';
import Shell from './b/shell.js';
import VueComponents from './all-components.js';

// Custom Elements
class PushButton extends HTMLElement {
  constructor() { super(); }
}
customElements.define ('push-button', PushButton);
class HFlex extends HTMLElement {
  constructor() { super(); }
}
customElements.define ('h-flex', HFlex);
class VFlex extends HTMLElement {
  constructor() { super(); }
}
customElements.define ('v-flex', VFlex);
class CGrid extends HTMLElement {
  constructor() { super(); }
}
customElements.define ('c-grid', CGrid);

const Jsonapi = {
  jstrigger_counter: 100000 * (10 + (0 | 89 * Math.random())),
  jstrigger_map: {},
  jstrigger_create (fun) {
    const id = 'JsonapiTrigger/_' + ++this.jstrigger_counter;
    this.jstrigger_map[id] = fun;
    Ase.Jsonipc.send ('JsonapiTrigger/create', [ id ]);
    Ase.Jsonipc.receive (id, fun);
    return id;
  },
  jstrigger_remove (id) {
    if (this.jstrigger_map[id])
      {
	Ase.Jsonipc.send ('JsonapiTrigger/remove', [ id ]);
	Ase.Jsonipc.receive (id, null);
	delete this.jstrigger_map[id];
      }
  },
  _init (AseJsonipc) {
    console.assert (AseJsonipc == Ase.Jsonipc);
    Ase.Jsonipc.Jsonapi = Jsonapi;
    Ase.Jsonipc.receive ('JsonapiTrigger/killed', this.jstrigger_remove.bind (this));
    this._init = undefined;
  },
};

// Bootup, called onload
async function bootup () {
  if (window.__Electron__)
    {
      window.Electron = Object.assign ({}, // setup extensible window.Electron context
				       await window.__Electron__.call ("electron_versions"),
				       { call: window.__Electron__.call });
      // Shift+Ctrl+I for devTools
      document.addEventListener ("keydown", (event) => {
	if (event.shiftKey && event.ctrlKey && event.keyCode == 73)
	  window.Electron.call ('toggle_dev_tools');
      });
    }
  // Reload page on Websocket connection loss
  const url = window.location.href.replace ('http', 'ws');
  const reconnect = async () => {
    const timeout = ms => new Promise (resolve => setTimeout (resolve, ms));
    let polltime = 150;
    while (1) {
      fetch ('/').then (() => location.reload()).catch (() => 0);
      await timeout (polltime);
      polltime += 150; // backoff
    }
  };
  const want_reconnect = __DEV__ ? reconnect : undefined;
  if (want_reconnect)
    console.log ("__DEV__: watching:", url);
  // connect to Ase.server
  let error;
  try {
    const cururl = new URL (window.location);
    const result = await Ase.Jsonipc.open (url,
					   cururl.searchParams.get ('subprotocol') || undefined,
					   { onclose: want_reconnect });
    if (result instanceof Ase.Server)
      {
	Ase.server (result);
	Object.defineProperty (globalThis, 'Ase', { value: Ase });
	Jsonapi._init (Ase.Jsonipc);
	Ase.Emittable.prototype.on = function (eventselector, fun) {
	  const triggerid = Jsonapi.jstrigger_create (fun);
	  const discon = () => Jsonapi.jstrigger_remove (triggerid);
	  const prom = this.js_trigger (eventselector, triggerid);
	  return discon;
	};
      }
    else
      error = 'not an Ase.Server: ' + String (result);
  } catch (ex) {
    error = String (ex);
  }
  if (error)
    throw (new Error ('Ase: failed to connect to AnklangSynthEngine: ' + error));
  console.bootlog ("ASE: connect: server", await Ase.server.get_version());

  // create and configure Vue App
  const VueApp = Vue.createApp (Shell);
  VueApp.config.isCustomElement = tag => !!window.customElements.get (tag);
  const app = new AppClass (VueApp);
  console.assert (app == App);
  // register directives and mixins
  for (let directivename in Util.vue_directives) // register all utility directives
    VueApp.directive (directivename, Util.vue_directives[directivename]);
  for (let mixinname in Util.vue_mixins)         // register all utility mixins
    VueApp.mixin (Util.vue_mixins[mixinname]);
  // register all Vue components
  for (const [name, component] of Object.entries (VueComponents))
    {
      if (component.sfc_template)
	component.template = component.sfc_template.call (null, Util.tmplstr, null);
      VueApp.component (name, component);
    }

  // provide common globals inside Vue handlers
  Object.assign (VueApp.config.globalProperties, {
    CONFIG: globalThis.CONFIG,
    debug: globalThis.debug,
    Util: globalThis.Util,
    Ase: globalThis.Ase,
    window: globalThis.window,
    document: globalThis.document,
    observable_from_getters: Util.vue_observable_from_getters,
  });
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
  if (CONFIG.mainjs && CONFIG.files.length)
    {
      console.bootlog ("Loading", CONFIG.files.length, "command line files...");
      for (let arg of CONFIG.files)
        {
	  const error = await App.load_project (arg);
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
  // TEST: let l={}; document.body.onpointermove = e=>{ console.log("screenX:",e.screenX-l.screenX,"movementX:",e.movementX); l=e; }
  const chrome_major = parseInt (( /\bChrome\/([0-9]+)\./.exec (navigator.userAgent) || [0,0] )[1]);
  const chrome_major_last_buggy = 89;
  const dpr_movement = chrome_major >= 37 && chrome_major <= chrome_major_last_buggy;
  console.assert (chrome_major <= chrome_major_last_buggy, `WARNING: Chrome/${chrome_major} has not been tested for the movementX devicePixelRatio bug`);
  if (dpr_movement)
    console.bootlog ("Detected Chrome bug #1092358...");
  return {
    dpr_movement,
    slidertabindex,
  };
}

// test basic functionality
async function self_tests() {
  // test Ase::Value and server
  const ukey = "AseTest" + Util.hash53 ("" + Math.random() + Math.random() + Math.random());
  const obj = { 5: 5, a: [false, true, 2, "3", { a: [], o: {}, f: -0.17 }], mm: ["STRING", 7e42, null] };
  Ase.server.set_session_data (ukey, obj);
  const r = await Ase.server.get_session_data (ukey);
  console.assert (Util.equals_recursively (obj, r));
  Ase.server.set_session_data (ukey);
}
