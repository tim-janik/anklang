// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import * as Dom from '../dom';

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// A resize triggers one debounced Shell relayout: repeated resizes collapse
/// into a single re-mount that detaches the old .b-shell node.
async function test_relayout_resize (): Promise<boolean>
{
  const app: any = (window as any).App;
  if (typeof app?.relayout !== 'function')
    throw new Error ('App.relayout not available in test environment');
  if (!app.project)
    throw new Error ('App.project not set in test environment');

  // Capture the current Shell root; a relayout re-mounts the tree, replacing it.
  const shell_before = document.querySelector ('.b-shell');
  if (!shell_before)
    throw new Error ('Shell element not found in test environment');

  // Drain the boot-time debounced relayout so it cannot fire during the test.
  await Dom.ui_wait (600);

  // Count relayouts while letting the real re-mount run
  let relayout_count = 0;
  const orig_relayout = app.relayout.bind (app);
  app.relayout = () => { relayout_count++; return orig_relayout(); };

  try {
    // Two rapid resizes: the debounce (restart=true) must fire once at the end.
    window.dispatchEvent (new Event ('resize'));
    await Dom.ui_wait (150);
    window.dispatchEvent (new Event ('resize'));
    // 500ms after the second resize; the first call is well past 500ms.
    await Dom.ui_wait (600);

    if (relayout_count < 1)
      throw new Error ('resize did not schedule a relayout');
    if (relayout_count > 1)
      throw new Error (`multiple resizes caused multiple relayouts: ${relayout_count}`);

    // The re-mount must have detached the original Shell node
    if (document.body.contains (shell_before))
      throw new Error ('relayout did not re-mount the Shell tree (old node still attached)');
    const shell_after = document.querySelector ('.b-shell');
    if (!shell_after)
      throw new Error ('relayout left no .b-shell element behind');
    if (shell_after === shell_before)
      throw new Error ('relayout did not replace the .b-shell element');
  } finally {
    app.relayout = orig_relayout;
  }

  return true;
}
sub_tests.push (['resize', test_relayout_resize]);

/// Relayout re-mounts the Shell tree but must keep the selected track
/// (Shell.reset() would reset it to the initial master track).
async function test_relayout_preserves_state (): Promise<boolean>
{
  const app: any = (window as any).App;
  const shell: any = (window as any).Shell;
  if (typeof app?.relayout !== 'function' || !shell)
    throw new Error ('App.relayout/Shell not available in test environment');

  // Set a non-default selection that reset() would clear
  const r = shell.r;
  const orig_track = r.current_track;
  r.current_track = 'relayout-test-track';

  try {
    app.relayout();
    await Dom.ui_next_frame();
    await Dom.ui_next_frame();

    if (r.current_track !== 'relayout-test-track')
      throw new Error (`relayout lost the selected track: ${r.current_track}`);
  } finally {
    r.current_track = orig_track;
  }

  return true;
}
sub_tests.push (['preserves_state', test_relayout_preserves_state]);

/// Relayout is a no-op before a project is assigned: the boot-time
/// dpr_rerender_all() call must not trigger it.
async function test_relayout_guard_no_project (): Promise<boolean>
{
  const app: any = (window as any).App;
  const shell: any = (window as any).Shell;
  if (!shell)
    throw new Error ('Shell not available in test environment');

  // Null Shell.project to simulate the pre-boot state without tearing down the Shell.
  const real_project = shell.project;
  shell.project = null;

  let relayout_count = 0;
  const orig_relayout = app.relayout.bind (app);
  app.relayout = () => { relayout_count++; return orig_relayout(); };

  try {
    window.dispatchEvent (new Event ('resize'));
    await Dom.ui_wait (700); // 500ms debounce + slack

    if (relayout_count != 0)
      throw new Error (`relayout fired with no project assigned: ${relayout_count}`);
  } finally {
    app.relayout = orig_relayout;
    shell.project = real_project;
  }

  return true;
}
sub_tests.push (['guard_no_project', test_relayout_guard_no_project]);

// == Master runner ==
/// Single exported entry point runs all sub-tests in sequence.
export async function test_relayout (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('relayout: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('relayout failures:\n  ' + failures.join ('\n  '));
  return true;
}
