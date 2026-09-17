// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createSignal } from 'solid-js';
import { type MenuAction, type MenuEntry } from '../b/menuitems';
import { createComponent, render } from 'solid-js/web';
import { ContextMenu } from '../b/contextmenu';
import * as Dom from '../dom';

function make_button (uri: string, label: string): MenuAction
{
  return { uri, label };
}

/// Mount a ContextMenu for testing and return helpers.
function mount_menu (props: {
  activate?: (uri: string, event?: Event) => void;
  onactivate?: (e: CustomEvent) => void;
  onclose?: (e: Event) => void;
  isactive?: (uri: string) => boolean | Promise<boolean>;
  items?: MenuEntry[];
})
{
  const container = document.createElement ('div');
  document.body.appendChild (container);

  const dispose = render (() => createComponent (ContextMenu, {
    ...props,
    items: props.items ?? [
      make_button ('do-test', 'Do Test'),
      make_button ('do-other', 'Do Other'),
    ],
  }), container);

  const dialog = () => container.querySelector ('dialog.b-contextmenu') as HTMLDialogElement | null;
  const popup = (event?: Event, popup_options?: any) => {
    const d = dialog();
    if (!d) throw new Error ('ContextMenu dialog not rendered');
    const dialog_popup = (d as any).popup as (event?: Event, popup_options?: any) => boolean;
    if (typeof dialog_popup !== 'function') throw new Error ('ContextMenu popup method not attached');
    return dialog_popup.call (d, event, popup_options);
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

/// Wait for ContextMenu's MutationObserver debounce and a subsequent DOM update.
async function wait_for_contextmenu_update (): Promise<void>
{
  await Dom.ui_next_frame();
  await Dom.ui_next_frame();
}

/// Capture ContextMenu development geometry diagnostics without hiding unrelated errors.
function capture_geometry_errors ()
{
  const original_error = console.error;
  const errors: string[] = [];
  console.error = (...args: any[]) => {
    if (typeof args[0] == 'string' && args[0].startsWith ('ContextMenu assert_geometry:'))
      errors.push (args.join (' '));
    else
      original_error.apply (console, args);
  };
  return {
    errors,
    restore: () => { console.error = original_error; },
  };
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

    if (close_count !== 1)
      throw new Error (`menu activation emitted ${close_count} close events`);
    menu.close();
    await Dom.ui_next_frame();
    if (Number (close_count) !== 1)
      throw new Error ('closing an already closed menu emitted another close');
    menu.popup();
    await wait_for_contextmenu_update();
    HTMLDialogElement.prototype.close.call (menu.dialog());
    await wait_for_contextmenu_update();
    if (Number (close_count) !== 2)
      throw new Error ('native close did not emit exactly one close');
  } finally {
    menu.cleanup();
  }

  return true;
}
sub_tests.push (['onclose_prop', test_contextmenu_onclose_prop]);

/// Test that mapped keyboard shortcuts activate menu items and can be removed again.
async function test_contextmenu_keyboard_map (): Promise<boolean>
{
  let activated_uri: string | undefined;
  const hotkey_button = make_button ('hotkey-item', 'Hotkey Item');
  hotkey_button.kbd = 'Ctrl+K';
  const menu = mount_menu ({
    activate: uri => { activated_uri = uri; },
    items: [hotkey_button],
  });

  try {
    await wait_for_contextmenu_update();
    const dialog = menu.dialog();
    if (!dialog) throw new Error ('ContextMenu dialog not rendered');
    const map_kbd_hotkeys = (dialog as any).map_kbd_hotkeys as (active?: boolean) => void;
    if (typeof map_kbd_hotkeys !== 'function')
      throw new Error ('ContextMenu map_kbd_hotkeys method not attached');

    map_kbd_hotkeys (true);
    menu.popup();
    await wait_for_contextmenu_update();
    document.dispatchEvent (new KeyboardEvent ('keydown', {
      key: 'k', code: 'KeyK', ctrlKey: true, bubbles: true, cancelable: true,
    }));
    await wait_for_contextmenu_update();

    if (activated_uri !== 'hotkey-item')
      throw new Error (`keyboard map activated wrong uri: ${activated_uri}`);

    map_kbd_hotkeys (false);
    activated_uri = undefined;
    document.dispatchEvent (new KeyboardEvent ('keydown', {
      key: 'k', code: 'KeyK', ctrlKey: true, bubbles: true, cancelable: true,
    }));
    await Dom.ui_next_frame();
    if (activated_uri !== undefined)
      throw new Error ('keyboard map remained active after removal');
  } finally {
    const dialog = menu.dialog();
    (dialog as any)?.map_kbd_hotkeys?.(false);
    menu.cleanup();
  }

  return true;
}
sub_tests.push (['keyboard_map', test_contextmenu_keyboard_map]);

async function test_contextmenu_dynamic_icons (): Promise<boolean>
{
  const [icon, set_icon] = createSignal ('✓');
  const supplied_icon = document.createElement ('span');
  supplied_icon.className = 'b-icon application-icon';
  supplied_icon.textContent = 'Application Icon';
  const menu = mount_menu ({ items: [
    { uri: 'dynamic-icon', label: 'Dynamic Icon', get icon () { return icon(); } },
    { uri: 'supplied-icon', label: 'Supplied Icon', children: supplied_icon },
  ] });
  try {
    await wait_for_contextmenu_update();
    const button = menu.container.querySelector ('button[uri=dynamic-icon]')!;
    if (button.querySelector ('.b-icon')?.textContent !== '✓')
      throw new Error ('menu did not render its icon');
    set_icon ('✗');
    await wait_for_contextmenu_update();
    if (button.querySelector ('.b-icon')?.textContent !== '✗' || button.querySelectorAll ('.b-icon').length !== 1)
      throw new Error ('menu did not update its icon');
    set_icon ('');
    await wait_for_contextmenu_update();
    if (button.querySelector ('.b-icon') || !supplied_icon.isConnected || supplied_icon.textContent !== 'Application Icon')
      throw new Error ('menu removed the wrong icon');
  } finally {
    menu.cleanup();
  }
  return true;
}
sub_tests.push (['dynamic_icons', test_contextmenu_dynamic_icons]);

/// Test that opening a collapsed tree focuses its visible summary rather than a hidden leaf.
async function test_contextmenu_initial_focus (): Promise<boolean>
{
  const focus_uri = 'quoted"-uri';
  const menu = mount_menu ({
    items: [
      { type: 'submenu', label: 'Category', items: [make_button ('hidden-leaf', 'Hidden Leaf')] },
      make_button (focus_uri, 'Requested Item'),
      make_button ('disabled-item', 'Disabled Item'),
    ],
    isactive: uri => uri !== 'disabled-item',
  });
  const summary = menu.container.querySelector ('summary');
  const requested_item = (menu.dialog() as any).find_menuitem (focus_uri);

  try {
    await wait_for_contextmenu_update();
    menu.popup();
    await wait_for_contextmenu_update();

    if (document.activeElement !== summary)
      throw new Error ('ContextMenu did not focus the first visible item in a collapsed tree');

    menu.close();
    await wait_for_contextmenu_update();
    menu.popup (undefined, { focus_uri: 'hidden-leaf' });
    await wait_for_contextmenu_update();
    if (document.activeElement !== summary)
      throw new Error ('ContextMenu did not fall back from a hidden focus target');

    menu.close();
    await wait_for_contextmenu_update();
    menu.popup (undefined, { focus_uri });
    await wait_for_contextmenu_update();
    if (document.activeElement !== requested_item)
      throw new Error ('ContextMenu did not focus an explicitly requested quoted URI');

    menu.close();
    await wait_for_contextmenu_update();
    menu.popup (undefined, { focus_uri: 'disabled-item' });
    await wait_for_contextmenu_update();
    if (document.activeElement !== summary)
      throw new Error ('ContextMenu did not fall back from a disabled focus target');
  } finally {
    menu.cleanup();
  }

  return true;
}
sub_tests.push (['initial_focus', test_contextmenu_initial_focus]);

/// Test that valid edge-positioned and height-capped menus do not emit geometry errors.
async function test_contextmenu_geometry_diagnostics (): Promise<boolean>
{
  if (document.documentElement.clientWidth <= 0 || document.documentElement.clientHeight <= 0)
    throw new Error ('ContextMenu geometry tests require a nonzero viewport');
  const capture = capture_geometry_errors();
  const cleanups: (() => void)[] = [];

  try {
    for (const side of ['left', 'right']) {
      const origin = document.createElement ('button');
      origin.textContent = `${side} origin`;
      origin.style.cssText = `position:fixed; ${side}:0; top:0; width:1px; height:1px;`;
      document.body.appendChild (origin);
      const menu = mount_menu ({});
      cleanups.push (() => { menu.cleanup(); origin.remove(); });

      await wait_for_contextmenu_update();
      menu.popup (undefined, { origin });
      await wait_for_contextmenu_update();
      const dialog = menu.dialog();
      if (!dialog || !dialog.open)
        throw new Error (`${side}-edge ContextMenu did not open`);
      const bounds = dialog.getBoundingClientRect();
      const viewport_width = document.documentElement.clientWidth;
      if (side == 'left' && bounds.left > 2)
        throw new Error ('left-edge ContextMenu was not left aligned');
      if (side == 'right' && bounds.right < viewport_width - 2)
        throw new Error ('right-edge ContextMenu was not right aligned');
      menu.close();
      await Dom.ui_next_frame();
    }

    const tall_children: MenuEntry[] = [];
    for (let i = 0; i < 100; i++)
      tall_children.push (make_button (`tall-${i}`, `Tall Item ${i}`));
    const tall_menu = mount_menu ({ items: tall_children });
    cleanups.push (tall_menu.cleanup);
    await wait_for_contextmenu_update();
    tall_menu.popup();
    await wait_for_contextmenu_update();
    const tall_dialog = tall_menu.dialog();
    if (!tall_dialog || tall_dialog.scrollHeight <= tall_dialog.clientHeight)
      throw new Error ('tall ContextMenu did not reach its intended height cap');
  } finally {
    for (const cleanup of cleanups.reverse())
      cleanup();
    capture.restore();
  }

  if (capture.errors.length)
    throw new Error (`ContextMenu emitted valid-placement geometry errors: ${capture.errors.join ('; ')}`);
  return true;
}
sub_tests.push (['geometry_diagnostics', test_contextmenu_geometry_diagnostics]);

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
