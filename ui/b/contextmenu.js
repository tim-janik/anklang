// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BContextMenu
 * @description
 * The <b-contextmenu> element implements a modal popup that displays contextmenu choices,
 * see [BMenuItem](menuitem_8js.html#BMenuItem), [BMenuRow](menurow_8js.html#BMenuRow),
 * [BMenuTitle](menutitle_8js.html#BMenuTitle) and [BMenuSeparator](menuseparator_8js.html#BMenuSeparator).
 * Using the `popup()` method, the menu can be popped up from the parent component,
 * and setting up a `.activate` handler can be used to handle menuitem actions. Example:
 * ```html
 * <div @contextmenu="e => querySelector('b-contextmenu').popup (e)">
 *   <b-contextmenu .activate="menuactivation">...</b-contextmenu>
 * </div>
 * ```
 * Note that keyboard presses, mouse clicks, drag selections and event bubbling can
 * all cause menu item clicks and contextmenu activation.
 * In order to deduplicate multiple events that arise from the same user interaction,
 * *one* popup request and *one* click activation is processed per animation frame.
 *
 * ### Properties:
 * *.activate(uri)*
 * : Property callback which is called once a menu item is activated.
 * *.isactive (uri)* -> bool
 * : Property callback used to check if a particular menu item should stay active or be disabled.
 * ### Attributes:
 * *xscale*
 * : Consider a wider area than the context menu width for popup positioning.
 * *yscale*
 * : Consider a taller area than the context menu height for popup positioning.
 * ### Events:
 * *click (event)*
 * : Event signaling activation of a menu item, the `uri` can be found via `event.target.uri`.
 * *close (event)*
 * : Event signaling closing of the menu, regardless of whether menu item activation occoured or not.
 * ### Methods:
 * *popup (event, { origin, focus_uri, data-contextmenu })*
 * : Popup the contextmenu, propagation of `event` is halted and the event coordinates or target is
 * : used for positioning unless `origin` is given.
 * : The `origin` is a reference DOM element to use for drop-down positioning.
 * : The `focus_uri` is a <b-menuitem/> URI to receive focus after popup.
 * : The `data-contextmenu` element (or `origin`) has the `data-contextmenu=true` attribute assigned during popup.
 * *close()*
 * : Hide the contextmenu.
 * *map_kbd_hotkeys (active)*
 * : Activate (deactivate) the `kbd=...` hotkeys specified in menu items.
 */

import { LitComponent, html, render, noChange, JsExtract, docs, ref } from '../little.js';
import * as Util from "../util.js";
import * as Kbd from '../kbd.js';

// == STYLE ==
JsExtract.scss`
b-contextmenu {
  display: contents; /* avoids interfering when inside a flexbox with justify-content:space-between */
}
dialog.b-contextmenu {
  flex-direction: column;
  align-items: stretch;
  justify-content: flex-start;
  margin: 0;
  padding: $b-menu-padding 0;
  color: $b-menu-foreground;
  background-color: $b-menu-background; border: 1px outset zmod($b-menu-background, Jz-=20%);
  box-shadow: $b-menu-box-shadow;
  overflow-y: auto;
  overflow-x: hidden;
  &[open], &.animating { display: flex; }
  [disabled], [disabled] * { pointer-events: none; }
}
dialog.b-contextmenu::backdrop {
  /* Menu backdrop must be transparent, for one a popup menu is different from a modal dialog,
   * and second, showing a modal dialog via menu item would result in bad flickernig. */
  background: transparent;
}`;

// == HTML ==
const HTML = (t, d) => html`
  <dialog class="b-contextmenu" ${ref (h => t.dialog = h)} part="dialog">
    <slot></slot>
  </dialog>
`;

// == SCRIPT ==
export function valid_uri (uri) {
  return !(uri === '' || uri === undefined || uri === null); // allow uri=0
}

/** @this{any} */
function menuitem_isdisabled ()
{
  if (valid_uri (this.uri) && undefined !== this.menudata.checkeduris[this.uri])
    return !this.menudata.checkeduris[this.uri];
  if (Object.hasOwnProperty.call (this, 'disabled') &&
      (this.disabled === "" || !!this.disabled))
    return true;
  const disabled_attr = this instanceof Element ? this.getAttribute ('disabled') : null;
  if (disabled_attr != undefined && disabled_attr != null)
    return true;
  if (this.$attrs && (this.$attrs['disabled'] === "" || !!this.$attrs['disabled']))
    return true;
  return false;
}

const BOOL_ATTRIBUTE = { type: Boolean, reflect: true }; // sync attribute with property
const FUNCTION_ATTRIBUTE = { type: Function, reflect: false };
const STRING_ATTRIBUTE = { type: String, reflect: true }; // sync attribute with property
const NUMBER_ATTRIBUTE = { type: Number, reflect: true }; // sync attribute with property
const PRIVATE_PROPERTY = { state: true };

export function provide_menudata (element)
{
  // find b-contextmenu
  const b_contextmenu = Util.closest (element, 'b-contextmenu');
  if (b_contextmenu && b_contextmenu.menudata)
    return b_contextmenu.menudata;
  // fallback
  return { close: () => undefined,
	   isactive: (uri) => true,
	   menu_stamp: 0,	// deduplicating frame_stamp() for contextmenu
	   item_stamp: 0,	// deduplicating frame_stamp() for menuitem
	   mapname: '',
	   showicons: true,
  };
}

class BContextMenu extends LitComponent {
  // If FOUC becomes an issue, add this as a bandaid:
  // static styles = [ css` dialog[open] { display: flex; flex-direction: column; margin: 0; } ` ];
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  static properties = {
    activate: FUNCTION_ATTRIBUTE,
    isactive: FUNCTION_ATTRIBUTE,
    showicons: BOOL_ATTRIBUTE,
    mapname: STRING_ATTRIBUTE,
    check: { type: Function, state: true },
    xscale: NUMBER_ATTRIBUTE,
    yscale: NUMBER_ATTRIBUTE,
    reposition: PRIVATE_PROPERTY,
  };
  constructor()
  {
    super();
    this.body_div = null;
    this.showicons = true;
    this.emit_close_ = 0;
    this.page_x = undefined;
    this.page_y = undefined;
    this.xscale = 1;
    this.yscale = 1;
    this.origin = null;
    this.focus_uri = '';
    this.data_contextmenu = null;
    this.checkuri = () => true;
    this.reposition = false;
    this.isdisabled = menuitem_isdisabled;
    this.keymap_ = [];
    this.dialog_ = null;
    this.activate = null;
    this.isactive = null;
    // context for descendant menuitems
    this.menudata = provide_menudata (this);
    this.menudata.close = this.close.bind (this);
    this.menudata.isactive = uri => valid_uri (uri) && (!this.isactive || this.isactive (uri));
    // prevent clicks bubbeling up
    this.allowed_click = null;
    this.onclick = this.bubbeling_click_.bind (this);
  }
  get open ()       { return this.dialog_ && this.dialog_.open; }
  get dialog ()     { return this.dialog_; }
  set dialog (dialog) {
    if (this.dialog_)
      {
	this.dialog_.onclose = null;
	this.dialog_.onkeydown = null;
	this.dialog_.onanimationend = null;
	Util.dialog_backdrop_autoclose (this.dialog_, false);
      }
    this.dialog_ = dialog;
    if (this.dialog_)
      {
	this.dialog_.onclose = this.menudata.close;
	this.dialog_.onkeydown = ev => {
	  if (ev.keyCode === 27 && // Escape
	      dialog.open && dialog.matches ('[open]:modal'))
	    ev.stopPropagation(); // stop bubbling
	};
	this.dialog_.onanimationend = ev => {
	  dialog.classList.remove ('animating');
	};
	Util.dialog_backdrop_autoclose (this.dialog_, true);
      }
  }
  connectedCallback() {
    super.connectedCallback();
  }
  disconnectedCallback() {
    super.disconnectedCallback();
    this.map_kbd_hotkeys (false);
  }
  updated()
  {
    if (this.reposition)
      {
	this.reposition = false;
	const p = Util.popup_position (this.dialog, { origin: this.origin, focus_uri: this.focus_uri,
						      x: this.page_x, y: this.page_y, xscale: this.xscale, yscale: this.yscale, });
	this.dialog.style.left = p.x + "px";
	this.dialog.style.top = p.y + "px";
	// chrome does auto-focus for showModal(), make FF behave the same
	Util.move_focus ('HOME');
      }
  }
  popup (event, popup_options = {})
  {
    // stop other user actions following modal popup
    Util.prevent_event (event);
    if (!popup_options)
      popup_options = { origin: null };
    if (this.dialog?.open || Util.frame_stamp() == this.menudata.menu_stamp)
      return false;     // duplicate popup request, only popup once per frame
    const origin = popup_options.origin === null ? null : popup_options.origin?.$el || popup_options.origin || event?.currentTarget;
    if (origin instanceof Element && Util.inside_display_none (origin))
      return false;     // cannot popup around hidden origin
    this.focus_uri = popup_options.focus_uri || '';
    this.origin = origin instanceof Element ? origin : null;
    this.menudata.menu_stamp = Util.frame_stamp();  // allows one popup per frame
    if (event && event.pageX && event.pageY)
      {
	this.page_x = event.pageX;
	this.page_y = event.pageY;
      }
    else
      this.page_x = this.page_y = undefined;
    this.data_contextmenu = popup_options['data-contextmenu'] || this.origin;
    this.data_contextmenu?.setAttribute ('data-contextmenu', 'true');
    this.emit_close_++;
    return (async () => {
      await this.updateComplete; // needed to access this.dialog
      this.reposition = true;
      this.dialog.showModal();
      App.zmove(); // force changes to be picked up
      // check items (and this used to handle auto-focus)
      const focussable = await this.check_isactive (this.focus_uri);
      this.focus_uri = '';
      if (focussable)
	focussable.focus();
    }) ();
  }
  async check_isactive (finduri = null)
  {
    const w = document.createTreeWalker (this, NodeFilter.SHOW_ELEMENT);
    let hasuri = null, e, a = [];
    while ( (e = w.nextNode()) ) {
      /**@type{any}*/ const any = e;
      if (any.check_isactive) {
	if (any.uri === finduri)
	  hasuri = any;
	a.push (any.check_isactive());
      }
    }
    await Promise.all (a);
    return hasuri;
  }
  bubbeling_click_ (event)
  {
    // event is this.click, not bubbeling
    if (this.allowed_click === event)
      return;
    // prevent any further bubbeling
    Util.prevent_event (event);
    // allows one click activation per frame
    if (Util.frame_stamp() == this.menudata.menu_stamp)
      return;
    // turn bubbled click into menu activation
    const target = event.target;
    if (target && valid_uri (target.uri) && target.check_isactive)
      {
	const isactive = target.check_isactive (false);
	if (isactive instanceof Promise)
	  return (async () => (await isactive) && this.click (target.uri)) ();
	if (isactive)
	  return this.click (target.uri);
      }
  }
  click (uri)
  {
    // prevent recursion
    if (this.allowed_click)
      return;
    // allows one click activation per frame
    if (Util.frame_stamp() == this.menudata.menu_stamp)
      return;
    // emit non-bubbling activation click
    if (valid_uri (uri)) {
      this.menudata.menu_stamp = Util.frame_stamp();
      const click_event = new CustomEvent ('click', { bubbles: false, composed: true, cancelable: true, detail: { uri } });
      this.allowed_click = click_event;
      const proceed = this.dispatchEvent (click_event);
      this.allowed_click = null;
      if (proceed) {
	if (this.activate)
	  this.activate (uri, click_event);
	else
	  this.dispatchEvent (new CustomEvent ('activate', {
	    detail: { uri }
	  }));
      }
      this.close();
    }
    else
      console.error ("BContextMenu.click: invalid uri:", uri);
  }
  close () {
    if (this.dialog?.open) {
      // this.dialog.classList.add ('animating');
      this.dialog.close();
    }
    this.origin = null;
    this.focus_uri = '';
    this.data_contextmenu?.removeAttribute ('data-contextmenu', 'true');
    this.data_contextmenu = null;
    App.zmove(); // force changes to be picked up
    if (this.emit_close_) {
      this.emit_close_--;
      this.dispatchEvent (new CustomEvent ('close', { detail: {} }));
    }
  }
  /// Activate or disable the `kbd=...` hotkeys in menu items.
  map_kbd_hotkeys (active = false) {
    if (this.keymap_.length) {
      this.keymap_.length = 0;
      Util.remove_keymap (this.keymap_);
    }
    if (!active)
      return;
    const w = document.createTreeWalker (this, NodeFilter.SHOW_ELEMENT);
    let e;
    while ( (e = w.nextNode()) ) {
      /**@type{any}*/ const any = e;
      const keymap_entry = any.keymap_entry;
      if (keymap_entry instanceof Util.KeymapEntry)
	this.keymap_.push (keymap_entry);
    }
    if (this.keymap_.length)
      Util.add_keymap (this.keymap_);
  }
  /// Find a menuitem via its `uri`.
  find_menuitem (uri)
  {
    const w = document.createTreeWalker (this, NodeFilter.SHOW_ELEMENT);
    let e;
    while ( (e = w.nextNode()) ) {
      /**@type{any}*/ const any = e;
      if (any.uri == uri)
	return e;
    }
    return null;
  }
}
customElements.define ('b-contextmenu', BContextMenu);
