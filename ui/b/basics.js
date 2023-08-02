// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { JsExtract } from '../little.js';
import * as Util from '../util.js';

// <STYLE/>
JsExtract.scss`
h-flex { @include h-flex(); }
h-flex[inline] { display: inline-flex; }

v-flex { @include v-flex(); }
v-flex[inline] { display: inline-flex; }

c-grid { display: grid; }
c-grid[inline] { display: inline-grid; }
`;

/** # PUSH-BUTTON - wrapper for an ordinary HTMLElement */
class PushButton extends HTMLElement {}
customElements.define ('push-button', PushButton);

/** # B-HFLEX
 * Horizontal [flex](https://developer.mozilla.org/en-US/docs/Web/CSS/flex) container element.
 */
class HFlex extends HTMLElement {}
customElements.define ('h-flex', HFlex);

/** # B-VFLEX
 * Vertical [flex](https://developer.mozilla.org/en-US/docs/Web/CSS/flex) container element.
 */
class VFlex extends HTMLElement {}
customElements.define ('v-flex', VFlex);

/** # B-CGRID
 * Simple [grid](https://developer.mozilla.org/en-US/docs/Web/CSS/grid) container element.
 * See also [Grid Container](https://www.w3.org/TR/css-grid-1/#grid-containers)
 * [Grid visual cheatsheet](http://grid.malven.co/)
 */
class CGrid extends HTMLElement {}
customElements.define ('c-grid', CGrid);