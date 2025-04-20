// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { JsExtract } from '../little.js';
import * as Util from '../util.js';

/** @class BPushButton
 * @description
 * The .asbutton class is a wrapper for an ordinary HTMLElement. It is styled
 * like a <button> and can behave like it, but cannot become a focus element.
 */
class BPushButton extends HTMLElement {}
customElements.define ('push-button', BPushButton);
