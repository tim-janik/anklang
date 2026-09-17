import { createSignal } from 'solid-js';
import { createComponent, render } from 'solid-js/web';
import { PianoRoll } from '../b/pianoroll';
import * as Dom from '../dom';

export async function test_pianoroll_shortcuts (): Promise<boolean>
{
  const [hidden, set_hidden] = createSignal (false);
  const container = document.createElement ('div');
  document.body.appendChild (container);
  const dispose = render (() => createComponent (PianoRoll, {
    get hidden () { return hidden(); },
  }), container);
  let tool_clicks = 0;
  let action_clicks = 0;
  const counts = () => [tool_clicks, action_clicks].join (',');
  const press_keys = async () => {
    document.dispatchEvent (new KeyboardEvent ('keydown', {
      key: 'x', code: 'KeyX', ctrlKey: true, bubbles: true, cancelable: true,
    }));
    document.dispatchEvent (new KeyboardEvent ('keydown', {
      key: '1', code: 'Digit1', bubbles: true, cancelable: true,
    }));
    await Dom.ui_next_frame();
  };
  try {
    await Dom.ui_next_frame();
    const grid = container.querySelector<HTMLElement> ('.b-pianoroll-grid')!;
    const tool = container.querySelector ('button[uri="S"]')!;
    const action = container.querySelector<HTMLButtonElement> ('.-pianorollmenu button[kbd="Ctrl+X"]')!;
    action.disabled = false;
    tool.addEventListener ('click', () => { tool_clicks++; });
    action.addEventListener ('click', () => { action_clicks++; });
    grid.dispatchEvent (new PointerEvent ('pointerenter'));
    grid.focus();
    grid.dispatchEvent (new FocusEvent ('focus'));
    if (document.activeElement !== grid)
      throw new Error (`grid did not take focus: ${document.activeElement?.outerHTML.slice (0, 200)}`);
    await press_keys();
    if (counts() !== '1,1')
      throw new Error (`visible piano shortcuts failed: ${counts()}`);
    set_hidden (true);
    await Dom.ui_next_frame();
    await press_keys();
    if (counts() !== '1,1')
      throw new Error (`hidden piano shortcuts remained active: ${counts()}`);
    if (!tool.isConnected || !action.isConnected)
      throw new Error ('hiding the piano roll removed its menu items');
    set_hidden (false);
    await Dom.ui_next_frame();
    await press_keys();
    if (counts() !== '1,1')
      throw new Error ('showing the piano roll restored stale hover or focus');
    grid.dispatchEvent (new PointerEvent ('pointerenter'));
    grid.focus();
    grid.dispatchEvent (new FocusEvent ('focus'));
    if (document.activeElement !== grid)
      throw new Error (`grid did not take focus: ${document.activeElement?.outerHTML.slice (0, 200)}`);
    await press_keys();
    if (counts() !== '2,2')
      throw new Error (`piano shortcuts did not resume: ${counts()}`);
  } finally {
    dispose();
    container.remove();
  }
  return true;
}
