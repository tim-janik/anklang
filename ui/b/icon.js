// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { LitComponent, html, JsExtract, docs } from '../little.js';
import * as Util from "../util.js";

/** # B-ICON
 * This element displays icons from various icon fonts.
 * In order to style the color of icon font symbols, simply apply the `color` CSS property
 * to this element (styling `fill` as for SVG elements is not needed).
 * ## Props:
 * *iconclass*
 * : A CSS class to apply to this icon.
 * *ic*
 * : A prefixed variant of `fa`, `bc`, `mi`, `uc`.
 * : Either a prefixed icon font symbol or a unicode character literal, see
 * : the Unicode [Lists](https://en.wikipedia.org/wiki/List_of_Unicode_characters) and [Symbols](https://en.wikipedia.org/wiki/Unicode_symbols#Symbol_block_list).
 * : The 'bc-' prefix indicates an icon from the "AnklangIcons Font" symbols.
 * : The 'fa-' prefix indicates an icon from the "Fork Awesome" collection (compatible with "Font Awesome 4"), see the [Fork Awesome Icons](https://forkaweso.me/Fork-Awesome/cheatsheet/).
 * : The 'mi-' prefix indicates an icon from the "Material Icons" collection, see the [Material Design Icons](https://material.io/tools/icons/).
 * *nosize*
 * : Prevent the element from applying default size constraints.
 * *fw*
 * : Apply fixed-width sizing.
 * *lg*
 * : Make the icon 33% larger than its container.
 * *hflip*
 * : Flip the icon horizontally.
 * *vflip*
 * : Flip the icon vertically.
 */

// == STYLE ==
const STYLE = await JsExtract.fetch_css (import.meta);
JsExtract.scss`
b-icon { // not using shadow-root here
  display: inline-flex; justify-content: center;
  margin: 0; padding: 0; line-height: 1;
  &[fw]			{ height: 1em; width: 1.28571429em; text-align: center; }
  &[lg]			{ font-size: 1.33333333em; }
  &[hflip]		{ transform: scaleX(-1); }
  &[vflip]		{ transform: scaleY(-1); }
  &[hflip][vflip]	{ transform: scaleX(-1) scaleY(-1); }
  &[nosize]		{ height: 1em; width: 1em; }
  &[ic^=mi-][nosize]	{ font-size: 1em; }
  .material-icons       { font-size: 1.28em; }
}`;

// == HTML ==
const HTML = (iconclasses, innertext) => html`
  <i class="${iconclasses}" role="icon" aria-hidden="true" >${innertext}</i>
`;

// == SCRIPT ==
const BOOL_ATTRIBUTE = { type: Boolean, reflect: true };  // sync attribute with property
const STRING_ATTRIBUTE = { type: String, reflect: true }; // sync attribute with property

class BIcon extends LitComponent {
  static properties = {
    iconclass: STRING_ATTRIBUTE, ic: STRING_ATTRIBUTE,
    hflip: BOOL_ATTRIBUTE, vflip: BOOL_ATTRIBUTE,
    fw: BOOL_ATTRIBUTE, lg: BOOL_ATTRIBUTE,
    nosize: undefined,
  };
  constructor()
  {
    super();
    this.ic = "";
    this.fw = false;
    this.lg = false;
    this.hflip = false;
    this.vflip = false;
    this.nosize = false;
  }
  render()
  {
    const { iconclasses, mi_, uc_ } = this;
    let inner = mi_ || uc_;
    return HTML (iconclasses, inner);
  }
  createRenderRoot()
  {
    Util.adopt_style (this, STYLE);
    Util.add_style_sheet (this, '/fonts/AnklangIcons.css');
    Util.add_style_sheet (this, '/fonts/material-icons.css');
    Util.add_style_sheet (this, '/fonts/forkawesome-webfont.css');
    // avoid using shadow-root which does not have access to icon fonts
    return this;
  }
  get iconclasses()
  {
    let classes = (this.iconclass || '').split (/ +/);
    if (this.mi_)
      {
	classes.push ('mi');
	classes.push ('material-icons');
      }
    else if (this.fa_)
      {
	classes.push ('fa');
	classes.push ('fa-' + this.fa_);
      }
    else if (this.bc_)
      {
	classes.push ('bc');
	classes.push ('AnklangIcons-' + this.bc_);
      }
    else
      classes.push ('uc');
    return classes.join (' ');
  }
  get bc_() { return this.ic.startsWith ('bc-') ? this.ic.substr (3) : undefined; }
  get fa_() { return this.ic.startsWith ('fa-') ? this.ic.substr (3) : undefined; }
  get mi_() { return this.ic.startsWith ('mi-') ? this.ic.substr (3) : undefined; }
  get uc_()
  {
    if (this.ic.startsWith ('bc-') || this.ic.startsWith ('fa-') || this.ic.startsWith ('mi-'))
      return undefined;
    let icon = this.ic;
    if (icon.startsWith ('uc-'))
      icon = icon.substr (3);
    return icon;
  }
}
customElements.define ('b-icon', BIcon);
