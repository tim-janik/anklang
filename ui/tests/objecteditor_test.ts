import { createComponent, render } from 'solid-js/web';
import { ObjectEditor } from '../b/objecteditor';
import * as Dom from '../dom';

export async function test_objecteditor (): Promise<boolean>
{
  for (const dispose_early of [false, true]) {
    let finish_loading: () => void;
    const loading = new Promise<void> (resolve => { finish_loading = resolve; });
    const prop = {
      update_: () => loading,
      fetch_: () => true,
      hints_: ':bool:',
      ident_: 'enabled',
      group_: 'Test group',
      label_: 'Enabled',
      value_: { val: true },
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
    } finally {
      dispose();
      container.remove();
    }
  }
  return true;
}
