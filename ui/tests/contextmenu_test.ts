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

type ContextMenuTestProps = {
  activate?: (uri: string, event?: Event) => void;
  onactivate?: (e: CustomEvent) => void;
  onclose?: (e: Event) => void;
  isactive?: (uri: string) => boolean | Promise<boolean>;
  showicons?: boolean;
  mapname?: string;
  class?: string;
  id?: string;
  xscale?: number;
  yscale?: number;
  children?: Node | Node[];
};

/// Mount a ContextMenu for testing and return helpers.
function mount_menu (props: ContextMenuTestProps = {})
{
  const container = document.createElement ('div');
  document.body.appendChild (container);

  const { children, ...component_props } = props;
  const dispose = render (() => createComponent (ContextMenu, {
    ...component_props,
    children: children || [
      make_button ('do-test', 'Do Test'),
      make_button ('do-other', 'Do Other'),
    ],
  }), container);

  const dialog = () => container.querySelector ('dialog.b-contextmenu') as HTMLDialogElement | null;
  const popup = (event?: Event, options?: Record<string, any>) => {
    const d = dialog();
    if (!d) throw new Error ('ContextMenu dialog not rendered');
    const dialog_popup = (d as any).popup as (event?: Event, options?: Record<string, any>) => boolean;
    if (typeof dialog_popup !== 'function') throw new Error ('ContextMenu popup method not attached');
    return dialog_popup.call (d, event, options);
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
  let dispatched_count = 0;
  const menu = mount_menu ({
    activate: (uri: string, event?: Event) => {
      activated_uri = uri;
      event_arg = event;
    },
  });

  try {
    await Dom.ui_next_frame();
    menu.dialog()?.addEventListener ('activate', () => { dispatched_count++; });
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
    if (dispatched_count !== 0)
      throw new Error ('activate prop should take precedence over DOM activate event');
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
  let dispatched_event: CustomEvent | undefined = undefined;
  const menu = mount_menu ({
    onactivate: (e: CustomEvent) => {
      activated_uri = e.detail?.uri;
      event_arg = e;
    },
  });

  try {
    await Dom.ui_next_frame();
    menu.dialog()?.addEventListener ('activate', e => { dispatched_event = e as CustomEvent; });
    menu.popup();
    await Dom.ui_next_frame();

    if (!Dom.ui_find ('button', { uri: 'do-other' }))
      throw new Error ('Menu item button not found');

    await Dom.ui_click_wait ('button', { uri: 'do-other' });

    if (activated_uri !== 'do-other')
      throw new Error (`onactivate event.detail.uri wrong: ${activated_uri}`);
    if (!event_arg || !(event_arg instanceof CustomEvent))
      throw new Error ('onactivate did not receive CustomEvent');
    if (dispatched_event !== event_arg)
      throw new Error ('onactivate and DOM listener did not receive the same event');
    if (event_arg.bubbles)
      throw new Error ('activate event should not bubble');
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
      throw new Error (`onclose called ${close_count} times after one activation`);
  } finally {
    menu.cleanup();
  }

  return true;
}
sub_tests.push (['onclose_prop', test_contextmenu_onclose_prop]);

/// Test component attributes, imperative methods, and menu-data lookup.
async function test_contextmenu_component_contract (): Promise<boolean>
{
  const menu = mount_menu ({ id: 'test-contextmenu', class: 'extra-menu-class', showicons: false, mapname: 'Test Map' });
  try {
    await Dom.ui_next_frame();
    const dialog = menu.dialog() as any;
    if (!dialog) throw new Error ('ContextMenu dialog not rendered');
    if (dialog.id !== 'test-contextmenu' || !dialog.classList.contains ('extra-menu-class'))
      throw new Error ('id or class prop was not applied to dialog');
    for (const method of ['popup', 'close', 'map_kbd_hotkeys', 'check_isactive', 'find_menuitem'])
      if (typeof dialog[method] !== 'function')
        throw new Error (`imperative method ${method} missing from dialog ref`);
    if (dialog.find_menuitem ('do-other')?.getAttribute ('uri') !== 'do-other')
      throw new Error ('find_menuitem did not find the requested item');
    if (dialog.find_menuitem ('missing') !== null)
      throw new Error ('find_menuitem should return null for an unknown URI');
    if (dialog.menudata?.showicons !== false || dialog.menudata?.mapname !== 'Test Map')
      throw new Error ('component props were not exposed through menu data');
  } finally {
    menu.cleanup();
  }
  return true;
}
sub_tests.push (['component_contract', test_contextmenu_component_contract]);

/// Popup must consume its event, reject duplicates, and clean up marker state once.
async function test_contextmenu_popup_lifecycle (): Promise<boolean>
{
  let close_count = 0;
  const menu = mount_menu ({ onclose: () => { close_count++; } });
  const marker = document.createElement ('div');
  document.body.appendChild (marker);
  try {
    await Dom.ui_next_frame();
    const event = new MouseEvent ('contextmenu', { bubbles: true, cancelable: true, clientX: 12, clientY: 24 });
    if (menu.popup (event, { 'data-contextmenu': marker }) !== true)
      throw new Error ('first popup request was rejected');
    if (!event.defaultPrevented)
      throw new Error ('popup did not prevent the triggering event');
    if (marker.getAttribute ('data-contextmenu') !== 'true')
      throw new Error ('popup did not mark its context element');
    if (menu.popup() !== false)
      throw new Error ('duplicate popup request should be rejected');
    menu.close();
    menu.close();
    await Dom.ui_next_frame();
    if (marker.hasAttribute ('data-contextmenu'))
      throw new Error ('close did not clear the context marker');
    if (close_count !== 1)
      throw new Error (`two close calls emitted ${close_count} close events`);
    if (menu.popup() !== true)
      throw new Error ('menu could not reopen on a later frame');
  } finally {
    menu.cleanup();
    marker.remove();
  }
  return true;
}
sub_tests.push (['popup_lifecycle', test_contextmenu_popup_lifecycle]);

/// A hidden origin cannot anchor a popup and must not acquire marker state.
async function test_contextmenu_hidden_origin (): Promise<boolean>
{
  let close_count = 0;
  const menu = mount_menu ({ onclose: () => { close_count++; } });
  const hidden = document.createElement ('div');
  hidden.style.display = 'none';
  document.body.appendChild (hidden);
  try {
    await Dom.ui_next_frame();
    if (menu.popup (undefined, { origin: hidden }) !== false)
      throw new Error ('popup accepted an invisible origin');
    if (menu.dialog()?.open)
      throw new Error ('rejected popup still opened the dialog');
    if (hidden.hasAttribute ('data-contextmenu'))
      throw new Error ('rejected popup marked its hidden origin');
    if (close_count !== 0)
      throw new Error ('rejected popup emitted a close event');
  } finally {
    menu.cleanup();
    hidden.remove();
  }
  return true;
}
sub_tests.push (['hidden_origin', test_contextmenu_hidden_origin]);

/// Async enablement must settle before focus is assigned to the requested item.
async function test_contextmenu_async_isactive_and_focus (): Promise<boolean>
{
  const checked: string[] = [];
  const menu = mount_menu ({
    isactive: async uri => {
      checked.push (uri);
      await Promise.resolve();
      return uri === 'do-other';
    },
  });
  try {
    await Dom.ui_next_frame();
    menu.popup (undefined, { focus_uri: 'do-other' });
    await Dom.ui_next_frame();
    const dialog = menu.dialog();
    const first = dialog?.querySelector ('button[uri="do-test"]') as HTMLButtonElement | null;
    const other = dialog?.querySelector ('button[uri="do-other"]') as HTMLButtonElement | null;
    if (!first || !other) throw new Error ('menu items not rendered');
    if (checked.join (',') !== 'do-test,do-other')
      throw new Error (`isactive checked unexpected URIs: ${checked.join (',')}`);
    if (!first.disabled || other.disabled)
      throw new Error ('async isactive result was not reflected in disabled state');
    if (document.activeElement !== other)
      throw new Error ('focus_uri item did not receive focus after async checks');
  } finally {
    menu.cleanup();
  }
  return true;
}
sub_tests.push (['async_isactive_and_focus', test_contextmenu_async_isactive_and_focus]);

/// Child integration adds accessibility/icon/hotkey markup and delegates nested clicks.
async function test_contextmenu_child_integration (): Promise<boolean>
{
  let activated_uri: string | undefined = undefined;
  const button = make_button ('nested-action', 'Nested Action');
  button.setAttribute ('ic', 'md-close');
  button.setAttribute ('kbd', 'Ctrl+N');
  const menu = mount_menu ({ children: button, activate: uri => { activated_uri = uri; } });
  try {
    await Dom.ui_next_frame();
    const icon = button.querySelector ('b-icon') as HTMLElement | null;
    const kbd = button.querySelector ('kbd') as HTMLElement | null;
    if (button.getAttribute ('aria-label') !== 'Nested Action')
      throw new Error (`unexpected aria-label: ${button.getAttribute ('aria-label')}`);
    if (icon?.getAttribute ('ic') !== 'md-close')
      throw new Error ('ic attribute did not produce the expected icon');
    if (!kbd || !kbd.innerText)
      throw new Error ('kbd attribute did not produce a shortcut label');
    menu.popup();
    await Dom.ui_next_frame();
    icon.click();
    await Dom.ui_wait (50);
    if (activated_uri !== 'nested-action')
      throw new Error (`nested icon click activated wrong URI: ${activated_uri}`);
  } finally {
    menu.cleanup();
  }
  return true;
}
sub_tests.push (['child_integration', test_contextmenu_child_integration]);

/// The mutation observer must integrate menu items appended after initial render.
async function test_contextmenu_dynamic_child (): Promise<boolean>
{
  const menu = mount_menu();
  try {
    await Dom.ui_next_frame();
    const inner = menu.dialog()?.querySelector ('.b-contextmenu-inner');
    if (!inner) throw new Error ('ContextMenu inner container not rendered');
    const dynamic = make_button ('dynamic', 'Dynamic Item');
    dynamic.setAttribute ('ic', 'fa-plus_circle');
    inner.appendChild (dynamic);
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();
    if (dynamic.getAttribute ('aria-label') !== 'Dynamic Item')
      throw new Error ('dynamically added item was not integrated');
    if (dynamic.querySelector ('b-icon')?.getAttribute ('ic') !== 'fa-plus_circle')
      throw new Error ('dynamic item icon was not integrated');
    if ((menu.dialog() as any).find_menuitem ('dynamic') !== dynamic)
      throw new Error ('dynamic item is unavailable through find_menuitem');
  } finally {
    menu.cleanup();
  }
  return true;
}
sub_tests.push (['dynamic_child', test_contextmenu_dynamic_child]);

/// Border-box sizing must include dialog border and padding around inner content.
async function test_contextmenu_border_box_height (): Promise<boolean>
{
  const menu = mount_menu();
  try {
    await Dom.ui_next_frame();
    const dialog = menu.dialog();
    const inner = dialog?.querySelector ('.b-contextmenu-inner') as HTMLElement | null;
    if (!dialog || !inner) throw new Error ('ContextMenu structure not rendered');
    dialog.style.boxSizing = 'border-box';
    dialog.style.borderStyle = 'solid';
    dialog.style.borderTopWidth = '2px';
    dialog.style.borderBottomWidth = '4px';
    dialog.style.paddingTop = '3px';
    dialog.style.paddingBottom = '5px';
    Object.defineProperty (inner, 'scrollHeight', { configurable: true, value: 80 });
    menu.popup();
    if (dialog.style.height !== '94px')
      throw new Error (`border-box height omitted box extras: ${dialog.style.height}`);
  } finally {
    menu.cleanup();
  }
  return true;
}
sub_tests.push (['border_box_height', test_contextmenu_border_box_height]);

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
