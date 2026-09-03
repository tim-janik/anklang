// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-StatusBar
 * SolidJS component that displays status messages and panel-switcher icons.
 */

import { createSignal, onMount, onCleanup } from 'solid-js';
import * as Util from "../util.js";
import * as Dom from "../dom.js";
import { Icon } from './icon';

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
.b-statusbar {
  @apply hflex m-0 h-6 select-none justify-between whitespace-nowrap px-4 py-0;
  .b-statusbar-field {
    display: flex; flex-wrap: nowrap; flex-shrink: 0; flex-grow: 0; white-space: nowrap;
  }
  .b-statusbar-text {
    display: inline-block; overflow: hidden visible; /* avoid scrolling */
    white-space: nowrap;
    flex-shrink: 1; flex-grow: 1;
    margin-left: calc(var(--b-statusbar-field-spacing) * 2);
  }
  .b-statusbar-spacer {
    display: inline; flex-shrink: 1; flex-grow: 0; white-space: nowrap;
    text-align: center;
    margin: 0 var(--b-statusbar-field-spacing);
    @include b-statusbar-vseparator;
  }
  .b-icon {
    align-items: center;
    padding: 0 var(--b-statusbar-field-spacing);
    filter: brightness(var(--b-statusbar-icon-brightness));
    &:hover:not(.b-active) {
      filter: brightness(calc(1.0 / var(--b-statusbar-icon-brightness)));
      transform: scale(var(--b-statusbar-icon-scaleup));
    }
    &.b-active {
      color: var(--b-color-active);
      transform: scale(var(--b-statusbar-icon-scaleup));
    }
  }

  /* markdown styling for statusbar */
  .b-statusbar-text { /* .b-markdown-it-outer */
    @include b-markdown-it-inlined;
    color: var(--b-statusbar-text-shade);
    * {
      display: inline-block; overflow-y: visible; /* avoids scrolling */
      padding: 0; margin: 0; font-size: inherit; white-space: nowrap;
    }
    strong { color: var(--b-main-foreground); padding: 0 0.5em; @include b-font-weight-normal(); }
    kbd {
      padding: 0 0.4em 1px;
      @include b-kbd-hotkey(true);
    }
  }
}`;

// == Component ==
export function StatusBar (props)
{
  const [kbd, set_kbd] = createSignal ('');
  const [status_msg, set_status_msg] = createSignal ('');
  let statusbar_text; // DOM ref for markdown rendering

  onMount (() =>
    {
      const seen_move = (event) =>
	    {
	      if (!event.buttons)
	      {
		// const els = document.body.querySelectorAll ('*:hover[data-tip]');
		const dtel = Util.find_element_from_point (document, event.clientX, event.clientY,
							   el => !!el.getAttribute ('data-tip'));
		const rawmsg = dtel ? dtel.getAttribute ('data-tip') : '';
		if (rawmsg != status_msg ())
		{
		  set_status_msg (rawmsg);
		  Dom.markdown_to_html (statusbar_text, rawmsg);
		}
		const rawkbd = !dtel ? '' : dtel.getAttribute ('data-kbd') || dtel.getAttribute ('data-hotkey');
		if (rawkbd != kbd ())
		  set_kbd (rawkbd);
	      }
	    };
      const zmove_del = App.zmoves_add (seen_move);
      onCleanup (zmove_del);
    });

  return (
    <div class={props.class ? 'b-statusbar ' + props.class : 'b-statusbar'}>
      <span class="b-statusbar-field">
        <Icon ic="md-equalizer" style="font-size:110%" hflip
		classList={{ 'b-active': Shell.r.panel2 == 'd' }}
		onClick={() => App.switch_panel2 ('d')} data-kbd="^"
		aria-label="Show Device Stack"
		data-tip="**CLICK** Show Device Stack"/>
        <Icon ic="md-playlist_music" style="font-size:110%"
		classList={{ 'b-active': Shell.r.panel2 == 'p' }}
		onClick={() => App.switch_panel2 ('p')} data-kbd="^"
		aria-label="Show Piano Roll Editor"
		data-tip="**CLICK** Show Piano Roll Editor"/>
      </span>
      <span class="b-statusbar-spacer"></span>
      <span class="b-statusbar-text" ref={statusbar_text}></span>
      <span class="b-statusbar-text"
	    style={{ 'flex-grow': 0, 'margin': '0 0.5em', 'display': kbd () ? '' : 'none' }}>
	<strong>KEY</strong> <kbd>{ Util.display_keyname (kbd ()) }</kbd>
      </span>
      <span class="b-statusbar-spacer"></span>
      <span class="b-statusbar-field">
        <Icon ic="md-info" style="font-size:110%"
		classList={{ 'b-active': Shell.r.panel3 == 'i' }}
		onClick={() => App.switch_panel3 ('i')} data-kbd="i"
		aria-label="Show Information View"
		data-tip="**CLICK** Show Information View"/>
        <Icon ic="md-folder_open" style="font-size:110%"
		classList={{ 'b-active': Shell.r.panel3 == 'b' }}
		onClick={() => App.switch_panel3 ('b')} data-kbd="i"
		aria-label="Show Browser"
		data-tip="**CLICK** Show Browser"/>
      </span>
    </div>
  );
};
