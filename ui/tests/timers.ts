// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL-2.0

/** Capture `window.setTimeout` callbacks so tests can advance timer driven
 * behavior on demand instead of waiting on the wall clock. Create TestTimers
 * before the code under test schedules its timers, fire them with `run()` and
 * put the original functions back with `restore()`. The delay argument is
 * ignored, and only `setTimeout`/`clearTimeout` are captured, so
 * `requestAnimationFrame` still runs normally.
 */
export class TestTimers {
  private set_timeout = window.setTimeout;
  private clear_timeout = window.clearTimeout;
  private callbacks = new Map<number, () => void>();

  constructor ()
  {
    window.setTimeout = ((callback: TimerHandler, _delay?: number, ...args: any[]) => {
      if (typeof callback !== 'function')
        throw new Error ('TestTimers expects a function');
      const id = this.set_timeout.call (window, () => {}, 0) as unknown as number;
      this.clear_timeout.call (window, id);
      this.callbacks.set (id, () => callback (...args));
      return id;
    }) as typeof window.setTimeout;
    window.clearTimeout = ((id?: number) => {
      this.callbacks.delete (id);
      this.clear_timeout.call (window, id);
    }) as typeof window.clearTimeout;
  }

  /// Number of captured callbacks that have not run yet.
  get pending () { return this.callbacks.size; }

  /// Run each captured callback once; callbacks they schedule stay pending.
  run ()
  {
    for (const [id, callback] of [...this.callbacks]) {
      if (this.callbacks.delete (id))
        callback();
    }
  }

  /// Restore the original timer functions and drop captured callbacks.
  restore ()
  {
    window.setTimeout = this.set_timeout;
    window.clearTimeout = this.clear_timeout;
    this.callbacks.clear();
  }
}
