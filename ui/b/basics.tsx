// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @class BPushButton
 * @description
 * The <push-button/> element class is a wrapper for an ordinary HTMLElement. It is styled
 * like a <button> and can behave like it, but cannot become a focus element.
 */

// == Component ==
export function PushButton (props: any)
{
  return (
    <div class="asbutton" {...props}>{props.children}</div>
  );
}
