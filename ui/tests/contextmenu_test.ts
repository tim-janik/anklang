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
  children?: any;
})
{
  const container = document.createElement ('div');
  document.body.appendChild (container);

  const dispose = render (() => createComponent (ContextMenu, {
    ...props,
    children: props.children ?? [
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

    if (close_count < 1)
      throw new Error ('onclose was not called after menu activation');
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
  hotkey_button.setAttribute ('kbd', 'Ctrl+K');
  const menu = mount_menu ({
    activate: uri => { activated_uri = uri; },
    children: [hotkey_button],
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

/// Test that ContextMenu-created icon spans track `ic` mutations without touching supplied icons.
async function test_contextmenu_dynamic_icons (): Promise<boolean>
{
  const dynamic_button = make_button ('dynamic-icon', 'Dynamic Icon');
  dynamic_button.setAttribute ('ic', '✓');
  const supplied_button = make_button ('supplied-icon', 'Supplied Icon');
  supplied_button.setAttribute ('ic', '✓');
  const supplied_icon = document.createElement ('span');
  supplied_icon.className = 'b-icon application-icon';
  supplied_icon.setAttribute ('ic', 'application-icon');
  supplied_icon.textContent = 'Application Icon';
  supplied_button.prepend (supplied_icon);
  const menu = mount_menu ({ children: [dynamic_button, supplied_button] });

  try {
    await wait_for_contextmenu_update();
    const first_icon = dynamic_button.querySelector ('.b-icon[data-contextmenu-icon]');
    if (!first_icon || first_icon.textContent !== '✓')
      throw new Error ('ContextMenu did not create the initial icon span');
    if (supplied_icon.getAttribute ('ic') !== 'application-icon')
      throw new Error ('ContextMenu changed an application-owned icon');

    dynamic_button.setAttribute ('ic', '✗');
    await wait_for_contextmenu_update();
    const second_icon = dynamic_button.querySelector ('.b-icon[data-contextmenu-icon]');
    if (!second_icon || second_icon === first_icon || second_icon.textContent !== '✗')
      throw new Error ('ContextMenu icon did not update after an ic mutation');
    if (supplied_icon.getAttribute ('ic') !== 'application-icon')
      throw new Error ('ContextMenu changed an application-owned icon after an ic mutation');

    dynamic_button.removeAttribute ('ic');
    await wait_for_contextmenu_update();
    if (dynamic_button.querySelector ('.b-icon[data-contextmenu-icon]'))
      throw new Error ('ContextMenu-owned icon remained after removing ic');
  } finally {
    menu.cleanup();
  }

  return true;
}
sub_tests.push (['dynamic_icons', test_contextmenu_dynamic_icons]);

/// Test that opening a collapsed tree focuses its visible summary rather than a hidden leaf.
async function test_contextmenu_initial_focus (): Promise<boolean>
{
  const details = document.createElement ('details');
  const summary = document.createElement ('summary');
  summary.textContent = 'Category';
  details.append (summary, make_button ('hidden-leaf', 'Hidden Leaf'));
  const focus_uri = 'quoted"-uri';
  const requested_item = make_button (focus_uri, 'Requested Item');
  const disabled_item = make_button ('disabled-item', 'Disabled Item');
  const menu = mount_menu ({
    children: [details, requested_item, disabled_item],
    isactive: uri => uri !== 'disabled-item',
  });

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

    const tall_children: HTMLButtonElement[] = [];
    for (let i = 0; i < 100; i++)
      tall_children.push (make_button (`tall-${i}`, `Tall Item ${i}`));
    const tall_menu = mount_menu ({ children: tall_children });
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
