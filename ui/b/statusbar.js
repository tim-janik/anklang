// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** # B-STATUSBAR
 * Area for the display of status messages and UI configuration.
 */

import { LitComponent, html, JsExtract, ref, docs } from '../little.js';
import * as Util from "../util.js";

// == STYLE ==
JsExtract.scss`
b-statusbar {
  display: flex; justify-content: space-between; white-space: nowrap;
  margin: 0; padding: 0 $b-statusbar-field-spacing;
  user-select: none;
  .b-statusbar-field {
    display: flex; flex-wrap: nowrap; flex-shrink: 0; flex-grow: 0; white-space: nowrap;
  }
  .b-statusbar-text {
    display: inline-block; overflow-y: visible; //* avoid scrolling */
    overflow-x: hidden; white-space: nowrap;
    flex-shrink: 1; flex-grow: 1;
    margin-left: calc($b-statusbar-field-spacing * 2);
  }
  .b-statusbar-spacer {
    display: inline; flex-shrink: 1; flex-grow: 0; white-space: nowrap;
    text-align: center;
    margin: 0 $b-statusbar-field-spacing;
    @include b-statusbar-vseparator;
  }
  .b-icon {
    align-items: center;
    padding: 0 $b-statusbar-field-spacing;
    filter: brightness(asfactor($b-statusbar-icon-brightness));
    &:hover:not(.b-active) {
      filter: brightness(div(1.0, asfactor($b-statusbar-icon-brightness)));
      transform: scale($b-statusbar-icon-scaleup);
    }
    &.b-active {
      color: $b-color-active;
      transform: scale($b-statusbar-icon-scaleup);
    }
  }

  /* markdown styling for statusbar */
  .b-statusbar-text { /* .b-markdown-it-outer */
    @include b-markdown-it-inlined;
    color: $b-statusbar-text-shade;
    * {
      display: inline-block; overflow-y: visible; //* avoids scrolling */
      padding: 0; margin: 0; font-size: inherit; white-space: nowrap;
    }
    strong { color: $b-main-foreground; padding: 0 0.5em; @include b-font-weight-normal(); }
    kbd {
      padding: 0 0.4em 1px;
      @include b-kbd-hotkey(true);
    }
  }
}`;

// == HTML ==
const HTML = (t, d) => html`
  <span class="b-statusbar-field" >
    <b-icon ic="mi-equalizer" style="font-size:110%;" hflip class=${App.panel2 == 'd' && 'b-active'}
	    @click=${() => App.switch_panel2 ('d')} data-kbd="^"
	    aria-label="Show Device Stack"
	    data-tip="**CLICK** Show Device Stack"></b-icon>
    <b-icon ic="mi-queue_music" style="font-size:110%" class=${App.panel2 == 'p' && 'b-active'}
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
    <b-icon ic="mi-info"        style="font-size:110%" class=${App.panel3 == 'i' && 'b-active'}
	    @click=${() => App.switch_panel3 ('i')} data-kbd="i"
	    data-tip="**CLICK** Show Information View"></b-icon>
    <b-icon ic="mi-folder_open" style="font-size:110%" class=${App.panel3 == 'b' && 'b-active'}
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
          Util.markdown_to_html (this.statusbar_text, this.status_ = rawmsg);
        const rawkbd = !dtel ? '' : dtel.getAttribute ('data-kbd') || dtel.getAttribute ('data-hotkey');
        if (rawkbd != this.kbd_)
          this.kbd_ = rawkbd;
      }
  }
}
customElements.define ('b-statusbar', BStatusBar);
