// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL-2.0

import DataBubbleIface from '../b/databubble';
import * as Dom from '../dom';

/// Count data-bubble elements currently attached to the shell bubble layer.
function count_bubbles (): number
{
  return document.querySelectorAll ('.b-data-bubble').length;
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Test that DataBubbleIface.dispose releases the zmove hook, observer and DOM node.
async function test_databubble_dispose (): Promise<boolean>
{
  const shell_element = document.querySelector ('.b-shell');
  if (!shell_element)
    throw new Error ('Shell element not found in test environment');

  const app: any = (window as any).App;
  if (!app?.zmoves_add)
    throw new Error ('App.zmoves_add not available in test environment');

  // Track zmove hook deletions via the deleter returned by App.zmoves_add
  let deleter_handed = false;
  let deleter_called = false;
  const prev_zmoves_add = app.zmoves_add.bind (app);
  const RO = window.ResizeObserver;
  let disconnect_delta: any = 0;
  const prev_disconnect = RO.prototype.disconnect;
  let dbiface: DataBubbleIface | undefined;
  try {
    app.zmoves_add = (hook: any) => {
      const deleter = prev_zmoves_add (hook);
      deleter_handed = true;
      return () => { deleter_called = true; deleter(); };
    };

    RO.prototype.disconnect = function () {
      disconnect_delta++;
      prev_disconnect.call (this);
    };

    const bubbles_before = count_bubbles();
    dbiface = new DataBubbleIface (shell_element);

    // The new instance must attach its own bubble element
    if (count_bubbles() != bubbles_before + 1)
      throw new Error ('DataBubbleIface did not add a bubble element');
    if (!deleter_handed)
      throw new Error ('zmove deleter was not handed out');
    if (disconnect_delta != 0)
      throw new Error (`unexpected ResizeObserver disconnect before dispose: ${disconnect_delta}`);

    dbiface.dispose();

    if (!deleter_called)
      throw new Error ('dispose did not delete the zmove hook');
    if (count_bubbles() != bubbles_before)
      throw new Error ('dispose did not remove the bubble element');
    if (disconnect_delta != 1)
      throw new Error (`dispose did not disconnect the ResizeObserver: ${disconnect_delta}`);

    // dispose must be idempotent
    dbiface.dispose();
    dbiface.dispose();
    if (count_bubbles() != bubbles_before)
      throw new Error ('double dispose did not stay idempotent');
    if (disconnect_delta != 1)
      throw new Error (`double dispose caused extra disconnects: ${disconnect_delta}`);

    return true;
  } finally {
    try {
      dbiface?.dispose();
    } finally {
      app.zmoves_add = prev_zmoves_add;
      RO.prototype.disconnect = prev_disconnect;
    }
  }
}
sub_tests.push (['dispose', test_databubble_dispose]);

/// Test that dispose cancels pending debounced bubble checks without crashing.
async function test_databubble_dispose_debounce (): Promise<boolean>
{
  const shell_element = document.querySelector ('.b-shell');
  if (!shell_element)
    throw new Error ('Shell element not found in test environment');

  const bubbles_before = count_bubbles();
  const dbiface = new DataBubbleIface (shell_element);
  const pending_element = document.createElement ('div');
  shell_element.appendChild (pending_element);
  try {
    dbiface.force (pending_element);
    document.body.dispatchEvent (new PointerEvent ('pointermove', { bubbles: true }));
    dbiface.dispose();
    await Dom.ui_wait (200);

    if (count_bubbles() != bubbles_before)
      throw new Error ('disposed debounce left a bubble element attached');
  } finally {
    dbiface.dispose();
    pending_element.remove();
  }

  return true;
}
sub_tests.push (['dispose_debounce', test_databubble_dispose_debounce]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_databubble (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('databubble: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('databubble failures:\n  ' + failures.join ('\n  '));
  return true;
}
