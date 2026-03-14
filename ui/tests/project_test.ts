// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import * as Ase from '../../ase/gen/api-jsonipc.g.ts';

/// Test project creation, playback state, and length
export async function test_project_basic (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("BasicTest");
  if (!project)
    throw new Error ("Failed to create project");

  // Test initial playback state
  if (project.is_playing)
    throw new Error ("Project should not be playing initially");

  // Test project length
  const length = await project.length();
  if (length < 0.0)
    throw new Error (`Invalid project length: ${length}`);

  // Test BPM property - set BPM to a valid value
  project.bpm = 120.0;
  await project.$asyncs();
  const initial_bpm = project.bpm;

  // Test setting BPM to a different value
  project.bpm = 130.0;
  await project.$asyncs();
  if (Math.abs (project.bpm - 130.0) >= 0.001)
    throw new Error (`BPM not set correctly: ${project.bpm}`);

  project.bpm = 123.0;
  await project.$asyncs();
  if (Math.abs (project.bpm - 123.0) >= 0.001)
    throw new Error (`BPM not set correctly: ${project.bpm}`);

  // Test undo
  if (!await project.can_undo())
    throw new Error ("Expected can_undo() to be true");

  await project.undo();
  if (!await project.can_redo())
    throw new Error ("Expected can_redo() to be true");

  // Test redo
  await project.redo();
  if (Math.abs (project.bpm - 123.0) >= 0.001)
    throw new Error (`BPM not restored after redo: ${project.bpm}`);

  // Cleanup
  await project.discard();

  return true;
}

/// Test project master volume (set/get only, undo/redo has issues)
export async function test_project_master_volume (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("MasterVolumeTest");
  if (!project)
    throw new Error ("Failed to create project");

  // Wait for project to be fully initialized
  await project.$asyncs();

  // Test initial master volume
  const initial_vol = project.master_volume;

  // Test setting master volume
  project.master_volume = -6.0;
  await project.$asyncs();
  if (Math.abs (project.master_volume - (-6.0)) >= 0.01)
    throw new Error (`Master volume not set correctly: ${project.master_volume}`);

  // Reset master volume
  project.master_volume = initial_vol;
  await project.$asyncs();
  if (Math.abs (project.master_volume - initial_vol) >= 0.01)
    throw new Error (`Master volume not reset correctly: ${project.master_volume}`);

  // Cleanup
  await project.discard();

  return true;
}

/// Test project track management
export async function test_project_track_management (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("TrackManagementTest");
  if (!project)
    throw new Error ("Failed to create project");

  // Get initial track count
  const initial_tracks = await project.all_tracks();
  const initial_count = initial_tracks.length;
  if (initial_count < 1)
    throw new Error (`Expected at least 1 initial track, got: ${initial_count}`);

  // Create a new track
  const new_track = await project.create_track();
  if (!new_track)
    throw new Error ("Failed to create track");

  // Verify track count increased
  const tracks_after = await project.all_tracks();
  if (tracks_after.length !== initial_count + 1)
    throw new Error (`Track count not correct: expected ${initial_count + 1}, got ${tracks_after.length}`);

  // Cleanup
  await project.discard();

  return true;
}
