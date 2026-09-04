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
import { ButtonBar } from './buttonbar';
import { Icon } from './icon';

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
    <ButtonBar class="b-playcontrols">
      <div class="asbutton button-down" disabled onClick={() => dispatch ('-todo-Last')}>
        <Icon fw lg ic="fa-fast_backward"/>
      </div>
      <div class="asbutton button-down" disabled onClick={() => dispatch ('-todo-Backwards')}>
        <Icon fw lg ic="fa-backward"/>
      </div>
      <div class="asbutton button-down" data-hotkey="S" data-tip="**CLICK** Stop playback"
           onClick={() => dispatch ('stop_playback')}>
        <Icon fw lg ic="fa-stop"/>
      </div>
      <div class="asbutton button-down" data-hotkey="RawSpace"
           data-tip="**CLICK** Start/pause playback" onClick={toggle_play}>
        <Icon fw lg ic="fa-play" hi="ho"/>
      </div>
      <div class="asbutton button-down" disabled onClick={() => dispatch ('-todo-Record')}>
        <Icon fw lg ic="fa-circle"/>
      </div>
      <div class="asbutton button-down" disabled onClick={() => dispatch ('-todo-Forwards')}>
        <Icon fw lg ic="fa-forward"/>
      </div>
      <div class="asbutton button-down" disabled onClick={() => dispatch ('-todo-Next')}>
        <Icon fw lg ic="fa-fast_forward"/>
      </div>
    </ButtonBar>
  );
}
