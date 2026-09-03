// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @class Icon
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
 * *aria-label* : Provide an accessible name, otherwise the icon is hidden from assistive technology.
 */

import { splitProps } from 'solid-js';

// == STYLE ==
Extra_css`
b-icon, .b-icon.nf,
.b-icon {
  display: inline-flex;
  place-content: center center;
  flex-wrap: wrap; /* needed for align-content:center */
  &[hflip]		{ transform: scaleX(-1); }
  &[vflip]		{ transform: scaleY(-1); }
  &[hflip][vflip]	{ transform: scaleX(-1) scaleY(-1); }
}`;

// == Component ==
export function Icon (props: any)
{
  const [local, rest] = splitProps (props, ['class', 'classList']);
  const ic_val = () => props.ic ?? '';
  const iconclass_val = () => props.iconclass ?? '';

  const all_classes = () => {
    const parts = ['b-icon', ...iconclasses (ic_val (), iconclass_val ()).split (' ').filter (Boolean)];
    if (local.class) parts.push (local.class);
    if (local.classList) {
      for (const [k, v] of Object.entries (local.classList)) {
        if (v) parts.push (k);
      }
    }
    return parts.join (' ');
  };

  const inner = () => inner_text (ic_val ());
  const labelled = () => !!props['aria-label'];

  return (
    <span class={all_classes ()}
          role={labelled () ? 'img' : undefined}
          aria-hidden={labelled () ? undefined : true} {...rest}>
      {inner ()}
    </span>
  );
}

/** Create a <span class="b-icon"> element for imperative DOM use. */
export function icon_element (ic: string): HTMLSpanElement
{
  const el = document.createElement ('span');
  el.className = 'b-icon ' + iconclasses (ic, '');
  el.setAttribute ('aria-hidden', 'true');
  el.setAttribute ('ic', ic);
  const text = inner_text (ic);
  if (text) el.textContent = text;
  return el;
}

// == Helpers ==
const PREFIXES = [
  "cod-", "dev-", "custom-", "extra-", "fa-", "fae-", "iec-",
  "indent-", "indentation-", "linux-", "md-", "oct-", "pl-",
  "ple-", "pom-", "seti-", "weather-"
];

function nf (ic: string): string
{
  const ic_ = ic.startsWith ("nf-") ? ic.substr (3) : ic;
  if (ic_.startsWith ("mi-"))
    return "nf-md-" + ic_.substr (3);
  for (const prefix of PREFIXES)
    if (ic_.startsWith (prefix))
      return "nf-" + ic_;
  return '';
}

function iconclasses (ic: string, iconclass: string | undefined): string
{
  let classes = (iconclass || '').split (/ +/);
  const nf_ = nf (ic);
  if (nf_) {
    classes.push ("nf");
    classes.push (nf_);
  } else {
    const bc_ = ic.startsWith ("bc-") ? ic.substr (3) : '';
    if (bc_)
      classes.push ("AnklangIcons-" + bc_);
    else
      classes.push ("uc");
  }
  return classes.join (" ");
}

function inner_text (ic: string): string
{
  const nf_ = nf (ic);
  const bc = ic.startsWith ("bc-") ? ic.substr (3) : '';
  if (nf_ || bc) return '';
  let icon = ic;
  if (icon.startsWith ("uc-"))
    icon = icon.substr (3);
  return icon;
}
