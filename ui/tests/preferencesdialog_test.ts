import { createComponent, render } from 'solid-js/web';
import { PreferencesDialog } from '../b/preferencesdialog';
import * as Ase from '../../ase/gen/api-jsonipc.g';
import * as Dom from '../dom';

export async function test_preferencesdialog (): Promise<boolean>
{
  const methods = Object.getPrototypeOf (Ase.server);
  const list_preferences = methods.list_preferences;
  const access_preference = methods.access_preference;
  try {
    for (const dispose_early of [false, true]) {
      let finish_loading: () => void;
      const loading = new Promise<void> (resolve => { finish_loading = resolve; });
      methods.list_preferences = async () => ['enabled'];
      methods.access_preference = async () => ({
        update_: () => loading,
        fetch_: () => true,
        hints_: ':bool:',
        ident_: 'enabled',
        group_: 'Test group',
        label_: 'Enabled',
        value: true,
      });
      const container = document.createElement ('div');
      document.body.appendChild (container);
      let closes = 0;
      const dispose = render (() => createComponent (PreferencesDialog, {
        onClose: () => { closes++; },
      }), container);
      try {
        await Dom.ui_next_frame();
        if (container.querySelector ('dialog')?.open)
          throw new Error ('preferences opened before its fields loaded');
        if (dispose_early)
          dispose();
        finish_loading();
        await Dom.ui_next_frame();
        const dialog = container.querySelector ('dialog');
        if (dispose_early) {
          if (dialog)
            throw new Error ('preferences opened after disposal');
          continue;
        }
        if (!dialog?.open || !dialog.querySelector ('input[type=checkbox]') || !dialog.textContent.includes ('Enabled'))
          throw new Error ('preferences did not open with its complete fields');
        dialog.querySelector ('button')!.click();
        await Dom.ui_next_frame();
        await Dom.ui_next_frame();
        if (dialog.open || closes !== 1)
          throw new Error ('preferences did not close once');
      } finally {
        dispose();
        container.remove();
      }
    }
  } finally {
    methods.list_preferences = list_preferences;
    methods.access_preference = access_preference;
  }
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (PreferencesDialog, {}), container);
  try {
    for (let frame = 0; frame < 120 && !container.querySelector ('dialog')?.open; frame++)
      await Dom.ui_next_frame();
    const dialog = container.querySelector ('dialog');
    if (!dialog?.open || !dialog.querySelector ('.b-objecteditor-field'))
      throw new Error ('real preferences did not open with editor fields');
    const before = dialog.getBoundingClientRect();
    await Dom.ui_wait (100);
    const after = dialog.getBoundingClientRect();
    if (before.width !== after.width || before.height !== after.height)
      throw new Error ('preferences changed size after opening');
  } finally {
    dispose();
    container.remove();
  }
  return true;
}
