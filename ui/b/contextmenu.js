// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** # B-CONTEXTMENU
 * A modal popup that displays contextmenu choices, see [B-MENUITEM](#b-menuitem),
 * [B-MENUSEPARATOR](#b-menuseparator).
 * Using the `popup()` method, the menu can be popped up from the parent component,
 * and setting up an `onclick` handler can be used to handle menuitem actions. Example:
 * ```html
 * <div @contextmenu.prevent="$refs.cmenu.popup">
 *   <b-contextmenu ref="cmenu" @click="menuactivation">...</b-contextmenu>
 * </div>
 * ```
 * ## Props:
 * *check*: function (uri: string): bool
 * : Callback property to check if a submenu item is enabled, the `uri` of the submenu is provided as argument.
 * *xscale*
 * : Consider a wider area than the context menu width for popup positioning.
 * *yscale*
 * : Consider a taller area than the context menu height for popup positioning.
 * ## Events:
 * *click (uri: string)*
 * : Event signaling activation of a submenu item, the `uri` of the submenu is provided as argument.
 * *close*
 * : Event signaling closing of a menu, regardless of whether menu item activation occoured or not.
 * *keepmounted*
 * : Keep the menu and menu items mounted at all times, needed for map_kbd_hotkeys().
 * ## Methods:
 * *popup (event, { origin, data-contextmenu })*
 * : Popup the contextmenu, the `event` coordinates are used for positioning, the `origin` is a
 * : reference DOM element to use for drop-down positioning. The `data-contextmenu` element (or `origin`)
 * : has the `data-contextmenu=true` attribute assigned during popup.
 * *close()*
 * : Hide the contextmenu.
 * *map_kbd_hotkeys (active)*
 * : Activate (deactivate) the `kbd=...` hotkeys specified in menu items.
 */

import { LitElement, html, render, noChange, css, postcss, docs, ref } from '../little.js';
import * as Kbd from '../kbd.js';

// == STYLE ==
const STYLE = await postcss`
@import 'mixins.scss';
dialog {
  &[open], &.animating { display: flex; }
  flex-direction: column;
  align-items: stretch;
  justify-content: flex-start;
  /* The template uses flex+start for layouting to scroll without vertical shrinking */
  [disabled], [disabled] * { pointer-events: none; }
  margin: 0;
  padding: $b-menu-padding 0;
  color: $b-menu-foreground;
  background-color: $b-menu-background; border: 1px outset zmod($b-menu-background, Jz-=20%);
  box-shadow: $b-menu-box-shadow;
  overflow-y: auto;
  overflow-x: hidden;
}
dialog::backdrop {
  background: $b-style-modal-overlay;
  background: #00000033;
}`;

// == HTML ==
const HTML = (t, d) => html`
  <dialog ${ref(e => t.dialog = e)} >
    <slot />
  </dialog>
`;

// == SCRIPT ==
export function valid_uri (uri) {
  return !(uri === '' || uri === undefined || uri === null); // allow uri=0
}

const menuitem_isdisabled = function () {
  if (this.uri && undefined !== this.menudata.checkeduris[this.uri])
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
};

const BOOL_ATTRIBUTE = { type: Boolean, reflect: true }; // sync attribute with property
const STRING_ATTRIBUTE = { type: String, reflect: true }; // sync attribute with property
const NUMBER_ATTRIBUTE = { type: Number, reflect: true }; // sync attribute with property

export function provide_menudata (element)
{
  // find b-contextmenu
  const b_contextmenu = Util.closest (element, 'b-contextmenu');
  if (b_contextmenu && b_contextmenu.menudata)
    return b_contextmenu.menudata;
  // fallback
  return { showicons: true, keepmounted: false,
	   mapname: '',
	   checkuri: () => true,
	   isdisabled: () => false,
	   close: () => undefined,
  };
}

class BContextMenu extends LitElement {
  static styles = [ STYLE ];
  render()
  {
    const d = {};
    return HTML (this, d);
  }
  static properties = {
    keepmounted: BOOL_ATTRIBUTE,	// FIXME: verify unmounted
    showicons: BOOL_ATTRIBUTE,
    mapname: STRING_ATTRIBUTE,
    check: { type: Function, state: true },
    xscale: NUMBER_ATTRIBUTE,
    yscale: NUMBER_ATTRIBUTE,
  };
  constructor()
  {
    super();
    this.body_div = null;
    this.showicons = true;
    this.page_x = undefined;
    this.page_y = undefined;
    this.xscale = 1;
    this.yscale = 1;
    this.popup_options = {};
    this.checkuri = () => true;
    this.isdisabled = menuitem_isdisabled;
    this.showmodal_update = false;
    this.keymap_ = [];
    this.dialog_ = null;
    // context for descendant menuitems
    this.menudata = provide_menudata (this);
    this.menudata.close = this.close.bind (this);
    // prevent clicks bubbeling up
    this.allowed_click = null;
    this.addEventListener ('click', e => this.handle_click_ (e));
  }
  get dialog ()     { return this.dialog_; }
  set dialog (dialog) {
    if (this.dialog_)
      {
	this.dialog_.onclose = null;
	this.dialog_.onanimationend = null;
      }
    this.dialog_ = dialog;
    if (this.dialog_)
      {
	this.dialog_.onclose = this.menudata.close;
	this.dialog_.onanimationend = ev => {
	  dialog.classList.remove ('animating');
	};
	dialog.onclick = ev => {
	  // backdrop click
	  if (ev.offsetX < 0 || ev.offsetX > ev.target.offsetWidth ||
	      ev.offsetY < 0 || ev.offsetY > ev.target.offsetHeight)
	    {
	      Util.prevent_event (ev); // prevent bubbling out of shadow DOM (if any)
	      dialog.close();
	    }
	};
      }
  }
  connectedCallback() {
    super.connectedCallback();
    this._timerInterval = setInterval(() => this.requestUpdate(), 1000);
  }
  disconnectedCallback() {
    super.disconnectedCallback();
    clearInterval(this._timerInterval);
    this.map_kbd_hotkeys (false);
  }
  updated () {
    if (this.showmodal_update)
      {
	// updated() call directly after showModal()
	this.showmodal_update = false;
	// TODO: checkitems
	const startfocus = true;
	if (startfocus) // auto-focus for menu bars
	  Util.move_focus (-999999);
      }
  }
  handle_click_ (event)
  {
    if (this.allowed_click === event)
      return;
    // prevent any bubbeling
    Util.prevent_event (event);
    // for valid menuitem clicks with URI
    if (event.target && event.target.uri)
      {
	// emit non-bubbling activation click
	this.click (event.target.uri);
      }
    // this.close();
  }
  click (uri)
  {
    if (this.allowed_click)
      return;
    if (uri && valid_uri (uri))
      {
	this.allowed_click = new CustomEvent ('click', { bubbles: false, composed: true, cancelable: true,
							 detail: { uri } });
	this.dispatchEvent (this.allowed_click);
	this.allowed_click = null;
	this.close();
      }
  }
  checkitems() {
    const checker = this.popup_options.checker || this.check;
    if (!checker)
      return;
    const checkrecursive = component => {
      component = Util.vue_component (component) || component;
      if (component.uri)
	{
	  let async_check = async () => {
	    let result = checker.call (null, component.uri, component);
	    result = await result;
	    if ('boolean' !== typeof result)
	      result = undefined;
	    if (result != this.checkeduris[component.uri])
	      {
		this.checkeduris[component.uri] = result; // Vue reactivity
		component.$forceUpdate();
	      }
	  };
	  async_check();
	}
      if (component.$children)
	for (const child of component.$children)
	  checkrecursive (child);
      else if (component.children)    // DOM element, possibly a function component
	for (const child of component.children)
	  checkrecursive (child);
    };
    checkrecursive (this);
  }
  popup (event, popup_options) {
    if (this.dialog?.open)
      return; // duplicate popup request
    const origin = popup_options.origin?.$el || popup_options.origin;
    if (origin && Util.inside_display_none (origin))
      return false;
    this.popup_options = Object.assign ({}, popup_options || {});
    if (event && event.pageX && event.pageY)
      {
	this.page_x = event.pageX;
	this.page_y = event.pageY;
      }
    else
      this.page_x = this.page_y = undefined;
    const dialog = this.dialog;
    dialog.showModal();
    const p = Util.popup_position (dialog, { origin,
					     x: this.page_x, xscale: this.xscale,
					     y: this.page_y, yscale: this.yscale, });
    dialog.style.left = p.x + "px";
    dialog.style.top = p.y + "px";
    this.showmodal_update = true;
    App.zmove(); // force changes to be picked up
  }
  popup_event (event, options = {})
  {
    if (!options.origin)
      options.origin = event.currentTarget;
    Util.prevent_event (event);
    return this.popup (event, options);
  }
  close () {
    if (this.dialog?.open)
      {
	// this.dialog.classList.add ('animating');
	this.dialog.close();
      }
    App.zmove(); // force changes to be picked up
  }
  /// Activate or disable the `kbd=...` hotkeys in menu items.
  map_kbd_hotkeys (active = false) {
    if (this.keymap_.length)
      {
	this.keymap_.length = 0;
	Util.remove_keymap (this.keymap_);
      }
    const w = document.createTreeWalker (this, NodeFilter.SHOW_ELEMENT);
    let e;
    while ( (e = w.nextNode()) )
      {
	const keymap_entry = e.keymap_entry;
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
    while ( (e = w.nextNode()) )
      if (e.uri == uri)
	return e;
    return null;
  }
}

customElements.define ('b-contextmenu', BContextMenu);
