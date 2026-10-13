// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import * as Ase from '../../ase/gen/api-jsonipc.g.ts';

async function click_play_toggle (button: HTMLElement, project: Ase.Project, timeout_ms = 5000): Promise<string>
{
  type PlaybackCall = { method: string; promise: Promise<any> };
  let resolve_call!: (call: PlaybackCall) => void;
  let reject_call!: (error: Error) => void;
  const called = new Promise<PlaybackCall> ((resolve, reject) => {
    resolve_call = resolve;
    reject_call = reject;
  });
  const timer = setTimeout (() => reject_call (new Error (`Play/Pause did not dispatch within ${timeout_ms}ms`)), timeout_ms);
  const original_send = Ase.Jsonipc.send;
  Ase.Jsonipc.send = function (method, params) {
    const promise = original_send.call (this, method, params);
    if (params[0] === project && (method === 'pause_playback' || method === 'start_playback'))
      resolve_call ({ method, promise });
    return promise;
  };
  try {
    button.click();
    const call = await called;
    await call.promise;
    await project.$asyncs();
    return call.method;
  } finally {
    clearTimeout (timer);
    Ase.Jsonipc.send = original_send;
  }
}

export async function test_project_play_toggle (): Promise<boolean>
{
  const shell = window.Shell;
  const old_project = shell.project;
  const project = await Ase.server.create_project ('ColdPlayToggle');
  try {
    await project.start_playback();
    if (!await Ase.Jsonipc.send ('get/is_playing', [project]))
      throw new Error ('test project did not start playback');
    if (project.$props.is_playing)
      throw new Error ('test requires an unread playback property');
    const button = () => document.querySelector ('.b-playcontrols [data-hotkey="RawSpace"]') as HTMLElement;
    if (!button()) throw new Error ('Play/Pause control not found');
    shell.project = project;
    for (const [method, playing] of [['pause_playback', false], ['start_playback', true]] as const) {
      const called = await click_play_toggle (button(), project);
      if (called !== method)
        throw new Error (`Play/Pause called ${called} instead of ${method}`);
      if (!!await Ase.Jsonipc.send ('get/is_playing', [project]) !== playing)
        throw new Error (`Play/Pause did not ${playing ? 'resume' : 'pause'} playback`);
    }
  } finally {
    shell.project = old_project;
    await project.discard();
  }
  return true;
}
