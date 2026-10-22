// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import * as Dom from '../dom';

export async function test_menubar (): Promise<boolean>
{
  for (const [name, key] of [['file', 'f'], ['edit', 'e'], ['view', 'v'], ['help', 'h']]) {
    const button = document.querySelector<HTMLElement> (`#g-${name}menu`)!;
    const dialog = button?.querySelector ('dialog');
    if (!dialog || !button.querySelector ('.b-menubar-icon .b-icon'))
      throw new Error (`${name} menu lost its button, dialog or icon`);
    for (const action of ['mousedown', 'click', 'hotkey']) {
      await Dom.ui_next_frame();
      if (action === 'hotkey') {
        (document.activeElement as HTMLElement)?.blur();
        document.dispatchEvent (new KeyboardEvent ('keydown', {
          key, code: 'Key' + key.toUpperCase(), altKey: true, bubbles: true, cancelable: true,
        }));
      } else
        button.dispatchEvent (new MouseEvent (action, { bubbles: true, cancelable: true }));
      await Shell.project.can_undo();
      await Dom.ui_next_frame();
      if (!dialog.open)
        throw new Error (`${name} menu did not open on ${action}`);
      dialog.close();
    }
  }
  return true;
}
