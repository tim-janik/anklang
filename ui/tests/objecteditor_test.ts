import { createSignal } from 'solid-js';
import { createComponent, render } from 'solid-js/web';
import { ObjectEditor } from '../b/objecteditor';
import * as Dom from '../dom';

export async function test_objecteditor (): Promise<boolean>
{
  for (const dispose_early of [false, true]) {
    let finish_loading: () => void;
    const loading = new Promise<void> (resolve => { finish_loading = resolve; });
    const [value, set_value] = createSignal (true, { equals: false });
    const edits: boolean[] = [];
    const prop = {
      update_: () => loading,
      fetch_: () => true,
      hints_: ':bool:',
      ident_: 'enabled',
      group_: 'Test group',
      label_: 'Enabled',
      get value () { return value(); },
      apply_: value => edits.push (value),
    };
    const container = document.createElement ('div');
    document.body.appendChild (container);
    let ready = 0;
    const dispose = render (() => createComponent (ObjectEditor, {
      value: [prop],
      onReady: () => {
        if (!container.querySelector ('input[type=checkbox]') || !container.textContent.includes ('Enabled'))
          throw new Error ('editor reported ready before rendering its fields');
        ready++;
      },
    }), container);
    try {
      await Dom.ui_next_frame();
      if (ready)
        throw new Error ('editor reported ready before loading');
      if (dispose_early)
        dispose();
      finish_loading();
      await Dom.ui_next_frame();
      if (ready !== Number (!dispose_early))
        throw new Error ('editor readiness did not follow its lifetime');
      if (!dispose_early) {
        const input = container.querySelector<HTMLInputElement> ('input[type=checkbox]')!;
        input.click();
        if (input.checked || edits.join (',') !== 'false')
          throw new Error ('editor did not send the user edit');
        set_value (true);
        await Dom.ui_next_frame();
        if (!input.checked || edits.length !== 1)
          throw new Error ('editor did not accept the backend correction quietly');
      }
    } finally {
      dispose();
      container.remove();
    }
  }
  return true;
}
