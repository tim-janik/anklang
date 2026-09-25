// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createComponent, render } from 'solid-js/web';
import { ModalDialogs } from '../b/modals';
import * as Dom from '../dom';

export async function test_modals (): Promise<boolean>
{
  for (const action of ['button', 'native', 'dispose']) {
    const container = document.createElement ('div');
    document.body.appendChild (container);
    let modals;
    const dispose = render (() => createComponent (ModalDialogs, { ref: value => { modals = value; } }), container);
    let destroyed = 0;
    let result;
    try {
      modals.async_modal_dialog ({
        title: 'Choose',
        text: 'A test dialog',
        buttons: ['Cancel', 'Accept'],
        destroy: () => { destroyed++; },
      }).then (value => { result = value; });
      await Dom.ui_next_frame();
      const dialog = container.querySelector ('dialog');
      if (!dialog?.open)
        throw new Error ('modal did not open');
      if (action === 'button')
        dialog.querySelectorAll ('button')[1].click();
      else if (action === 'native')
        dialog.close();
      else
        dispose();
      await Dom.ui_next_frame();
      await Dom.ui_next_frame();
      if (result !== (action === 'button' ? 1 : -1))
        throw new Error (`${action}: unexpected modal result ${result}`);
      if (destroyed !== 1)
        throw new Error (`${action}: destroy called ${destroyed} times`);
      if (container.querySelector ('dialog'))
        throw new Error (`${action}: closed modal remains mounted`);
    } finally {
      dispose();
      container.remove();
    }
    if (destroyed !== 1)
      throw new Error (`${action}: disposal called destroy again`);
  }
  return true;
}
