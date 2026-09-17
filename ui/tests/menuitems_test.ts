import { createSignal } from 'solid-js';
import { type MenuEntry } from '../b/menuitems';
import { createComponent, render } from 'solid-js/web';
import { ContextMenu } from '../b/contextmenu';
import * as Dom from '../dom';

export async function test_menuitems (): Promise<boolean>
{
  const container = document.createElement ('div');
  document.body.appendChild (container);
  let activated = '';
  const dispose = render (() => createComponent (ContextMenu, {
    activate: uri => { activated = uri; },
    items: [
      { type: 'title', label: 'Menu' },
      { uri: 'first', label: 'First', icon: '✓', kbd: 'Ctrl+K' },
      { type: 'separator' },
      { type: 'row', noturn: true, items: [{ uri: 'row', label: 'Row' }] },
      { type: 'submenu', label: 'Submenu', items: [{ uri: 'nested', label: 'Nested' }] },
    ],
  }), container);
  try {
    await Dom.ui_next_frame();
    const dialog = container.querySelector ('dialog')!;
    const first = dialog.querySelector<HTMLButtonElement> ('button[uri=first]')!;
    if (!first.querySelector ('.b-icon') || !first.querySelector ('kbd'))
      throw new Error ('menu icon or shortcut was not rendered');
    if (!dialog.querySelector ('.b-menutitle') || !dialog.querySelector ('.b-menuseparator') || !dialog.querySelector ('.b-menurow.noturn'))
      throw new Error ('menu structure was not rendered');
    (dialog as any).popup();
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();
    const height = dialog.getBoundingClientRect().height;
    const summary = dialog.querySelector ('summary')!;
    summary.dispatchEvent (new KeyboardEvent ('keydown', { key: 'ArrowRight', bubbles: true }));
    if (!dialog.querySelector ('details')!.open)
      throw new Error ('submenu did not open from the keyboard');
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();
    if (dialog.getBoundingClientRect().height <= height)
      throw new Error ('popup did not grow to fit the open submenu');
    dialog.querySelector<HTMLButtonElement> ('button[uri=nested]')!.click();
    await Dom.ui_next_frame();
    if (activated !== 'nested' || dialog.open)
      throw new Error ('submenu activation did not close the popup');
    if (!first.isConnected)
      throw new Error ('closing the popup removed shortcut items');
  } finally {
    dispose();
    container.remove();
  }
  await test_menu_shortcuts();
  return true;
}

async function test_menu_shortcuts ()
{
  const [items, set_items] = createSignal<MenuEntry[]> ([
    { uri: 'hotkey', label: 'Hotkey', kbd: 'Alt+Shift+Y' },
    { uri: 'disabled', label: 'Unavailable', disabled: true },
  ]);
  let allowed = false;
  let activations = 0;
  const count = () => activations;
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (ContextMenu, {
    get items () { return items(); },
    isactive: () => allowed,
    activate: () => { activations++; },
  }), container);
  const press_key = async () => {
    document.dispatchEvent (new KeyboardEvent ('keydown', {
      key: 'Y', code: 'KeyY', altKey: true, shiftKey: true, bubbles: true, cancelable: true,
    }));
    await Dom.ui_next_frame();
  };
  try {
    await Dom.ui_next_frame();
    const dialog = container.querySelector ('dialog')! as any;
    dialog.map_kbd_hotkeys (true);
    await press_key();
    if (count())
      throw new Error ('shortcut activated an unavailable item');
    allowed = true;
    await press_key();
    if (count() !== 1)
      throw new Error ('closed menu shortcut did not activate');
    dialog.popup();
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();
    dialog.close();
    await Dom.ui_next_frame();
    if (!dialog.querySelector ('button[uri=disabled]').disabled)
      throw new Error ('closing the menu enabled an explicitly disabled item');
    set_items ([]);
    await Dom.ui_next_frame();
    await press_key();
    if (count() !== 1)
      throw new Error ('removed menu item retained its shortcut');
  } finally {
    dispose();
    container.remove();
  }
}
