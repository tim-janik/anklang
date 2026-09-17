// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @class PositionView
 * SolidJS component that displays the project
 * transport position pointer and related information.
 */

import { Editable } from './editable';

// <STYLE/>
Extra_css`
@reference "../tailwind.css";
.b-positionview {
  @apply hflex;
  --b-positionview-fg: var(--b-lcdscreen-fg);
  --b-positionview-bg: var(--b-lcdscreen-bg);
  --b-positionview-b0: oklch(from var(--b-positionview-bg) calc(l - 0.01) c h);
  --b-positionview-b1: oklch(from var(--b-positionview-bg) calc(l + 0.01) c h);
  margin: 0; padding: 5px 1em;
  letter-spacing: 0.05em;
  border-radius: var(--b-button-radius); align-items: baseline;
  border-top:    1px solid oklch(from var(--b-positionview-bg) calc(l * 0.97) c h);
  border-left:   1px solid oklch(from var(--b-positionview-bg) calc(l * 0.97) c h);
  border-right:  1px solid oklch(from var(--b-positionview-bg) calc(l * 1.03) c h);
  border-bottom: 1px solid oklch(from var(--b-positionview-bg) calc(l * 1.03) c h);
  background: var(--b-positionview-bg);
  background: linear-gradient(to bottom, var(--b-positionview-b0) 0%, var(--b-positionview-b1) 100%);
  color: var(--b-positionview-fg);
  .b-positionview-counter,
  .b-positionview-timer	{ font-size: 110%; margin-right: .5em; }
  .b-positionview-counter { width: 7em; } /* fixed size reduces layouting during updates */
  .b-positionview-timer	  { width: 7em; } /* fixed size reduces layouting during updates */
}`;

// <SCRIPT/>
import { onMount, onCleanup } from 'solid-js';
import * as Util from '../util.js';

export function PositionView (props: any)
{
  const counter_text = document.createTextNode ('');
  const timer_text = document.createTextNode ('');

  onMount (() => {
    const subscription = App.project.telemetry().then (fields => {
      const names = ['current_bar', 'current_beat', 'current_sixteenth', 'current_minutes', 'current_seconds'];
      return Util.telemetry_subscribe (recv_telemetry, fields.filter (field => names.includes (field.name)));
    });
    onCleanup (() => { subscription.then (Util.telemetry_unsubscribe); });
  });

  function recv_telemetry (tsub: any, arrays: any)
  {
    const ds = "\u2007"; // FIGURE SPACE - "Tabular width", the width of digits
    const s3 = (n: number) => (n >= 100 ? "" : n >= 10 ? ds : ds + ds) + n;
    const s2 = (n: number) => (n >= 10 ? "" : ds) + n;
    const z2 = (n: number | string) => (+n >= 10 ? "" : "0") + n;
    const ff = (n: number, d = 2) => Number.parseFloat (String (n)).toFixed (d);
    // const tick = arrays[tsub.current_tick.type][tsub.current_tick.index];
    // const bpm = arrays[tsub.current_bpm.type][tsub.current_bpm.index];
    const bar = arrays[tsub.current_bar.type][tsub.current_bar.index];
    const beat = arrays[tsub.current_beat.type][tsub.current_beat.index];
    const sixteenth = arrays[tsub.current_sixteenth.type][tsub.current_sixteenth.index];
    const minutes = arrays[tsub.current_minutes.type][tsub.current_minutes.index];
    const seconds = arrays[tsub.current_seconds.type][tsub.current_seconds.index];
    const barpos = s3 (1 + bar) + "." + s2 (1 + beat) + "." + (1 + sixteenth).toFixed (2);
    const timepos = z2 (minutes) + ":" + z2 (ff (seconds, 3));
    if (counter_text.nodeValue != barpos)
      counter_text.nodeValue = barpos;
    if (timer_text.nodeValue != timepos)
      timer_text.nodeValue = timepos;
  }

  function apply_sig (v: string)
  {
    const parts = ("" + v).split ('/');
    if (parts.length == 2) {
      const n = Number (parts[0]), d = Number (parts[1]);
      if (n > 0 && d > 0) {
	App.project.numerator = n;
	App.project.denominator = d;
      }
    }
  }

  const project = App.project;

  return (
    <div class="b-positionview">
      <Editable class="w-16 text-center" onChange={e => apply_sig ((e as CustomEvent).detail.value)} selectall
	value={project.numerator + '/' + project.denominator} />
      <span class="b-positionview-counter">{counter_text}</span>
      <Editable class="w-16 text-center" onChange={e => { project.bpm = 0 | (e as CustomEvent).detail.value }} selectall
	value={project.bpm} />
      <span class="b-positionview-timer">{timer_text}</span>
    </div>
  );
}
