// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { ContextMenu } from '../b/contextmenu';
import * as Dom from '../dom';

/// Create a menu item button.
function make_button (uri: string, text: string): HTMLButtonElement
{
  const btn = document.createElement ('button');
  btn.setAttribute ('uri', uri);
  btn.textContent = text;
  return btn;
}

/// Mount a ContextMenu for testing and return helpers.
function mount_menu (props: {
  activate?: (uri: string, event?: Event) => void;
  onactivate?: (e: CustomEvent) => void;
  onclose?: (e: Event) => void;
  isactive?: (uri: string) => boolean | Promise<boolean>;
})
{
  const container = document.createElement ('div');
  document.body.appendChild (container);

  const dispose = render (() => createComponent (ContextMenu, {
    ...props,
    children: [
      make_button ('do-test', 'Do Test'),
      make_button ('do-other', 'Do Other'),
    ],
  }), container);

  const dialog = () => container.querySelector ('dialog.b-contextmenu') as HTMLDialogElement | null;
  const popup = () => {
    const d = dialog();
    if (!d) throw new Error ('ContextMenu dialog not rendered');
    const dialog_popup = (d as any).popup as (event?: Event) => boolean;
    if (typeof dialog_popup !== 'function') throw new Error ('ContextMenu popup method not attached');
    return dialog_popup.call (d);
  };
  const close = () => {
    const d = dialog();
    if (d)
      {
	const dialog_close = (d as any).close as () => void;
	if (typeof dialog_close === 'function') dialog_close.call (d);
      }
  };
  const cleanup = () => {
    close();
    dispose();
    container.remove();
  };
  return { container, dialog, popup, close, cleanup };
}

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Test that the `activate` prop callback receives (uri, event).
async function test_contextmenu_activate_prop (): Promise<boolean>
{
  let activated_uri: string | undefined = undefined;
  let event_arg: Event | undefined = undefined;
  const menu = mount_menu ({
    activate: (uri: string, event?: Event) => {
      activated_uri = uri;
      event_arg = event;
    },
  });

  try {
    await Dom.ui_next_frame();
    menu.popup();
    await Dom.ui_next_frame();

    const dialog = menu.dialog();
    if (!dialog || !dialog.open)
      throw new Error ('ContextMenu dialog did not open');

    if (!Dom.ui_find ('button', { uri: 'do-test' }))
      throw new Error ('Menu item button not found');

    await Dom.ui_click_wait ('button', { uri: 'do-test' });

    if (activated_uri !== 'do-test')
      throw new Error (`activate prop received wrong uri: ${activated_uri}`);
    if (!event_arg || !(event_arg instanceof MouseEvent))
      throw new Error ('activate prop did not receive MouseEvent as second argument');
  } finally {
    menu.cleanup();
  }

  return true;
}
sub_tests.push (['activate_prop', test_contextmenu_activate_prop]);

/// Test that the `onactivate` event listener receives event.detail.uri.
async function test_contextmenu_onactivate_event (): Promise<boolean>
{
  let activated_uri: string | undefined = undefined;
  let event_arg: CustomEvent | undefined = undefined;
  const menu = mount_menu ({
    onactivate: (e: CustomEvent) => {
      activated_uri = e.detail?.uri;
      event_arg = e;
    },
  });

  try {
    await Dom.ui_next_frame();
    menu.popup();
    await Dom.ui_next_frame();

    if (!Dom.ui_find ('button', { uri: 'do-other' }))
      throw new Error ('Menu item button not found');

    await Dom.ui_click_wait ('button', { uri: 'do-other' });

    if (activated_uri !== 'do-other')
      throw new Error (`onactivate event.detail.uri wrong: ${activated_uri}`);
    if (!event_arg || !(event_arg instanceof CustomEvent))
      throw new Error ('onactivate did not receive CustomEvent');
  } finally {
    menu.cleanup();
  }

  return true;
}
sub_tests.push (['onactivate_event', test_contextmenu_onactivate_event]);

/// Test that the `isactive` prop disables inactive items.
async function test_contextmenu_isactive_prop (): Promise<boolean>
{
  let activated_uri: string | undefined = undefined;
  const menu = mount_menu ({
    isactive: (uri: string) => uri !== 'do-test',
    activate: (uri: string) => { activated_uri = uri; },
  });

  try {
    await Dom.ui_next_frame();
    menu.popup();
    await Dom.ui_next_frame();

    const disabled_btn = Dom.ui_find ('button', { uri: 'do-test' });
    const active_btn = Dom.ui_find ('button', { uri: 'do-other' });
    if (!disabled_btn || !active_btn)
      throw new Error ('Menu item buttons not found');
    if (!disabled_btn.hasAttribute ('disabled'))
      throw new Error ('Inactive menu item should be disabled');
    if (active_btn.hasAttribute ('disabled'))
      throw new Error ('Active menu item should not be disabled');

    // Clicking a disabled button must not trigger activation.
    await Dom.ui_click_wait ('button', { uri: 'do-test' });
    if (activated_uri !== undefined)
      throw new Error ('Disabled menu item triggered activation');

    // Clicking an active button must trigger activation.
    await Dom.ui_click_wait ('button', { uri: 'do-other' });
    if (activated_uri !== 'do-other')
      throw new Error (`Active menu item triggered wrong uri: ${activated_uri}`);
  } finally {
    menu.cleanup();
  }

  return true;
}
sub_tests.push (['isactive_prop', test_contextmenu_isactive_prop]);

/// Test that the `onclose` prop is called when the menu closes.
async function test_contextmenu_onclose_prop (): Promise<boolean>
{
  let close_count = 0;
  const menu = mount_menu ({
    activate: () => {},
    onclose: () => { close_count++; },
  });

  try {
    await Dom.ui_next_frame();
    menu.popup();
    await Dom.ui_next_frame();

    if (!Dom.ui_find ('button', { uri: 'do-test' }))
      throw new Error ('Menu item button not found');

    // Activating an item closes the menu.
    await Dom.ui_click_wait ('button', { uri: 'do-test' });

    if (close_count < 1)
      throw new Error ('onclose was not called after menu activation');
  } finally {
    menu.cleanup();
  }

  return true;
}
sub_tests.push (['onclose_prop', test_contextmenu_onclose_prop]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_contextmenu (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('contextmenu: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('contextmenu failures:\n  ' + failures.join ('\n  '));
  return true;
}
