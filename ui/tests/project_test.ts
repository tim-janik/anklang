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
  const initial_bpm = await project.$refetch (() => project.bpm);

  // Test setting BPM to a different value
  project.bpm = 130.0;
  const bpm_val = await project.$refetch (() => project.bpm);
  if (Math.abs (bpm_val - 130.0) >= 0.001)
    throw new Error (`BPM not set correctly: ${bpm_val}`);

  project.bpm = 123.0;
  const bpm_val2 = await project.$refetch (() => project.bpm);
  if (Math.abs (bpm_val2 - 123.0) >= 0.001)
    throw new Error (`BPM not set correctly: ${bpm_val2}`);

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

  // Test initial master volume (also waits for project initialization)
  const initial_vol = await project.$refetch (() => project.master_volume);

  // Test setting master volume
  project.master_volume = -6.0;
  const vol = await project.$refetch (() => project.master_volume);
  if (Math.abs (vol - (-6.0)) >= 0.01)
    throw new Error (`Master volume not set correctly: ${vol}`);

  // Reset master volume
  project.master_volume = initial_vol;
  const vol2 = await project.$refetch (() => project.master_volume);
  if (Math.abs (vol2 - initial_vol) >= 0.01)
    throw new Error (`Master volume not reset correctly: ${vol2}`);

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

/// Test that removing a track emits all_tracks notification
export async function test_project_track_removal_notification (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("TrackRemovalNotificationTest");
  if (!project)
    throw new Error ("Failed to create project");

  // Get initial track count
  const tracks_before = await project.all_tracks();
  const count_before = tracks_before.length;
  if (count_before < 1)
    throw new Error (`Expected at least 1 initial track, got: ${count_before}`);

  // Create a new track
  const new_track = await project.create_track();
  if (!new_track)
    throw new Error ("Failed to create track");
  await project.$asyncs();

  // Verify track count increased
  const tracks_after_create = await project.all_tracks();
  if (tracks_after_create.length !== count_before + 1)
    throw new Error (`Track count not correct after create: expected ${count_before + 1}, got ${tracks_after_create.length}`);

  // Listen for all_tracks notifications
  let notification_count = 0;
  const remove_listener = project.on ("notify:all_tracks", () => { notification_count++; });

  // Remove the track
  await new_track.remove_self();
  await project.$asyncs();

  // Verify the track is gone
  const tracks_final = await project.all_tracks();
  if (tracks_final.length !== count_before)
    throw new Error (`Track count not correct after removal: expected ${count_before}, got ${tracks_final.length}`);

  // Verify notification was emitted
  if (notification_count <= 0)
    throw new Error (`Expected all_tracks notification, got: ${notification_count}`);

  // Clean up listener
  remove_listener();

  // Cleanup
  await project.discard();

  return true;
}
