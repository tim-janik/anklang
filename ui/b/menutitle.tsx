// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BMenuTitle
 * @description
 * The <b-menutitle> element can be used as menu title inside a
 * [BContextMenu](contextmenu_8js.html#BContextMenu).
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
export function MenuTitle (props: {
  style?: string;
  children?: any;
})
{
  return (
    <div class="b-menutitle" style={props.style}>
      <span class="menulabel">{props.children}</span>
    </div>
  );
}
