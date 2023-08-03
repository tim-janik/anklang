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

/** @class BPushButton
 * @description
 * The <push-button> element is a wrapper for an ordinary HTMLElement. It is styled
 * like a <button> and can behave like it, but cannot become a focus element.
 */
class BPushButton extends HTMLElement {}
customElements.define ('push-button', BPushButton);

/** @class BHFlex
 * @description
 * The <h-flex> element is a horizontal [flex](https://developer.mozilla.org/en-US/docs/Web/CSS/flex) container element.
 */
class BHFlex extends HTMLElement {}
customElements.define ('h-flex', BHFlex);

/** @class BVFlex
 * @description
 * The <v-flex> element is a vertical [flex](https://developer.mozilla.org/en-US/docs/Web/CSS/flex) container element.
 */
class BVFlex extends HTMLElement {}
customElements.define ('v-flex', BVFlex);

/** @class BGrid
 * @description
 * The <c-grid> element is a simple [grid](https://developer.mozilla.org/en-US/docs/Web/CSS/grid) container element.
 * See also [Grid Container](https://www.w3.org/TR/css-grid-1/#grid-containers) and the
 * [Grid visual cheatsheet](http://grid.malven.co/).
 */
class BGrid extends HTMLElement {}
customElements.define ('c-grid', BGrid);
