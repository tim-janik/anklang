// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { run_tests } from './runner.js';
import * as Dom from '../dom';

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// Act like a window resize by one pixel, wait for the frame that reports it to ResizeObserver.
async function resize_body (): Promise<void>
{
  document.body.style.height = (document.body.clientHeight - 1) + 'px';
  window.dispatchEvent (new Event ('resize'));
  await Dom.ui_next_frame();
  await Dom.ui_next_frame();
}

/// A window resize calls the Shell.on_resize() callbacks and keeps the mounted Shell tree.
async function test_relayout_resize (): Promise<boolean>
{
  const shell_before = Shell.shell_element;
  const height = document.body.style.height;
  let calls = 0;
  const unsubscribe = Shell.on_resize (() => calls++);
  try {
    await resize_body();
    if (Shell.shell_element !== shell_before || !document.body.contains (shell_before))
      throw new Error ('resize re-mounted the Shell tree');
    if (!calls)
      throw new Error ('resize did not call the Shell.on_resize() callbacks');
  } finally {
    unsubscribe();
    document.body.style.height = height;
  }
  return true;
}
sub_tests.push (['resize', test_relayout_resize]);

/// A DPR change re-mounts the Shell tree right away, keeps the selected track
/// and drops the resize callbacks of the old tree.
async function test_relayout_dpr (): Promise<boolean>
{
  const Electron = (window as any).Electron;
  const dpr_change = () => new Promise (resolve => {
    matchMedia (`(resolution: ${window.devicePixelRatio}dppx)`).addEventListener ('change', resolve, { once: true });
  });
  const shell_before = Shell.shell_element;
  const track = Shell.r.current_track;
  const height = document.body.style.height;
  const zoom = await Electron.call ('zoom_level');
  let calls = 0;
  Shell.on_resize (() => calls++);
  const set_zoom = async (level: number) => {
    const changed = dpr_change();
    await Electron.call ('zoom_level', level);
    await changed;
  };
  try {
    await set_zoom (zoom + 1);
    if (Shell.shell_element === shell_before || document.body.contains (shell_before))
      throw new Error ('DPR change did not re-mount the Shell tree right away');
    if (Shell.r.current_track !== track)
      throw new Error (`DPR change lost the selected track: ${Shell.r.current_track}`);
    calls = 0; // the zoom may resize the body before the DPR change event
    await resize_body();
    if (calls)
      throw new Error ('re-mount kept the resize callbacks of the old Shell tree');
  } finally {
    document.body.style.height = height;
    await set_zoom (zoom);
  }
  return true;
}
sub_tests.push (['dpr', test_relayout_dpr]);

/// Relayout is a no-op before a project is assigned.
async function test_relayout_guard_no_project (): Promise<boolean>
{
  const shell_before = Shell.shell_element;
  const project = Shell.project;
  Shell.project = null;
  try {
    App.relayout();
    if (Shell.shell_element !== shell_before)
      throw new Error ('relayout re-mounted the Shell tree without a project');
  } finally {
    Shell.project = project;
  }
  return true;
}
sub_tests.push (['guard_no_project', test_relayout_guard_no_project]);

export async function test_relayout (): Promise<boolean>
{
  return run_tests ('relayout', sub_tests);
}
