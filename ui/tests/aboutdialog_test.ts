import { createComponent, render } from 'solid-js/web';
import { AboutDialog } from '../b/aboutdialog';
import * as Ase from '../../ase/gen/api-jsonipc.g';
import * as Dom from '../dom';

export async function test_aboutdialog (): Promise<boolean>
{
  const names = ['get_build_id', 'get_flac_version', 'get_opus_version', 'get_sndfile_version'];
  const server_methods = Object.getPrototypeOf (Ase.server);
  const originals = names.map (name => server_methods[name]);
  let resolve_info: (value: string) => void;
  for (const name of names)
    server_methods[name] = async () => 'test-version';
  try {
    for (const action of ['button', 'escape', 'dispose']) {
      server_methods.get_build_id = () => new Promise (resolve => { resolve_info = resolve; });
      const container = document.createElement ('div');
      document.body.appendChild (container);
      let closes = 0;
      const dispose = render (() => createComponent (AboutDialog, {
        onClose: () => { closes++; },
      }), container);
      try {
        await Dom.ui_next_frame();
        if (container.querySelector ('dialog'))
          throw new Error ('about dialog mounted before its content was ready');
        if (action === 'dispose')
          dispose();
        resolve_info ('loaded-build');
        await Dom.ui_next_frame();
        const dialog = container.querySelector ('dialog');
        if (action === 'dispose') {
          if (dialog || closes)
            throw new Error ('loading after disposal opened or closed a dialog');
          continue;
        }
        if (!dialog?.open || !dialog.textContent.includes ('loaded-build') || !dialog.textContent.includes ('test-version'))
          throw new Error ('about dialog opened without its complete content');
        if (action === 'button')
          dialog.querySelector ('button')!.click();
        else
          dialog.dispatchEvent (new KeyboardEvent ('keydown', { key: 'Escape', bubbles: true }));
        await Dom.ui_next_frame();
        await Dom.ui_next_frame();
        if (dialog.open || closes !== 1)
          throw new Error ('about dialog did not close once');
      } finally {
        dispose();
        container.remove();
      }
    }
  } finally {
    names.forEach ((name, index) => { server_methods[name] = originals[index]; });
  }
  return true;
}
