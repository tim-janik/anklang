// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BContextMenu
 * @description
 * The <b-contextmenu> element implements a modal popup that displays contextmenu choices,
 * based on `<button uri=... ic=... kbd=.../>` elements, see also [BMenuRow](#BMenuRow),
 * [BMenuTitle](#BMenuTitle) and [BMenuSeparator](#BMenuSeparator).
 * Menu actions are identified via URI attributes, they can be activated by calling a handler
 * which is assigned via the `.activate` property, or the actions can be checked for being disabled
 * by calling a handler which is assigned via the `.isactivate` property.
 * The `ic` attribute on buttons embeds a `<b-icon ic=.../>` inside the buttons that are children of a <b-contextmenu>.
 * Using the `popup()` method, the menu can be popped up via
 * [HTMLDialogElement.showModal](https://developer.mozilla.org/en-US/docs/Web/API/HTMLDialogElement/showModal).
 * Example:
 * ```html
 * <div @contextmenu="e => querySelector('b-contextmenu').popup (e)">
 *   <b-contextmenu .activate="menuactivation">
 *     <button ic="mi-close" kbd="Shift+Ctrl+Q" uri="quit" > Quit </button>
 *   </b-contextmenu>
 * </div>
 * ```
 * Note that keyboard presses, mouse clicks, drag selections and event bubbling can
 * all cause menu item clicks and contextmenu activation.
 * In order to deduplicate multiple events that arise from the same user interaction,
 * *one* popup request and *one* click activation is processed per animation frame.
 *
 * ### Properties:
 * *.activate (uri)*
 * : Callback handler which is called with a menu item URI once a menu item is activated.
 * : Note, this handler can be called with an URI for which `.isactive` previously returned `false`, in particular via hotkeys.
 * *.isactive (uri)* -> Promise<bool>
 * : Async callback used to check for a particular menu item by URI to stay active or be disabled, called during popup().
 *
 * ### Attributes:
 * *xscale*
 * : Consider a wider area than the context menu width for popup positioning.
 * *yscale*
 * : Consider a taller area than the context menu height for popup positioning.
 *
 * ### Events:
 * *click (event)*
 * : Event signaling activation of a menu item, the `uri` can be found via `get_uri (event.target)`.
 * *close (event)*
 * : Event signaling closing of the menu, regardless of whether menu item activation occoured or not.
 *
 * ### Methods:
 * *popup (event, { origin, focus_uri, data-contextmenu })*
 * : Popup the contextmenu, propagation of `event` is halted and the event coordinates or target is
 * : used for positioning unless `origin` is given.
 * : The `origin` is a reference DOM element to use for drop-down positioning.
 * : The `focus_uri` is a `<button uri.../>` menu item URI to receive focus after popup.
 * : The `data-contextmenu` element (or `origin`) has the `data-contextmenu=true` attribute assigned during popup.
 * *close()*
 * : Hide the contextmenu.
 * *map_kbd_hotkeys (active)*
 * : Activate/deactivate a global hotkey map containing the `<button kbd=.../>` hotkeys specified in menu items.
 * : For hotkeys, no prior `.isactive` check is carried out.
 */

import { LitComponent, html, render, noChange, JsExtract, docs, ref } from '../little.js';
import * as Util from "../util.js";
import * as Kbd from '../kbd.js';
import { text_content, get_uri, valid_uri, has_uri } from '../dom.js';

// == STYLE ==
JsExtract.scss`
b-contextmenu {
  display: contents; /* avoids interfering when inside a flexbox with justify-content:space-between */
}
dialog.b-contextmenu {
  flex-direction: column;
  align-items: stretch;
  justify-content: flex-start;
  @apply m-0 p-2;
  color: $b-menu-foreground;
  background-color: $b-menu-background; border: 1px outset zmod($b-menu-background, Jz-=20%);
  box-shadow: $b-menu-box-shadow;
  overflow: hidden auto;
  &[open], &.animating { display: flex; }
}
dialog.b-contextmenu::backdrop {
  /* Menu backdrop must be transparent, for one a popup menu is different from a modal dialog,
   * and second, showing a modal dialog via menu item would result in bad flickernig. */
  background: transparent;
}
b-contextmenu push-button,
b-contextmenu button {
  @apply hflex flex-nowrap items-stretch px-4 py-1 text-left;
  background: transparent; color: $b-menu-foreground; border: 1px solid transparent;
  cursor: pointer; user-select: none; outline: none;
  kbd { flex-grow: 1; color: zmod($b-menu-foreground, Jz-=15); }
  > b-icon:first-child {
    margin: 0 0.75rem 0 0;
    width: 2rem; height: 1rem;
    align-self: center;
    color: $b-menu-fill;
  }
  kbd { font-family: inherit; text-align: right; margin-left: 2.5em; }
  kbd[data-can-remap] { font-style: italic; }
  &[turn] {
    flex-direction: column; align-items: center;
    > b-icon:first-child { margin: 0 0 $b-menu-spacing 0; }
  }
  &[disabled], &[disabled] * {
    pointer-events: none;
    color: $b-menu-disabled;
    b-icon { color: $b-menu-disabled-fill; }
    kbd { color: $b-menu-disabled-fill; }
  }
}
b-contextmenu b-menurow button {
  @apply px-1;
  min-width: 5rem; //* this aligns blocks of 2-digit numbers */
  > b-icon:first-child { @apply m-0 mb-1; }
}
b-contextmenu button:focus {
  background-color: $b-menu-focus-bg; color: $b-menu-focus-fg; outline: none;
  kbd { color: inherit; }
  border: 1px solid zmod($b-menu-focus-bg, Jz-=50%);
  b-icon { color: $b-menu-focus-fg; }
}
b-contextmenu :is(button.active, button:focus.active, button:focus:active, button:active) {
  background-color: $b-menu-active-bg; color: $b-menu-active-fg; outline: none;
  kbd { color: inherit; }
  b-icon { color: $b-menu-active-fg; }
  border: 1px solid zmod($b-menu-active-bg, Jz-=50%);
}`;

// == HTML ==
const HTML = (t, d) => html`
  <dialog class="b-contextmenu" ${ref (h => t.dialog = h)} part="dialog">
    <slot></slot>
  </dialog>
`;

// == SCRIPT ==
/** @this{any} */
function menuitem_isdisabled ()
{
  const uri = get_uri (this);
  if (valid_uri (uri)  && undefined !== this.menudata.checkeduris[uri])
    return !this.menudata.checkeduris[uri];
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
	   isactive: uri => true,
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
    this.data_contextmenu = null;
    this.checkuri = () => true;
    this.reposition = false;
    this.isdisabled = menuitem_isdisabled;
    this.keymap_ = [];
    this.keymap_active = false;
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
	this.dialog_.onkeydown = this.dialog_keydown.bind (this);
	this.dialog_.onanimationend = ev => {
	  dialog.classList.remove ('animating');
	};
	Util.dialog_backdrop_autoclose (this.dialog_, true);
      }
  }
  dialog_keydown (event)
  {
    if (event.keyCode === 27 && // Escape
	this.dialog.open && this.dialog.matches ('[open]:modal'))
      return true;  // bubble up to browser // Util.prevent_event (event);
    if (Util.keydown_move_focus (event))
      return false; // handled, no-default
  }
  start_observer()
  {
    if (this._observer || !document.body.contains (this)) return;
    this._observer = new MutationObserver (Util.debounce (() => {
      this.stop_observer();
      this.integrate_children();
      this.start_observer();
    }));
    this._observer.observe (this, { childList: true, subtree: true, attributes: true });
  }
  stop_observer()
  {
    if (!this._observer) return;
    this._observer.disconnect();
    this._observer = null;
  }
  connectedCallback()
  {
    this.integrate_children();
    super.connectedCallback();
    this.toggle_force_children (true);
    this.start_observer();
  }
  disconnectedCallback()
  {
    this.stop_observer();
    this.map_kbd_hotkeys (false);
    super.disconnectedCallback();
  }
  integrate_children()
  {
    for (let b of this.querySelectorAll ('button,push-button')) {
      /**@type{any}*/ const any = b;
      integrate_button.call (any, this);
    }
    // rebuild keymap
    this.map_kbd_hotkeys (this.keymap_active);
  }
  async toggle_active_children()
  {
    const isactive = async uri => !uri || !this.isactive || await this.isactive (uri);
    const proms = [];
    for (let b of this.querySelectorAll ('button,push-button')) {
      /**@type{any}*/ const any = b;
      const uri = any.getAttribute ('uri');
      if (uri === null) continue;
      const promise = isactive (uri);
      promise['element'] = any;
      proms.push (promise);
    }
    const toggles = await Promise.all (proms);
    this.stop_observer();
    for (let i = 0; i < proms.length; i++) {
      const element = proms[i]['element'], toggle = !!toggles[i];
      element.toggleAttribute ('disabled', !toggle);
    }
    this.start_observer();
    proms.length = 0;
  }
  toggle_force_children (enabled)
  {
    this.stop_observer();
    for (let b of this.querySelectorAll ('button,push-button')) {
      /**@type{any}*/ const any = b;
      const uri = any.getAttribute ('uri');
      if (uri === null) continue;
      any.toggleAttribute ('disabled', !enabled);
    }
    this.start_observer();
  }
  updated()
  {
    if (this.reposition)
      {
	this.reposition = false;
	const p = Util.popup_position (this.dialog, { origin: this.origin,
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
    if (this.dialog?.open || Util.frame_stamp() == this.menudata.menu_stamp)
      return false;     				// duplicate popup request, only popup once per frame
    if (!popup_options)
      popup_options = { origin: null };
    const origin = popup_options.origin === null ? null : popup_options.origin?.$el || popup_options.origin || event?.currentTarget;
    if (origin instanceof Element && Util.inside_display_none (origin))
      return false;     				// cannot popup around hidden origin
    this.toggle_force_children (false);			// add [disabled] attribute to chldren
    const toggles = this.toggle_active_children();	// concurrently, enable active children
    this.origin = origin instanceof Element ? origin : null;
    this.menudata.menu_stamp = Util.frame_stamp();  	// allows one popup per frame
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
    // auto-focus a specific child, or the first child with uri
    const furi = popup_options.focus_uri ? 'uri="' + popup_options.focus_uri + '"' : 'uri';
    return (async () => {
      await this.updateComplete; // needed to access this.dialog
      this.reposition = true;
      this.dialog.showModal();
      this.blur();
      App.zmove(); // force changes to be picked up
      // check items (and this used to handle auto-focus)
      await toggles;
      const qse = this.querySelector (`button[${furi}],push-button[${furi}]`);
      const focus_child = /**@type{HTMLElement}*/ (qse);
      if (focus_child && !focus_child.getAttribute ('disabled'))
	focus_child.focus();
    }) ();
  }
  async check_isactive (finduri = null)
  {
    const w = document.createTreeWalker (this, NodeFilter.SHOW_ELEMENT);
    let hasuri = null, e, a = [];
    while ( (e = w.nextNode()) ) {
      /**@type{any}*/ const any = e;
      if (any.check_isactive) {
	if (get_uri (any) === finduri)
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
    const uri = get_uri (target);
    if (target && valid_uri (uri))
      {
	const isactive = !target.check_isactive ? true : target.check_isactive (false);
	if (isactive instanceof Promise)
	  return (async () => (await isactive) && this.click (uri)) ();
	if (isactive)
	  return this.click (uri);
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
    this.toggle_force_children (true);
    this.origin = null;
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
    this.keymap_active = !!active;
    if (!this.keymap_active)
      return;
    const w = document.createTreeWalker (this, NodeFilter.SHOW_ELEMENT);
    let e;
    while ( (e = w.nextNode()) ) {
      /**@type{any}*/ const any = e;
      const keymap_entry = any['_keymap_entry'];
      if (keymap_entry instanceof Util.KeymapEntry)
	this.keymap_.push (keymap_entry);
    }
    if (this.keymap_.length)
      Util.add_keymap (this.keymap_);
  }
  /// Find a menuitem via its URI.
  find_menuitem (uri)
  {
    const w = document.createTreeWalker (this, NodeFilter.SHOW_ELEMENT);
    let e;
    while ( (e = w.nextNode()) ) {
      /**@type{any}*/ const any = e;
      if (get_uri (any) == uri)
	return e;
    }
    return null;
  }
}
customElements.define ('b-contextmenu', BContextMenu);

/// Render and return a cached reusable <b-contextmenu/> from `make_lithtml(target,data)`.
export function render_contextmenu (b_contextmenu, make_lithtml, target = undefined, data = undefined)
{
  let cm = !b_contextmenu ? null : b_contextmenu['.cm'];
  if (!cm) {
    cm = { target, contextmenu: null, proxy: null, div: document.createElement ('div') };
    cm.proxy = new Proxy (cm, {
      get (cm, prop, receiver) // bind methods to latest render target
      {
	const value = cm.target[prop];
	return value instanceof Function ? value.bind (cm.target) : value;
      },
    });
    cm.div.setAttribute ('style', "position:absolute;width:0;height:0;border:0;visibility:hidden;");
    cm.div.toggleAttribute ('inert', true);
  } else {
    b_contextmenu['.cm'] = null;
    delete b_contextmenu['.cm'];
    cm.contextmenu = null;
    cm.target = target;
  }
  // https://lit.dev/docs/libraries/standalone-templates/#render-options
  render (make_lithtml.apply (cm.proxy, [ cm.proxy, data ]), cm.div, { host: cm.proxy });
  cm.contextmenu = cm.div.querySelector ('b-contextmenu');
  if (cm.contextmenu) {
    cm.contextmenu['.cm'] = cm;
    if (!cm.div.parentElement)
      document.body.appendChild (cm.div);
    return cm.contextmenu;
  }
  throw new ReferenceError ("lit-html construct failed to create <b-contextmenu/>");
}

/** Integrate `b-contextmenu button` into contextmenu handling.
 * @this{HTMLElement}
 */
function integrate_button (contextmenu)
{
  // click & focus
  if (!this.onclick)
    this.onclick = contextmenu.onclick;
  if (!this.onmouseenter)
    /**@ts-ignore*/
    this.onmouseenter = this.focus.bind (this);
  // <b-icon ic/>
  const ic = this.getAttribute ('ic');
  if (ic) {
    const icon = this.querySelector ('b-icon') || document.createElement ('b-icon');
    if (!icon.parentElement) {
      icon.className = "pointer-events-none";
      this.prepend (icon);
    }
    icon.setAttribute ('ic', ic);
  } else
    this.querySelector ('b-icon')?.remove();
  // aria-label
  const aria_label = text_content (this, false).trim();
  this.setAttribute ('aria-label', aria_label);
  // menurow children
  const turn = !!Util.closest (this, 'b-menurow:not([noturn])');
  this.toggleAttribute ("turn", turn);
  const noturn = !!Util.closest (this, 'b-menurow[noturn]');
  this.toggleAttribute ("noturn", noturn);
  // <kbd/>
  const kbds = this.getAttribute ('kbd');
  if (kbds) {
    const kbd = this.querySelector ('kbd') || document.createElement ('kbd');
    if (!kbd.parentElement) {
      kbd.className = "pointer-events-none";
      this.appendChild (kbd);
      kbd.innerText = kbds;
    }
    // hotkey
    if (!this['_keymap_entry'])
      this['_keymap_entry'] = new Util.KeymapEntry ('', this.click.bind (this), this);
    const menudata = provide_menudata (this);
    const shortcut = Kbd.shortcut_lookup (menudata.mapname, aria_label, kbds);
    if (shortcut != this['_keymap_entry'].key)
      this['_keymap_entry'].key = shortcut;
    kbd.innerText = Util.display_keyname (shortcut);
  } else
    this.querySelector ('kbd')?.remove();
}
