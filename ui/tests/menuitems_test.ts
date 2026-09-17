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
    const summary = dialog.querySelector ('summary')!;
    summary.dispatchEvent (new KeyboardEvent ('keydown', { key: 'ArrowRight', bubbles: true }));
    if (!dialog.querySelector ('details')!.open)
      throw new Error ('submenu did not open from the keyboard');
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
  return true;
}
