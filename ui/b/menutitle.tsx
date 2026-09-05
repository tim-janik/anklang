// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import type { JSX } from 'solid-js';
import { splitProps } from 'solid-js';

/** @class MenuTitle
 * @description
 * The MenuTitle component can be used as menu title inside a
 * [ContextMenu](#ContextMenu).
 * ### Children:
 * : All children will be rendered as contents of this element.
 */

// == STYLE ==
Extra_css`
b-menutitle, .b-menutitle {
  display: inline-flex; flex: 0 0 auto; flex-flow: row nowrap;
  align-items: baseline;      /* distribute extra cross-axis space */
  margin: 0; padding: 5px 1em; text-align: left;
  background: transparent; cursor: pointer; user-select: none;
  border: 1px solid transparent;
  color: var(--b-menu-foreground);
  font-variant: small-caps; @include b-font-weight-bolder();
  /* InterVariable-4.0 has broken c2sc (small-caps), https://github.com/rsms/inter/issues/556#issuecomment-1598010623 */
  text-transform: uppercase; font-size: 80%;
  ::first-letter {
    font-size: 130%;
  }
}`;

// == COMPONENT ==
export function MenuTitle (props: JSX.HTMLAttributes<HTMLDivElement>)
{
  const [local, rest] = splitProps (props, ['children', 'class']);
  const class_ = () => 'b-menutitle' + (local.class ? ' ' + local.class : '');
  return (
    <div {...rest} class={class_()}>
      <span class="menulabel">{local.children}</span>
    </div>
  );
}
