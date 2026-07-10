// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import * as Ase from '../../ase/gen/api-jsonipc.g.ts';

/// Test clip volume property
export async function test_clip_volume (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("ClipVolumeTest");
  if (!project)
    throw new Error ("Failed to create project");

  const track = await project.create_track();
  if (!track)
    throw new Error ("Failed to create track");

  // Create a MIDI clip
  const clip = await track.create_midi_clip ("TestClip", 0.0, 4.0);
  if (!clip)
    throw new Error ("Failed to create MIDI clip");
  // Test initial volume (also waits for reactive init)
  const initial_vol = await clip.$refetch (() => clip.volume);

  // Test setting volume
  clip.volume = -6.0;
  const vol = await clip.$refetch (() => clip.volume);
  if (Math.abs (vol - (-6.0)) >= 0.01)
    throw new Error (`Clip volume not set correctly: ${vol}`);

  // Reset volume
  clip.volume = 0.0;
  const vol2 = await clip.$refetch (() => clip.volume);
  if (Math.abs (vol2) >= 0.01)
    throw new Error (`Clip volume not reset correctly: ${vol2}`);

  // Cleanup
  await project.discard();

  return true;
}

/// Test clip mute property
export async function test_clip_mute (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("ClipMuteTest");
  if (!project)
    throw new Error ("Failed to create project");

  const track = await project.create_track();
  if (!track)
    throw new Error ("Failed to create track");

  // Create a MIDI clip
  const clip = await track.create_midi_clip ("MuteTestClip", 0.0, 4.0);
  if (!clip)
    throw new Error ("Failed to create MIDI clip");

  // Test initial mute state
  if (await clip.is_muted())
    throw new Error ("Clip should not be muted initially");

  // Test setting mute
  await clip.set_muted (true);
  if (!await clip.is_muted())
    throw new Error ("Clip should be muted");

  // Reset mute
  await clip.set_muted (false);
  if (await clip.is_muted())
    throw new Error ("Clip should not be muted after reset");

  // Cleanup
  await project.discard();

  return true;
}

/// Test clip pan property (audio clips only)
export async function test_clip_pan (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("ClipPanTest");
  if (!project)
    throw new Error ("Failed to create project");

  const track = await project.create_track();
  if (!track)
    throw new Error ("Failed to create track");

  // Create an audio clip (pan is only available on audio clips)
  const clip = await track.create_audio_clip ("PanTestClip", 0.0, 4.0);
  if (!clip)
    throw new Error ("Failed to create audio clip");

  // Test initial pan (also waits for reactive init)
  const initial_pan = await clip.$refetch (() => clip.pan);

  // Test setting pan
  clip.pan = 0.5;
  const pan = await clip.$refetch (() => clip.pan);
  if (Math.abs (pan - 0.5) >= 0.01)
    throw new Error (`Clip pan not set correctly: ${pan}`);

  // Reset pan
  clip.pan = -0.5;
  const pan2 = await clip.$refetch (() => clip.pan);
  if (Math.abs (pan2 - (-0.5)) >= 0.01)
    throw new Error (`Clip pan not reset correctly: ${pan2}`);

  // Cleanup
  await project.discard();

  return true;
}

/// Test clip notes property
export async function test_clip_notes (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("ClipNotesTest");
  if (!project)
    throw new Error ("Failed to create project");

  const track = await project.create_track();
  if (!track)
    throw new Error ("Failed to create track");

  // Create a MIDI clip
  const clip = await track.create_midi_clip ("NotesTestClip", 0.0, 4.0);
  if (!clip)
    throw new Error ("Failed to create MIDI clip");

  // Test initial notes (should be empty)
  const initial_notes = clip.all_notes;
  if (initial_notes.length !== 0)
    throw new Error (`Expected empty notes, got: ${initial_notes.length}`);

  // Test setting notes
  const new_note: Ase.ClipNote = {
    id: -1,
    key: 60,
    channel: 0,
    selected: false,
    tick: 0,
    duration: 960,
    velocity: 0.8,
    fine_tune: 0.0
  };
  clip.all_notes = [new_note];

  const notes = await clip.$refetch (() => clip.all_notes);
  if (notes.length !== 1)
    throw new Error (`Expected 1 note, got: ${notes.length}`);
  if (notes[0].key !== 60)
    throw new Error (`Expected note key 60, got: ${notes[0].key}`);

  // Cleanup
  await project.discard();

  return true;
}

/// Test clip range property
export async function test_clip_range (): Promise<boolean>
{
  const server = Ase.server;
  const project = await server.create_project ("ClipRangeTest");
  if (!project)
    throw new Error ("Failed to create project");

  const track = await project.create_track();
  if (!track)
    throw new Error ("Failed to create track");

  // Create a MIDI clip
  const clip = await track.create_midi_clip ("RangeTestClip", 0.0, 4.0);
  if (!clip)
    throw new Error ("Failed to create MIDI clip");

  // Test initial range - just verify we can get values
  const [start_tick, stop_tick] = await Promise.all ([
    clip.start_tick(),
    clip.stop_tick()
  ]);
  if (start_tick < 0)
    throw new Error (`Expected start_tick >= 0, got: ${start_tick}`);
  if (stop_tick <= start_tick)
    throw new Error (`Expected stop_tick > start_tick, got start=${start_tick}, stop=${stop_tick}`);

  // Test setting range - increase by 1000 ticks
  const new_end_tick = stop_tick + 1000;
  clip.end_tick = new_end_tick;
  await clip.$asyncs();
  const new_stop_tick = await clip.stop_tick();
  if (new_stop_tick !== new_end_tick)
    throw new Error (`Expected stop_tick ${new_end_tick}, got: ${new_stop_tick}`);

  // Cleanup
  await project.discard();

  return true;
}
