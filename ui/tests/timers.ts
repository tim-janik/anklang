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

  get pending () { return this.callbacks.size; }

  run ()
  {
    for (const [id, callback] of [...this.callbacks]) {
      if (this.callbacks.delete (id))
        callback();
    }
  }

  restore ()
  {
    window.setTimeout = this.set_timeout;
    window.clearTimeout = this.clear_timeout;
    this.callbacks.clear();
  }
}
