// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

import { type JSX, splitProps } from 'solid-js';

// == STYLE ==
Extra_css`
.b-buttonbar {
  display: inline-flex; background-color: var(--b-button-border);
  border: 1px solid var(--b-button-border);
  border-radius: var(--b-button-radius);
  > button, > .asbutton {
    margin: 0 0 0 1px;
    &:first-of-type	{ margin-left: 0; }
    &:first-of-type	{ border-top-left-radius: var(--b-button-radius); border-bottom-left-radius: var(--b-button-radius); }
    &:last-of-type	{ border-top-right-radius: var(--b-button-radius); border-bottom-right-radius: var(--b-button-radius); }
  }
}`;

// == COMPONENT ==
/** @class ButtonBar
 * @description
 * The ButtonBar component is a container for tight packing of buttons.
 * ### Children:
 * : All children passed to this component will be packed tightly and styled as buttons.
 */
export function ButtonBar (props: JSX.HTMLAttributes<HTMLDivElement>)
{
  const [local, others] = splitProps (props, ['class', 'children']);
  const class_name = 'b-buttonbar' + (local.class ? ' ' + local.class : '');
  return <div class={class_name} {...others}>{local.children}</div>;
}
