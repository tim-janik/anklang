// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/// The window title includes the project name (M7).
async function test_activation_title (): Promise<boolean>
{
  const app: any = (window as any).App;
  const name = app?.project?.name;
  if (!name)
    throw new Error ('App.project has no name in test environment');

  if (!document.title.includes (name))
    throw new Error (`document.title does not contain project name: "${document.title}"`);
  if (!document.title.includes ('Anklang'))
    throw new Error (`document.title does not contain app name: "${document.title}"`);

  return true;
}
sub_tests.push (['title', test_activation_title]);

/// The initial current_track is editable, not the master output (M7).
async function test_activation_current_track (): Promise<boolean>
{
  const app: any = (window as any).App;
  const shell: any = (window as any).Shell;
  const track = app?.current_track;
  if (!track)
    throw new Error ('App.current_track is not set after boot');
  if (await track.is_control_track())
    throw new Error ('App.current_track is a control track');
  if (shell?.r?.current_track !== track)
    throw new Error ('Shell.r.current_track disagrees with App.current_track');

  return true;
}
sub_tests.push (['current_track', test_activation_current_track]);

/// Activation opens the piano roll for the first clip (M7).
async function test_activation_piano_roll (): Promise<boolean>
{
  const app: any = (window as any).App;
  const shell: any = (window as any).Shell;
  const old_project = app?.project;
  if (!old_project)
    throw new Error ('App.project not set in test environment');

  const project = await Ase.server.create_project ('AppActivationTest');
  if (!project)
    throw new Error ('create_project failed');
  try {
    const tracks = await project.all_tracks();
    let track: any = null;
    for (const candidate of tracks)
      if (!await candidate.is_control_track()) {
        track = candidate;
        break;
      }
    if (!track)
      throw new Error ('project has no editable track');

    const new_track = await project.create_track();
    if (!new_track)
      throw new Error ('create_track failed');
    if (await new_track.is_control_track())
      throw new Error ('created track is classified as a control track');
    const clip = await track.create_midi_clip ('activation-test', 0, 4);
    if (!clip)
      throw new Error ('create_midi_clip failed');

    await app.load_project (project);

    if (shell.r.current_track !== track)
      throw new Error (`activation did not select the editable track: ${shell.r.current_track?.name ?? shell.r.current_track}`);
    if (shell.r.piano_roll_source !== clip)
      throw new Error ('activation did not open the first clip in the piano roll');
  } finally {
    await app.load_project (old_project);
    await project.discard();
  }

  return true;
}
sub_tests.push (['piano_roll', test_activation_piano_roll]);

// == Master runner ==
/// Runs all sub-tests in sequence.
export async function test_app (): Promise<boolean>
{
  if (!sub_tests.length)
    throw new Error ('app: no sub-tests registered');
  const failures: string[] = [];
  for (const [name, fn] of sub_tests) {
    try {
      await fn();
    } catch (e) {
      failures.push (`${name}: ${(e as Error)?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error ('app failures:\n  ' + failures.join ('\n  '));
  return true;
}
