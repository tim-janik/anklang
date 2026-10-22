// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import { createComponent, render } from 'solid-js/web';
import { PositionView } from '../b/positionview';
import * as Dom from '../dom';

export async function test_positionview_values (): Promise<boolean>
{
  const previous_project = Shell.project;
  const project = { bpm: 120, numerator: 4, denominator: 4, telemetry: async () => [] };
  const container = document.createElement ('div');
  document.body.appendChild (container);
  let dispose = () => {};
  try {
    Shell.project = project;
    dispose = render (() => createComponent (PositionView, {}), container);
  } finally {
    Shell.project = previous_project;
  }
  try {
    await Dom.ui_next_frame();
    const inputs = container.querySelectorAll ('input');
    const edit = async (input: HTMLInputElement, text: string) => {
      input.parentElement!.click();
      input.value = text;
      input.dispatchEvent (new KeyboardEvent ('keydown', { key: 'Enter', bubbles: true }));
      await Dom.ui_next_frame();
    };
    for (const [text, expected] of [
      ['120.5', 120.5], ['abc', 120.5], ['', 120.5], ['Infinity', 120.5], ['0', 120.5], ['-1', 120.5], ['135.25', 135.25],
    ] as const) {
      await edit (inputs[1], text);
      if (project.bpm !== expected)
        throw new Error (`BPM edit ${text} produced ${project.bpm}, expected ${expected}`);
    }
    for (const [text, numerator, denominator] of [
      ['7/8', 7, 8], ['4.5/4', 7, 8], ['3/8.5', 7, 8], ['abc/4', 7, 8], ['0/4', 7, 8], ['4/0', 7, 8], ['3/4', 3, 4],
    ] as const) {
      await edit (inputs[0], text);
      if (project.numerator !== numerator || project.denominator !== denominator)
        throw new Error (`Time signature edit ${text} produced ${project.numerator}/${project.denominator}`);
    }
  } finally {
    dispose();
    container.remove();
  }
  return true;
}
