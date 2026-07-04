// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import type { JSX } from 'solid-js';
import { splitProps } from 'solid-js';

/** @class MenuSeparator
 * @description
 * The MenuSeparator component renders a horizontal rule for use as a visual separator between other menu elements.
 */

// == STYLE ==
Extra_css`
b-menuseparator, .b-menuseparator {
  margin: calc(1em - 1px) 1em;
  border: 1px solid var(--b-menu-separator);
}
`;

// == COMPONENT ==
export function MenuSeparator (props: JSX.HTMLAttributes<HTMLHRElement>)
{
  const [local, rest] = splitProps (props, ['class']);
  const class_ = () => 'b-menuseparator' + (local.class ? ' ' + local.class : '');
  return <hr {...rest} class={class_()} />;
}
