// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { Signal } from "signal-polyfill";

if (!window['Signal'])
  Object.defineProperty (window, 'Signal', { enumerable: true, value: Signal }); // !configurable

export const State = Signal.State;
export const Computed = Signal.Computed;
export const Watcher = Signal.subtle.Watcher;
export { Signal };

/**
 * @brief tracking_wrapper (queue_rerun, callback) -> tracked_callback
 * @param {function} queue_rerun Function called whenever a dependency becomes dirty.
 * @param {function} callback Function for which to track Signal dependencies.
 * @return {tracked_callback} Function that calls `callback` and tracks Signal dependencies.
 *
 * This function returns a `tracked_callback`, that is a wrapper for the `callback` argument.
 * It tracks the dependencies of `callback` and re-runs `queue_rerun()` whenever one of the
 * Signal dependencies used by `callback` becomes dirty.
 * Execution of `queue_rerun()` should be a light weight, i.e. not access any Signals.
 * It could e.g. just perfom a call to queueMicrotask() to cause a future re-run of
 * `tracked_callback` at a later time.
 */
export function tracking_wrapper (queue_rerun, callback, opt = {})	// -> tracked_callback
{
  // Watcher to track dependencies of callback.
  const watcher = opt.watcher || new Signal.subtle.Watcher (() => blocked || queue_rerun());
  if (!watcher.dirty_toggle)
    watcher.dirty_toggle = new Signal.State (false);
  let callback_this, callback_args, blocked = false;
  let callback_signal = new Signal.Computed (() => {
    watcher.dirty_toggle.get();                         // install dirty_toggle as callback_signal dependency
    return callback.apply (callback_this, callback_args);
  });
  watcher.watch (callback_signal);
  /**@this any*/
  function tracked_callback (...args) {
    if (!callback) return;
    const was_blocked = blocked;
    blocked = true;                                     // prevent queue_rerun() due to just dirty_toggle changes
    watcher.dirty_toggle.set (!watcher.dirty_toggle.get()); // force uncaching of callback_signal
    blocked = was_blocked;                              // re-enable watcher notifications
    callback_this = this;
    callback_args = args;
    const result = callback_signal.get();               // synchronously calls callback() with args
    callback_this = callback_args = null;               // allow GC of arguments
    watcher.watch();                                    // restart Signal dirty watching
    return result;
  }
  tracked_callback.watcher = watcher;
  // Allow reuse of the watcher
  // Disable future calls via `tracked_callback.destroy()`
  tracked_callback.destroy = () => {
    blocked = true;
    watcher.unwatch (callback_signal);
    callback = null;
    callback_signal = null;
    queue_rerun = null;
  };
  return tracked_callback;
}
