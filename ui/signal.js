// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import * as Solid from "solid-js";
Object.defineProperty (window, 'Solid', { enumerable: true, value: Solid }); // !configurable

const solid_global = {};
function init_solid_global()
{
  if (!solid_global.dispose)
    Solid.createRoot (dispose => {
      solid_global.owner = Solid.getOwner();
      solid_global.dispose = dispose;
    });
  return solid_global;
}

const solidCreateSignal = Solid.createSignal;
export { solidCreateSignal as createSignal };
export const State = () => { console.error ("signal.js: State() should be unused"); };
export const Computed = () => { console.error ("signal.js: Computed() should be unused"); };
export const Watcher = () => { console.error ("signal.js: Watcher() should be unused"); };
const SignalF = () => { console.error ("signal.js: Signal() should be unused"); };
SignalF.State = function (v = undefined) {
  init_solid_global();
  Solid.runWithOwner (solid_global.owner, () => {
    const [get, set] = Solid.createSignal (v);
    this.get = get;
    this.set = set;
  });
};
export { SignalF as Signal };
if (!window['Signal'])
  Object.defineProperty (window, 'Signal', { enumerable: true, value: SignalF }); // !configurable

function solidjs_tracking_wrapper (queue_rerun, callback)	// -> tracked_callback
{
  init_solid_global();
  return Solid.runWithOwner (solid_global.owner, () => {
    const tracker = Solid.createReaction (queue_rerun);
    // TODO: add .destroy via a new createRoot
    return (...args) => { let r; tracker (() => r = callback (...args)); return r; };
  });
}
export { solidjs_tracking_wrapper as tracking_wrapper };

function solidjs_create_computed (callback)	// -> { destroy() }
{
  const handle = { dispose: undefined };
  let owner;
  Solid.createRoot (dispose => {
    owner = Solid.getOwner();
    handle.dispose = dispose;
  });
  Solid.runWithOwner (owner, () => Solid.createComputed (callback));
  return handle;
}
export { solidjs_create_computed as create_computed };
