// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { LitComponent, html, JsExtract, docs } from '../little.js';
import * as Util from "../util.js";
import * as Dom from "../dom.js";

/** @class BIcon
 * @description
 * The <b-icon> element displays icons from various icon fonts.
 * In order to style the color of icon font symbols, simply apply the `color` CSS property
 * to this element (styling `fill` as for SVG elements is not needed).
 * ### Props:
 * *iconclass*
 * : A CSS class to apply to this icon.
 * *ic*
 * : A prefixed variant of `fa`, `bc`, `mi`, `uc`.
 * : Either a prefixed icon font symbol or a unicode character literal, see
 * : the Unicode [Lists](https://en.wikipedia.org/wiki/List_of_Unicode_characters) and [Symbols](https://en.wikipedia.org/wiki/Unicode_symbols#Symbol_block_list).
 * : The 'bc-' prefix indicates an icon from the "AnklangIcons Font" symbols.
 * : The 'fa-' prefix indicates an icon from the "Fork Awesome" collection (compatible with "Font Awesome 4"), see the [Fork Awesome Icons](https://forkaweso.me/Fork-Awesome/cheatsheet/).
 * : The 'mi-' prefix indicates an icon from the "Material Icons" collection, see the [Material Design Icons](https://material.io/tools/icons/).
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
JsExtract.scss`
b-icon { // not using shadow-root here
  display: inline-flex !important;
  place-content: center center;
  flex-wrap: wrap; /* needed for align-content:center */
  &[hflip]		{ transform: scaleX(-1); }
  &[vflip]		{ transform: scaleY(-1); }
  &[hflip][vflip]	{ transform: scaleX(-1) scaleY(-1); }
  &[ic^="mi-"]		{ font-size: 1.28em; }
}`;

// == SCRIPT ==
const BOOL_ATTRIBUTE = { type: Boolean, reflect: true };  // sync attribute with property
const STRING_ATTRIBUTE = { type: String, reflect: true }; // sync attribute with property
const STRING_PROPERTY = { type: String, state: true };

class BIcon extends LitComponent {
  createRenderRoot()
  {
    // icon fonts must already be loaded into document and shadowRoot ancestors
    verify_font_family();
    // avoid using shadow-root which does not have access to icon fonts
    return this;
  }
  render()
  {
    const { iconclasses, mi_, uc_ } = this;
    this.innerText = mi_ || uc_;
    for (let c of this.lastclass_.split (' '))
      !!c && this.classList.remove (c);
    this.lastclass_ = iconclasses;
    for (let c of this.lastclass_.split (' '))
      !!c && this.classList.add (c);
    return null;
  }
  static properties = {
    iconclass: STRING_PROPERTY,
    hflip: BOOL_ATTRIBUTE,
    vflip: BOOL_ATTRIBUTE,
    ic: STRING_ATTRIBUTE,
    fw: BOOL_ATTRIBUTE,
    lg: BOOL_ATTRIBUTE,
  };
  constructor()
  {
    super();
    this.ic = "";
    this.fw = false;
    this.lg = false;
    this.hflip = false;
    this.vflip = false;
    this.iconclass = "";
    this.lastclass_ = '';
  }
  connectedCallback()
  {
    super.connectedCallback();
    this.role = "icon";
    this.setAttribute ('aria-hidden', 'true');
  }
  get iconclasses()
  {
    let classes = (this.iconclass || '').split (/ +/);
    if (this.mi_)
      {
	classes.push ('material-icons');
      }
    else if (this.fa_)
      {
	classes.push ('fa');
	classes.push ('fa-' + this.fa_);
      }
    else if (this.bc_)
      {
	classes.push ('AnklangIcons-' + this.bc_);
      }
    else
      classes.push ('uc');
    return classes.join (' ');
  }
  get bc_() { return this.ic.startsWith ('bc-') ? this.ic.substr (3) : ''; }
  get fa_() { return this.ic.startsWith ('fa-') ? this.ic.substr (3) : ''; }
  get mi_() { return this.ic.startsWith ('mi-') ? this.ic.substr (3) : ''; }
  get uc_()
  {
    if (this.ic.startsWith ('bc-') || this.ic.startsWith ('fa-') || this.ic.startsWith ('mi-'))
      return '';
    let icon = this.ic;
    if (icon.startsWith ('uc-'))
      icon = icon.substr (3);
    return icon;
  }
}
customElements.define ('b-icon', BIcon);

function verify_font_family (testnow = false)
{
  if (verify_font_family_status === undefined) {
    verify_font_family_status = setTimeout (() => verify_font_family (true), 3 * 1000);
    return undefined;
  } else if (!testnow)
    return undefined;
  const Material_Icons_loaded = Dom.font_family_loaded ({ font_family: 'Material Icons' });
  console.assert (Material_Icons_loaded, 'Failed to verify loaded font: "Material Icons"');

  const AnklangIcons_loaded = Dom.font_family_loaded ({ font_family: 'AnklangIcons', text: '\uea01\uea02\uea03\uea04\uea05\uea06' });
  console.assert (AnklangIcons_loaded, 'Failed to verify loaded font: AnklangIcons');

  const ForkAwesome_loaded = Dom.font_family_loaded ({ font_family: 'ForkAwesome', text: '\uf011\uf040\uf05a' });
  console.assert (ForkAwesome_loaded, 'Failed to verify loaded font: ForkAwesome');

  verify_font_family_status = !!(Material_Icons_loaded && AnklangIcons_loaded && ForkAwesome_loaded);
}
let verify_font_family_status = undefined;
