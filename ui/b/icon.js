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
 * : A prefixed variant of `fa`, `bc`, `md`, `uc`.
 * : Either a prefixed icon font symbol or a unicode character literal, see
 * : the Unicode [Lists](https://en.wikipedia.org/wiki/List_of_Unicode_characters) and [Symbols](https://en.wikipedia.org/wiki/Unicode_symbols#Symbol_block_list).
 * : The 'bc-' prefix indicates an icon from the "AnklangIcons Font" symbols.
 * : The 'fa-' prefix indicates an icon from the "Fork Awesome" collection (compatible with "Font Awesome 4"), see the [Fork Awesome Icons](https://forkaweso.me/Fork-Awesome/cheatsheet/).
 * : The 'md-' prefix indicates an icon from the "Material Design Icons" collection, see the [Material Design Icons](https://material.io/tools/icons/).
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
Extra_css`
b-icon.nf,
b-icon {
  display: inline-flex;
  place-content: center center;
  flex-wrap: wrap; /* needed for align-content:center */
  &[hflip]		{ transform: scaleX(-1); }
  &[vflip]		{ transform: scaleY(-1); }
  &[hflip][vflip]	{ transform: scaleX(-1) scaleY(-1); }
}`;

// == SCRIPT ==
const BOOL_ATTRIBUTE = { type: Boolean, reflect: true };  // sync attribute with property
const STRING_ATTRIBUTE = { type: String, reflect: true }; // sync attribute with property
const STRING_PROPERTY = { type: String, state: true };

class BIcon extends LitComponent {
  createRenderRoot()
  {
    // avoid using shadow-root which does not have access to icon fonts
    return this;
  }
  render()
  {
    const { iconclasses, md_, uc_ } = this;
    const inner_text = uc_;
    for (let c of this.lastclass_.split (' '))
      !!c && this.classList.remove (c);
    this.lastclass_ = iconclasses;
    for (let c of this.lastclass_.split (' '))
      !!c && this.classList.add (c);
    return inner_text;
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
    this.setAttribute ("aria-hidden", "true");
  }
  get iconclasses()
  {
    let classes = (this.iconclass || '').split (/ +/);
    const nf_ = this.nf_;
    if (nf_) {
      classes.push ("nf");
      classes.push (nf_);
    } else if (this.bc_)
      classes.push ("AnklangIcons-" + this.bc_);
    else
      classes.push ("uc");
    return classes.join (" ");
  }
  get bc_() { return this.ic.startsWith ("bc-") ? this.ic.substr (3) : ''; }
  get nf_()
  {
    const ic_ = this.ic.startsWith ("nf-") ? this.ic.substr (3) : this.ic;
    if (ic_.startsWith ("mi-"))
      return "nf-md-" + ic_.substr (3);
    const prefixes = [
      "cod-", "dev-", "custom-", "extra-", "fa-", "fae-", "iec-",
      "indent-", "indentation-", "linux-", "md-", "oct-", "pl-",
      "ple-", "pom-", "seti-", "weather-"
    ];
    for (const prefix of prefixes)
      if (ic_.startsWith (prefix))
	return "nf-" + ic_;
    return '';
  }
  get uc_()
  {
    const nf = this.nf_;
    if (nf || this.bc_) return '';
    let icon = this.ic;
    if (icon.startsWith ("uc-"))
      icon = icon.substr (3);
    return icon;
  }
}
customElements.define ("b-icon", BIcon);
