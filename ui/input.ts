import { createEffect, onCleanup, untrack } from 'solid-js';

export function local_input_value<T> (read: () => T, show: (value: T) => void): (value: T) => void
{
  // Each input masks backend updates only until its own edit timer expires.
  let timer = 0;
  createEffect (() => {
    const value = read();
    if (!timer)
      untrack (() => show (value));
  });
  onCleanup (() => clearTimeout (timer));
  return value => {
    clearTimeout (timer);
    timer = window.setTimeout (() => {
      timer = 0;
      show (read());
    }, CONFIG.INPUT_EDIT_GRACE_MS);
    show (value);
  };
}
