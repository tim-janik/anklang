// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BStatusBar
 * @description
 * The <b-statusbar> element is an area for the display of status messages and UI configuration.
 */

import { LitComponent, html, JsExtract, ref, docs } from '../little.js';
import * as Util from "../util.js";
import * as Dom from "../dom.js";

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
b-statusbar {
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

// == HTML ==
const HTML = (t, d) => html`
  <span class="b-statusbar-field" >
    <b-icon ic="md-equalizer" style="font-size:110%;" hflip class=${App.panel2 == 'd' && 'b-active'}
	    @click=${() => App.switch_panel2 ('d')} data-kbd="^"
	    aria-label="Show Device Stack"
	    data-tip="**CLICK** Show Device Stack"></b-icon>
    <b-icon ic="md-playlist_music" style="font-size:110%" class=${App.panel2 == 'p' && 'b-active'}
	    @click=${() => App.switch_panel2 ('p')} data-kbd="^"
	    aria-label="Show Piano Roll Editor"
	    data-tip="**CLICK** Show Piano Roll Editor"></b-icon>
  </span>
  <span class="b-statusbar-spacer" ></span>
  <span class="b-statusbar-text" ${ref (h => t.statusbar_text = h)} ></span>
  <span class="b-statusbar-text" style="flex-grow: 0; margin: 0 0.5em; ${!t.kbd_ && 'display:none'}" >
    <strong>KEY</strong> <kbd>${ Util.display_keyname (t.kbd_) }</kbd> </span>
  <span class="b-statusbar-spacer" ></span>
  <span class="b-statusbar-field" >
    <b-icon ic="md-info"        style="font-size:110%" class=${App.panel3 == 'i' && 'b-active'}
	    @click=${() => App.switch_panel3 ('i')} data-kbd="i"
	    data-tip="**CLICK** Show Information View"></b-icon>
    <b-icon ic="md-folder_open" style="font-size:110%" class=${App.panel3 == 'b' && 'b-active'}
	    @click=${() => App.switch_panel3 ('b')} data-kbd="i"
	    data-tip="**CLICK** Show Browser"></b-icon>
  </span>
`;

// == SCRIPT ==
const PRIVATE_PROPERTY = { state: true };

class BStatusBar extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  static properties = {
    kbd_: PRIVATE_PROPERTY,
  };
  constructor()
  {
    super();
    this.kbd_ = '';
    this.statusbar_text = null;
  }
  connectedCallback()
  {
    super.connectedCallback();
    this.zmovedel = App.zmoves_add (this.seen_move.bind (this));
  }
  disconnectedCallback()
  {
    super.disconnectedCallback();
    this.zmovedel();
    this.zmovedel = null;
  }
  seen_move (event)
  {
    if (!event.buttons)
      {
        // const els = document.body.querySelectorAll ('*:hover[data-tip]');
	const dtel = Util.find_element_from_point (document, event.clientX, event.clientY, el => !!el.getAttribute ('data-tip'));
        const rawmsg = dtel ? dtel.getAttribute ('data-tip') : '';
        if (rawmsg != this.status_)
          Dom.markdown_to_html (this.statusbar_text, this.status_ = rawmsg);
        const rawkbd = !dtel ? '' : dtel.getAttribute ('data-kbd') || dtel.getAttribute ('data-hotkey');
        if (rawkbd != this.kbd_)
          this.kbd_ = rawkbd;
      }
  }
}
customElements.define ('b-statusbar', BStatusBar);
