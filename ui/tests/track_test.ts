// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import * as Ase from '../../ase/gen/api-jsonipc.g.ts';

/// Test track volume property
export async function test_track_volume (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("TrackVolumeTest");
  if (!project)
    throw new Error ("Failed to create project");

  const track = await project.create_track();
  if (!track)
    throw new Error ("Failed to create track");
  // Test initial volume (also waits for reactive init)
  const initial_vol = await track.$refetch (() => track.volume);

  // Test setting volume
  track.volume = -6.0;
  const vol = await track.$refetch (() => track.volume);
  if (Math.abs (vol - (-6.0)) >= 0.01)
    throw new Error (`Track volume not set correctly: ${vol}`);

  // Reset volume
  track.volume = 0.0;
  const vol2 = await track.$refetch (() => track.volume);
  if (Math.abs (vol2) >= 0.01)
    throw new Error (`Track volume not reset correctly: ${vol2}`);

  // Cleanup
  await project.discard();

  return true;
}

/// Test track pan property
export async function test_track_pan (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("TrackPanTest");
  if (!project)
    throw new Error ("Failed to create project");

  const track = await project.create_track();
  if (!track)
    throw new Error ("Failed to create track");
  // Test initial pan (also waits for reactive init)
  const initial_pan = await track.$refetch (() => track.pan);

  // Test setting pan
  track.pan = 0.5;
  const pan = await track.$refetch (() => track.pan);
  if (Math.abs (pan - 0.5) >= 0.01)
    throw new Error (`Track pan not set correctly: ${pan}`);

  // Reset pan
  track.pan = -0.5;
  const pan2 = await track.$refetch (() => track.pan);
  if (Math.abs (pan2 - (-0.5)) >= 0.01)
    throw new Error (`Track pan not reset correctly: ${pan2}`);

  // Cleanup
  await project.discard();

  return true;
}

/// Test track mute property
export async function test_track_mute (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("TrackMuteTest");
  if (!project)
    throw new Error ("Failed to create project");

  const track = await project.create_track();
  if (!track)
    throw new Error ("Failed to create track");

  // Test initial mute state
  if (await track.is_muted())
    throw new Error ("Track should not be muted initially");

  // Test setting mute
  await track.set_muted (true);
  if (!await track.is_muted())
    throw new Error ("Track should be muted");

  // Reset mute
  await track.set_muted (false);
  if (await track.is_muted())
    throw new Error ("Track should not be muted after reset");

  // Cleanup
  await project.discard();

  return true;
}

/// Test track solo property
export async function test_track_solo (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("TrackSoloTest");
  if (!project)
    throw new Error ("Failed to create project");

  const track = await project.create_track();
  if (!track)
    throw new Error ("Failed to create track");

  // Test initial solo state
  if (await track.is_solo())
    throw new Error ("Track should not be solo initially");

  // Test setting solo
  await track.set_solo (true);
  if (!await track.is_solo())
    throw new Error ("Track should be solo");

  // Reset solo
  await track.set_solo (false);
  if (await track.is_solo())
    throw new Error ("Track should not be solo after reset");

  // Cleanup
  await project.discard();

  return true;
}

/// Test track midi_channel property
export async function test_track_midi_channel (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("TrackMidiChannelTest");
  if (!project)
    throw new Error ("Failed to create project");

  const track = await project.create_track();
  if (!track)
    throw new Error ("Failed to create track");
  // Test initial midi channel (also waits for reactive init)
  const initial_channel = await track.$refetch (() => track.midi_channel);

  // Test setting midi channel
  track.midi_channel = 1;
  const midi_channel = await track.$refetch (() => track.midi_channel);
  if (midi_channel !== 1)
    throw new Error (`Midi channel not set correctly: ${midi_channel}`);

  // Cleanup
  await project.discard();

  return true;
}
