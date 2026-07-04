// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class MenuRow
 * @description
 * The MenuRow component can contain `<button/>` menu items of a [ContextMenu](#ContextMenu),
 * that are packed horizontally inside a menurow.
 *
 * ### Props:
 * *noturn*
 * : Avoid turning the icon-label direction in menu items to be upside down.
 *
 * ### Children:
 * : All children will be rendered as contents of this element.
 */

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
b-menurow, .b-menurow {
  @apply m-0 flex flex-initial items-baseline justify-center text-center;
  flex-flow: row nowrap;
}`;

// == COMPONENT ==
export function MenuRow (props: {
  noturn?: boolean;
  children?: any;
})
{
  return (
    <div class="b-menurow" classList={{ noturn: props.noturn }}>
      {props.children}
    </div>
  );
}
