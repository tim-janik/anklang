// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** # B-MENUITEM
 * A menuitem element to be used as a descendant of a [B-CONTEXTMENU](#b-contextmenu).
 * The menuitem can be activated via keyboard focus or mouse click and will notify
 * its B-CONTEXTMENU about the click and its `uri`, unless the `@click.prevent`
 * modifier is used.
 * If no `uri` is specified, the B-CONTEXTMENU will still be notified to be closed,
 * unless `$event.preventDefault()` is called.
 * ## Properties:
 * *.isactive*: bool (uri)
 * : Property callback used to check if a particular menu item should stay active or be disabled.
 * ## Attributes:
 * *uri*
 * : Unique identifier for this menu item.
 * *kbd*
 * : Hotkey to acivate the menu item, displayed next to the menu item label.
 * *disabled*
 * : Boolean flag indicating disabled state.
 * *ic*
 * : Shorthands icon properties that are forwarded to a [B-ICON](#b-icon) used inside the menuitem.
 * ## Events:
 * *click*
 * : Event emitted on keyboard or mouse activation, use `event.stopPropagation()` to prevent bubbling to the contextmenu.
 * ## Slots:
 * *default*
 * : All contents passed into this slot will be rendered as contents of this element.
 */

import { LitElement, html, css, postcss, docs } from '../little.js';
import * as Kbd from '../kbd.js';
import * as ContextMenu from './contextmenu.js';

// == STYLE ==
const STYLE = await postcss`
@import 'shadow.scss';
@import 'mixins.scss';
:host {
  display: flex; flex-direction: column; align-items: stretch;
}
button {
  display: flex; flex-direction: column; align-items: stretch;
  flex: 0 0 auto;
  flex-wrap: nowrap;
  flex-direction: row;
  margin: 0; padding: $b-menu-vpad $b-menu-hpad; text-align: left;
  // button-reset
  background: transparent; color: $b-menu-foreground; border: 1px solid transparent;
  cursor: pointer; user-select: none; outline: none;
  kbd { color: zmod($b-menu-foreground, Jz-=15); }
  b-icon { color: $b-menu-fill; height: 1em; width: 1em; }
  > b-icon:first-child { margin: 0 $b-menu-spacing 0 0; }
}
button:focus {
  background-color: $b-menu-focus-bg; color: $b-menu-focus-fg; outline: none;
  kbd { color: inherit; }
  border: 1px solid zmod($b-menu-focus-bg, Jz-=50%);
  b-icon { color: $b-menu-focus-fg; }
}
button.active, button:focus.active, button:focus:active,
button:active {
  background-color: $b-menu-active-bg; color: $b-menu-active-fg; outline: none;
  kbd { color: inherit; }
  b-icon { color: $b-menu-active-fg; }
  border: 1px solid zmod($b-menu-active-bg, Jz-=50%);
}
*[disabled] {
  color: $b-menu-disabled;
  b-icon { color: $b-menu-disabled-fill; }
}
.kbdspan {
  flex-grow: 1;
  text-align: right;
  padding-left: 2.5em;
  kbd {
    font-family: inherit;
    font-weight: 400;
  }
}
&.-nokbd .kbdspan { display:none; }
button[turn] {
  flex-direction: column; align-items: center;
  & > b-icon:first-child { margin: 0 0 $b-menu-spacing 0; }
}
button[noturn] {
  .menulabel { min-width: 2em; } //* this aligns blocks of 2-digit numbers */
  > b-icon:first-child { margin: 0 $b-menu-tightspace 0 0; }
}
.kbdspan[kbdhidden] { display: none; }
kbd[data-can-remap] { font-style: italic; }
`;

// == HTML ==
const HTML = (t, d) => html`
  <button ?disabled=${d.isdisabled} ?turn=${d.turn} ?noturn=${d.noturn}
    @keydown="${Util.keydown_move_focus}"
    @mouseenter="${t.focus}" @mouseup="${t.mouseup_}"
    @click="${t.click}" @contextmenu="${t.rightclick}" >
    <b-icon class=${t.iconclass} ic=${t.ic} ></b-icon>
    <span class="menulabel"><slot /></span>
    <span class="kbdspan" ?kbdhidden=${!d.hotkey} >
      <kbd ?data-can-remap=${t.can_remap}
	   title="${t.menudata.mapname && 'RIGHT-CLICK Assign Shortcut'}"
	>${d.keyname}</kbd></span>
  </button>
`;

// == SCRIPT ==
const BOOL_ATTRIBUTE = { type: Boolean, reflect: true }; // sync attribute with property
const STRING_ATTRIBUTE = { type: String, reflect: true }; // sync attribute with property
const PRIVATE_PROPERTY = { state: true };

class BMenuItem extends LitElement {
  static styles = [ STYLE ];
  static shadowRootOptions = { ...LitElement.shadowRootOptions, delegatesFocus: true };
  render()
  {
    const turn = !!Util.closest (this, 'b-menurow:not([noturn])');
    const noturn = !!Util.closest (this, 'b-menurow[noturn]');
    const isdisabled = this.isactive_cache == false || this.getAttribute ('disabled') != null;
    const d = {
      isdisabled,
      turn, noturn,
      hotkey: this.keymap_entry.key,
      keyname: Util.display_keyname (this.keymap_entry.key),
    };
    return HTML (this, d);
  }
  static properties = {
    uri: STRING_ATTRIBUTE,
    disabled: BOOL_ATTRIBUTE,
    iconclass: STRING_ATTRIBUTE,
    ic: STRING_ATTRIBUTE,
    kbd: STRING_ATTRIBUTE,
    isactive_cache: PRIVATE_PROPERTY,
  };
  constructor()
  {
    super();
    this.uri = '';
    this.disabled = false;
    this.isactive = null;
    this.isactive_cache = true;
    this.iconclass = '';
    this.ic = '';
    this.kbd = '';
    // BMenuItem.keymap_entry is picked up by contextmenu
    this.keymap_entry = new Util.KeymapEntry ('', this.click.bind (this), this);
    this.can_click_ = 0;
  }
  updated (changed_props)
  {
    const rendered_slot_label = this.slot_label();
    if (rendered_slot_label)
      {
	const shortcut = Kbd.shortcut_lookup (this.menudata.mapname, rendered_slot_label, this.kbd);
	// Note, the correct shortcut cannot be found until *after* render
	if (shortcut != this.keymap_entry.key)
	  {
	    this.keymap_entry.key = shortcut;
	    this.requestUpdate();               // Note, this triggers a re-render
	  }
      }
    this.check_isactive();
  }
  check_isactive (requery = true)
  {
    if (!requery)
      return this.isactive_cache;
    // query now, return result unless callbacks yield Promises
    let isactive = !this.isactive || this.isactive (this.uri);
    if (isactive instanceof Promise)
      return (async () => {
	isactive = (await isactive) && this.menudata.isactive (this.uri);
	isactive = !!await isactive;
	if (this.isactive_cache != isactive)
	  this.isactive_cache = isactive;
	return this.isactive_cache;
      }) ();
    if (isactive)
      {
	isactive = this.menudata.isactive (this.uri);
	if (isactive instanceof Promise)
	  return (async () => {
	    isactive = !!await isactive;
	    if (this.isactive_cache != isactive)
	      this.isactive_cache = isactive;
	    return this.isactive_cache;
	  }) ();
      }
    isactive = !!isactive;
    if (this.isactive_cache != isactive)
      this.isactive_cache = isactive;
    return this.isactive_cache;
  }
  get menudata()
  {
    return ContextMenu.provide_menudata (this);
  }
  slot_label()
  {
    const slot = this.shadowRoot.querySelector ('slot');
    const stext = Util.collect_text_content (slot?.assignedNodes());
    return Util.lrstrip (stext);
  }
  close (trigger)
  {
    Util.prevent_event (trigger); // prevent trigger event bubbling out of shadow DOM (if any)
    // dispatch close event
    const event = new Event ('close', { bubbles: true, composed: true, cancelable: true });
    this.dispatchEvent (event);
  }
  click (trigger)
  {
    // prevent trigger event bubbling out of shadow DOM (if any)
    Util.prevent_event (trigger);
    // ignore duplicate click()-triggers, permit once per frame
    if (Util.frame_stamp() == this.menudata.item_stamp)
      return;
    this.menudata.item_stamp = Util.frame_stamp();
    // check if click is allowed
    const maybe_isactive = this.check_isactive();
    if (maybe_isactive instanceof Promise)
      {
	const async_click = async (isactive) => {
	  isactive = await isactive;
	  return !!isactive && super.click();
	};
	return async_click (maybe_isactive);
      }
    // super does: dispatchEvent (new Event ('click', { bubbles: true, composed: true, cancelable: true }));
    return !!maybe_isactive && super.click();
  }
  get can_remap()
  {
    const slottext = this.slot_label();
    return this.menudata.mapname && !!slottext;
  }
  async rightclick (event)
  {
    event.preventDefault();
    event.stopPropagation();
    if (this.can_remap)
      {
	if (await Kbd.shortcut_dialog (this.menudata.mapname, this.slot_label(), this.kbd))
	  this.requestUpdate();
      }
  }
  mouseup_ (event)
  {
    if (Util.has_ancestor (event.target, this))
      {
	const now = new Date();
	// threshold to avoid unintended click-drag activation
	if (this.menudata.menu_stamp + 500 < now)
	  {
	    // simulate click for press-popup-drag-release menu selection
	    this.click (event);
	  }
      }
  }
}

customElements.define ('b-menuitem', BMenuItem);
