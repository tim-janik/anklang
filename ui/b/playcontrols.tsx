// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @class BPlayControls
 * @description
 * The <b-playcontrols> element is a container holding the play and seek controls for a Ase.song.
 */

// == STYLE ==
Extra_css`
b-playcontrols, .b-playcontrols {
  button, .asbutton	{ padding: 5px; text-align: center; }
}
`;

// == Component ==
import { createComputed } from 'solid-js';

export function PlayControls (props: any)
{
  /** @param {string} method */
  const dispatch = async (method: string) => {
    const project = (window as any).Shell?.project ?? App.project;
    const func = (project as any)[method];
    let message: string;
    if (func !== undefined) {
      let result = await func.call (project);
      if (result == undefined)
	result = 'ok';
      message = method + ': ' + result;
    } else {
      message = method + ': unimplemented';
    }
    App.status (message);
  };

  const toggle_play = () => {
    const project = (window as any).Shell?.project ?? App.project;
    const playing = project.is_playing;
    dispatch (playing ? 'pause_playback' : 'start_playback');
  };

  // Log playback state changes
  createComputed (() => {
    console.log ("is_playing:", App.project.is_playing);
  });

  return (
    <b-buttonbar class="b-playcontrols">
      <div class="asbutton button-down" disabled onClick={() => dispatch ('-todo-Last')}>
        <b-icon fw lg ic="fa-fast_backward"></b-icon>
      </div>
      <div class="asbutton button-down" disabled onClick={() => dispatch ('-todo-Backwards')}>
        <b-icon fw lg ic="fa-backward"></b-icon>
      </div>
      <div class="asbutton button-down" data-hotkey="S" data-tip="**CLICK** Stop playback"
           onClick={() => dispatch ('stop_playback')}>
        <b-icon fw lg ic="fa-stop"></b-icon>
      </div>
      <div class="asbutton button-down" data-hotkey="RawSpace"
           data-tip="**CLICK** Start/pause playback" onClick={toggle_play}>
        <b-icon fw lg ic="fa-play" hi="ho"></b-icon>
      </div>
      <div class="asbutton button-down" disabled onClick={() => dispatch ('-todo-Record')}>
        <b-icon fw lg ic="fa-circle"></b-icon>
      </div>
      <div class="asbutton button-down" disabled onClick={() => dispatch ('-todo-Forwards')}>
        <b-icon fw lg ic="fa-forward"></b-icon>
      </div>
      <div class="asbutton button-down" disabled onClick={() => dispatch ('-todo-Next')}>
        <b-icon fw lg ic="fa-fast_forward"></b-icon>
      </div>
    </b-buttonbar>
  );
}
