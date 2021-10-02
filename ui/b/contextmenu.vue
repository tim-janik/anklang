<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-CONTEXTMENU
  A modal popup that displays contextmenu choices, see [B-MENUITEM](#b-menuitem),
  [B-MENUSEPARATOR](#b-menuseparator).
  Using the `popup()` method, the menu can be popped up from the parent component,
  and setting up an `onclick` handler can be used to handle menuitem actions. Example:
  ```html
  <div @contextmenu.prevent="$refs.cmenu.popup">
    <b-contextmenu ref="cmenu" @click="menuactivation">...</b-contextmenu>
  </div>
  ```
  ## Props:
  *xscale*
  : Consider a wider area than the context menu width for popup positioning.
  *yscale*
  : Consider a taller area than the context menu height for popup positioning.
  ## Events:
  *click (uri)*
  : Event signaling activation of a submenu item, the `uri` of the submenu is provided as argument.
  *close*
  : Event signaling closing of a menu, regardless of whether menu item activation occoured or not.
  *startfocus*
  : Auto focus a menu item upon popup, in the same way menu bar menus work.
  *keepmounted*
  : Keep the menu and menu items mounted at all times, needed for map_kbd_hotkeys().
  ## Methods:
  *popup (event, { origin, data-contextmenu })*
  : Popup the contextmenu, the `event` coordinates are used for positioning, the `origin` is a
  : reference DOM element to use for drop-down positioning. The `data-contextmenu` element (or `origin`)
  : has the `data-contextmenu=true` attribute assigned during popup.
  *close()*
  : Hide the contextmenu.
  *map_kbd_hotkeys (active)*
  : Activate (deactivate) the `kbd=...` hotkeys specified in menu items.
</docs>

<style lang="scss">
  @import 'mixins.scss';
  body div.b-contextmenu-modalshield div.b-contextmenu-area { //* constraint area for context menu placement */
    position: fixed; right: 0; bottom: 0;		    //* fixed bottom right */
    display: flex; flex-direction: column; align-items: flex-start;
    pointer-events: none;
    .b-contextmenu {	//* since menus are often embedded, this needs high specificity */
      //* The template uses flex+start for layouting to scroll without vertical shrinking */
      position: relative; max-width: 100%; max-height: 100%;  //* constrain to .b-contextmenu-area */
      overflow-y: auto; overflow-x: hidden;		    //* scroll if necessary */
      padding: $b-menu-padding 0;
      background-color: $b-menu-background; border: 1px outset darken($b-menu-background, 20%);
      color: $b-menu-foreground;
      box-shadow: $b-menu-box-shadow;
      pointer-events: initial;
      [disabled], [disabled] * { pointer-events: none; }
    }
  }
.b-contextmenu-placeholder { width: 0; height: 0; pointer-events: none; visibility: hidden; }
.b-contextmenu-modalshield {
  position: fixed; top: 0; left: 0; bottom: 0; right: 0;
  width: 100%; height: 100%;
  display: flex;
  background: $b-style-modal-overlay;
  background: #00000033;
  pointer-events: all;
}

//* transition */
$duration: 0.097s;
.b-contextmenu-modalshield, .b-contextmenu {
  transition: opacity $duration linear, transform $duration ease;
  transform-origin: bottom;
}
.b-contextmenu-modalshield {
  &.-slide-enter-from, &.-slide-leave-to {
    & { opacity: 0; }
    .b-contextmenu {
      //* scaling messes up positioning: transform: translateY(-100% +100% * 0.17) scaleY(0.17); */
    }
  }
}
</style>

<template>
  <div class="b-contextmenu-placeholder" >
    <transition name="-slide">
      <div class="b-contextmenu-modalshield" ref="shield" v-show='visible' v-if='visible || keepmounted' >
	<div class='b-contextmenu-area' :class='cmenu_class' ref='contextmenuarea' >
	  <v-flex class='b-contextmenu' :class="popupclass" ref='cmenu' start
		  :style="cmenu_style()">
	    <slot />
	  </v-flex>
	</div>
      </div>
    </transition>
  </div>
</template>

<script>
import * as Util from '../util.js';

const menuitem_onclick = function (event) {
  this.$emit ('click', this.uri, event);
  if (!event.defaultPrevented)
    {
      if (this.uri !== undefined && this.menudata.clicked)
	this.menudata.clicked (this.uri);
      else if (this.menudata.close)
	this.menudata.close();
    }
  event.stopPropagation();
  event.preventDefault(); // avoid submit, etc
};

const menuitem_isdisabled = function () {
  if (this.uri && undefined !== this.menudata.checkeduris[this.uri])
    return !this.menudata.checkeduris[this.uri];
  if (Object.hasOwnProperty.call (this, 'disabled') &&
      (this.disabled === "" || !!this.disabled))
    return true;
  if (this.$attrs && (this.$attrs['disabled'] === "" || !!this.$attrs['disabled']))
    return true;
  return false;
};

export default {
  sfc_template,
  emits: { click: uri => !!uri,
	   close: null, },
  props: { notransitions: { default: false },
	   keepmounted: { type: Boolean, },
	   startfocus: { type: Boolean, },
	   xscale: { default: 1, },
	   yscale: { default: 1, }, },
  computed: {
    cmenu_class() { return this.notransitions !== false ? 'b-contextmenu-notransitions' : ''; },
    popupclass() {
      const pclasses = Array.from (this.$el?.classList || []);
      const cclasses = pclasses.filter (e => e !== 'b-contextmenu-placeholder');
      return cclasses;
    },
  },
  data() { return {
    visible: false, doc_x: undefined, doc_y: undefined,
    resize_observer: undefined, checkeduris: {},
    showicons: true, popup_options: {},
    onclick: menuitem_onclick, isdisabled: menuitem_isdisabled,
  }; },
  provide: Util.fwdprovide ('b-contextmenu.menudata',	// context for menuitem descendants
			    [ 'checkeduris', 'showicons', 'keepmounted', 'clicked', 'close', 'onclick', 'isdisabled' ]),
  methods: {
    dom_update () {
      const shield = this.$refs.shield; // capture shield for callbacks
      if (shield?.parentNode == this.$el)
	{
	  this.remove_reparentation?.();
	  document.body.querySelector ('#b-app-shell-modalmenus-layer').insertBefore (shield, null);
	  this.remove_reparentation = () => shield.parentNode?.removeChild?. (shield);
	}
      this.resize_observer?.disconnect?.();
      if (this.visible && !this.resize_observer)
	this.resize_observer = new Util.ResizeObserver (() => this.position_popup());
      if (!this.$refs.cmenu) {
	this.undo_shield_setup?.();
	this.undo_shield_setup = null;
      }
      if (this.$refs.cmenu) // hotkeys are active if !visible
	this.checkitems();
      if (this.$refs.cmenu && this.visible) {
	if (!this.undo_shield_setup)
	  this.undo_shield_setup = Util.setup_shield_element (this.$refs.shield, this.$refs.contextmenuarea, this.close.bind (this));
	this.position_popup();
	this.resize_observer.observe (document.body);
	if (this.popup_options?.origin)
	  this.resize_observer.observe (this.popup_options.origin.$el || this.popup_options.origin);
      }
      if (!this.$refs.cmenu || !this.visible)
	this.clear_data_contextmenu();
      if (this.visible && !this.was_visible && this.startfocus)
	Util.move_focus (-999999);
      else if (!this.visible && this.was_visible)
	Util.forget_focus (this.$el);
      this.was_visible = this.visible;
    },
    dom_destroy () {
      this.clear_dragging();
      this.undo_shield_setup?.();
      this.undo_shield_setup = null;
      this.remove_reparentation?.();
      this.remove_reparentation = null;
      this.clear_data_contextmenu();
      this.resize_observer?.disconnect?.();
      this.resize_observer = null;
      this.map_kbd_hotkeys (false);
    },
    clear_data_contextmenu ()
    {
      if (this._data_contextmenu_element)
	{
	  this._data_contextmenu_element.removeAttribute ('data-contextmenu');
	  this._data_contextmenu_element = undefined;
	}
    },
    add_data_contextmenu (element)
    {
      this.clear_data_contextmenu();
      this._data_contextmenu_element = element;
      this._data_contextmenu_element.setAttribute ('data-contextmenu', 'true');
    },
    position_popup()
    {
      if (this.position_popup_queued)
	return;
      this.position_popup_queued = 1;
      Vue.nextTick (() => {
	this.position_popup_queued = undefined;
	this.position_popup_at_vuetick();
      });
    },
    position_popup_at_vuetick() {
      const area_el = this.$refs.contextmenuarea;
      if (area_el && area_el.getBoundingClientRect) // ignore Vue placeholder (html comment)
	{
	  const origin = this.popup_options.origin?.$el || this.popup_options.origin;
	  if (origin && Util.inside_display_none (origin))
	    {
	      this.close();
	      return;
	    }
	  const menu_el = this.$refs.cmenu;
	  // unset size constraints before calculating desired size, otherwise resizing
	  // can take dozens of resize_observer/popup_position frame iterations
	  area_el.style.left = '0px';
	  area_el.style.top = '0px';
	  const p = Util.popup_position (menu_el, { x: this.doc_x, xscale: this.xscale,
						    y: this.doc_y, yscale: this.yscale,
						    origin });
	  area_el.style.left = p.x + "px";
	  area_el.style.top = p.y + "px";
	}
    },
    checkitems() {
      if (!this.popup_options.checker)
	return;
      const checkrecursive = component => {
	component = Util.vue_component (component) || component;
	if (component.uri)
	  {
	    let async_check = async () => {
	      let result = this.popup_options.checker.call (null, component.uri, component);
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
      if (this.$refs.cmenu)
	checkrecursive (this.$refs.cmenu);
    },
    popup (event, options) {
      this.popup_options = Object.assign ({}, options || {});
      this.visible = false;
      this.clear_data_contextmenu();
      this.clear_dragging();
      if (event && event.pageX && event.pageY)
	{
	  this.doc_x = event.pageX;
	  this.doc_y = event.pageY;
	}
      else
	this.doc_x = this.doc_y = undefined;
      if (event.type == "mousedown")
	{
	  console.assert (!this.dragging);
	  this.dragging = {
	    button: event.button,
	    ignoreclick: true,
	    timer: setTimeout (() => { if (this.dragging) this.dragging.ignoreclick = false; }, 500),
	    handler: (e) => this.drag_event (e),
	    evpassive: { capture: true, passive: true },
	    evactive: { capture: false, passive: false }
	  };
	  window.addEventListener ('mouseup',   this.dragging.handler, this.dragging.evactive);
	  window.addEventListener ('mousedown', this.dragging.handler, this.dragging.evpassive);
	  window.addEventListener ('keydown',   this.dragging.handler, this.dragging.evpassive);
	}
      const data_contextmenu_element = this.popup_options['data-contextmenu'] || this.popup_options.origin;
      if (data_contextmenu_element)
	this.add_data_contextmenu (data_contextmenu_element);
      this.visible = true;
      App.zmove(); // force changes to be picked up
    },
    clear_dragging() {
      if (!this.dragging)
	return;
      if (this.dragging.ignoreclick)
	clearTimeout (this.dragging.timer);
      window.removeEventListener ('mouseup',   this.dragging.handler, this.dragging.evactive);
      window.removeEventListener ('mousedown', this.dragging.handler, this.dragging.evpassive);
      window.removeEventListener ('keydown',   this.dragging.handler, this.dragging.evpassive);
      this.dragging = undefined;
    },
    drag_event (event) {
      console.assert (this.dragging);
      let clickit = null;
      if (this.visible && !this.dragging.ignoreclick &&
	  event.type == 'mouseup' && event.button == this.dragging.button &&
	  this.$refs.cmenu.contains (document.activeElement) &&
	  document.activeElement.contains (event.target))
	{
	  clickit = document.activeElement;
	  event.stopPropagation(); // avoid other 'mouseup' handlers
	  event.preventDefault();  // avoid generating 'click' from 'mouseup'
	}
      // clean up `dragging` state
      this.clear_dragging();
      // activate due to valid drag-selection
      if (clickit)
	Util.keyboard_click (clickit);
    },
    cmenu_style() {
      // avoid spurious enter/leave etc events during transitions that could steal focus, etc
      return this.visible ? "" : "pointer-events: none";
    },
    close () {
      this.clear_dragging();
      if (!this.visible)
	return;
      this.visible = false;
      // take down shield immediately, to remove focus guards
      this.undo_shield_setup?.();
      this.undo_shield_setup = undefined;
      this.$emit ('close');
      App.zmove(); // force changes to be picked up
      if (this.$refs.cmenu) {
	/* here, v-if=visible evaluates to false, so Vue3 ignores any further
	 * update requests (including $forceUpdate). so we have to force a
	 * cmenu.style update to prevent spurious events past menu activation.
	 */
	this.$refs.cmenu.style = this.cmenu_style();
      }
    },
    clicked (uri) {
      this.$emit ('click', uri);
      this.close();
    },
    /// Activate or disable the `kbd=...` hotkeys in menu items.
    map_kbd_hotkeys (active = false) {
      if (Object.hasOwnProperty.call (this, 'active_kmap_'))
	{
	  for (const k in this.active_kmap_)
	    Util.remove_hotkey (k, this.active_kmap_[k]);
	  this.active_kmap_ = undefined;
	}
      if (!active || !this.$el)
	return;
      const kmap = {}; // key -> component
      const buildmap = v => {
	const key = Object.hasOwnProperty.call (v, 'kbd_hotkey') && v.kbd_hotkey();
	if (key)
	  kmap[key] = _ => v.$el && Util.keyboard_click (v.$el);
	for (const c of v.$children)
	  buildmap (c);
      };
      buildmap (this);
      this.active_kmap_ = kmap;
      for (const k in this.active_kmap_)
	Util.add_hotkey (k, this.active_kmap_[k], this.$el);
    },
    /// Find a menuitem via its `uri`.
    find_menuitem (uri) {
      const match_uri = e => {
	if (Object.hasOwnProperty.call (e, 'uri') && e.uri == uri)
	  return e;
	for (const c of e.$children)
	  {
	    const r = match_uri (c);
	    if (r)
	      return r;
	  }
	return undefined;
      };
      return match_uri (this);
    },
  },
};
</script>
