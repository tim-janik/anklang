// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @class ContextMenu
 * @description
 * The ContextMenu component implements a modal popup that displays contextmenu choices,
 * based on `<button uri=... ic=... kbd=.../>` elements, see also [MenuRow](#MenuRow),
 * [MenuTitle](#MenuTitle) and [MenuSeparator](#MenuSeparator).
 * Menu actions are identified via URI attributes, they can be activated by calling a handler
 * which is assigned via the `.activate` property, or the actions can be checked for being disabled
 * by calling a handler which is assigned via the `.isactive` property.
 * The `ic` attribute on buttons embeds a `.b-icon` span inside the buttons.
 * Using the `popup()` method, the menu can be shown via
 * [HTMLDialogElement.showModal](https://developer.mozilla.org/en-US/docs/Web/API/HTMLDialogElement/showModal).
 * Example:
 * ```tsx
 * <div onContextMenu={e => cm_ref.popup(e)}>
 *   <ContextMenu ref={cm_ref} activate={menuactivation}>
 *     <button ic="md-close" kbd="Shift+Ctrl+Q" uri="quit"> Quit </button>
 *   </ContextMenu>
 * </div>
 * ```
 * Note that keyboard presses, mouse clicks, drag selections and event bubbling can
 * all cause menu item clicks and contextmenu activation.
 * In order to deduplicate multiple events that arise from the same user interaction,
 * *one* popup request and *one* click activation is processed per animation frame.
 *
 * ### Props:
 * *activate (uri)*
 * : Callback handler which is called with a menu item URI once a menu item is activated.
 * : Note, this handler can be called with an URI for which `.isactive` previously returned `false`, in particular via hotkeys.
 * *isactive (uri)* -> Promise<bool>
 * : Async callback used to check for a particular menu item by URI to stay active or be disabled, called during popup().
 *
 * ### Attributes:
 * *xscale*
 * : Consider a wider area than the context menu width for popup positioning.
 * *yscale*
 * : Consider a taller area than the context menu height for popup positioning.
 *
 * ### Events:
 * *activate (event)*
 * : Event signaling activation of a menu item, the `uri` can be found via `get_uri (event.detail)`.
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

import { onMount, onCleanup } from 'solid-js';
import * as Util from "../util.js";
import * as Kbd from '../kbd.js';
import { text_content, get_uri, valid_uri } from '../dom.js';
import * as Dom from "../dom.js";
import { icon_element } from './icon';

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
dialog.b-contextmenu {
  color: var(--b-menu-foreground);
  background-color: var(--b-menu-background);
  border: 1px outset oklch(from var(--b-menu-background) calc(l * 0.8) c h);
  box-shadow: var(--b-menu-box-shadow);
  padding: 0;
  /* showModal() sets position:fixed which breaks intrinsic sizing.
     Height is set explicitly in popup() based on content scrollHeight. */
  overflow-y: auto !important;
  overflow-x: hidden;
  &:not([open]) { display: none; }
}
dialog.b-contextmenu > .b-contextmenu-inner {
  @apply flex flex-col items-stretch justify-start p-2;
}
dialog.b-contextmenu::backdrop {
  /* Menu backdrop must be transparent, for one a popup menu is different from a modal dialog,
   * and second, showing a modal dialog via menu item would result in bad flickernig. */
  background: transparent;
}
.b-contextmenu :is(button, .asbutton, summary) {
  @apply hflex flex-nowrap items-stretch px-4 py-1 text-left;
  background: transparent; color: var(--b-menu-foreground); border: 1px solid transparent;
  cursor: pointer; user-select: none; outline: none; width: 100%;
  kbd { flex-grow: 1; color: oklch(from var(--b-menu-foreground) calc(l - 0.15) c h); }
  > .b-icon:first-child {
    margin: 0 0.75rem 0 0;
    width: 2rem; height: 1rem;
    align-self: center;
  }
  kbd { font-family: inherit; text-align: right; margin-left: 2.5em; }
  kbd[data-can-remap] { font-style: italic; }
  &[turn] {
    flex-direction: column; align-items: center;
    > .b-icon:first-child { margin: 0 0 var(--b-menu-spacing) 0; }
  }
  &[disabled], &[disabled] * {
    pointer-events: none;
    color: var(--b-menu-disabled);
    kbd { color: var(--b-menu-disabled-fill); }
  }
}
.b-contextmenu b-menurow button,
.b-contextmenu .b-menurow button {
  @apply px-1;
  min-width: 5rem; /* this aligns blocks of 2-digit numbers */
  > .b-icon:first-child { @apply m-0 mb-1; }
}
.b-contextmenu button:focus {
  background-color: var(--b-menu-focus-bg); color: var(--b-menu-focus-fg); outline: none;
  kbd { color: inherit; }
  border: 1px solid oklch(from var(--b-menu-focus-bg) calc(l * 0.5) c h);
}
.b-contextmenu :is(button.active, button:focus.active, button:focus:active, button:active) {
  background-color: var(--b-menu-active-bg); color: var(--b-menu-active-fg); outline: none;
  kbd { color: inherit; }
  border: 1px solid oklch(from var(--b-menu-active-bg) calc(l * 0.5) c h);
}`;

// == SCRIPT ==
function rects_no_overlap (rects: DOMRect[]): boolean
{
  for (let i = 0; i < rects.length; i++)
    for (let j = i + 1; j < rects.length; j++) {
      const a = rects[i], b = rects[j];
      if (a.right > b.left && a.left < b.right && a.bottom > b.top && a.top < b.bottom)
        return false;
    }
  return true;
}

const CONTEXTMENU_VIEWPORT_MARGIN = 40;

function assert_geometry (dialog: HTMLDialogElement, expect_centered: boolean)
{
  if (!__DEV__) return;
  const CENTER_THRESHOLD = 2;
  const vw = document.documentElement.clientWidth, vh = document.documentElement.clientHeight;
  const b = dialog.getBoundingClientRect();
  const max_h = window.innerHeight - CONTEXTMENU_VIEWPORT_MARGIN;
  const height_capped = dialog.offsetHeight >= max_h;
  // no unnecessary scrollbars
  if (b.width < vw && dialog.scrollWidth > dialog.clientWidth)
    console.error ("ContextMenu assert_geometry: horizontal scrollbar despite fitting viewport");
  if (b.height < vh && dialog.scrollHeight > dialog.clientHeight && !height_capped)
    console.error ("ContextMenu assert_geometry: vertical scrollbar despite fitting viewport");
  // same scrollbar check for visible children
  for (const child of dialog.querySelectorAll (':scope > .b-contextmenu-inner > *')) {
    if (!Util.check_visibility (child)) continue;
    if (b.width < vw && (child as HTMLElement).scrollWidth > (child as HTMLElement).clientWidth)
      console.error ("ContextMenu assert_geometry: child horizontal scrollbar despite fitting viewport");
    if (b.height < vh && (child as HTMLElement).scrollHeight > (child as HTMLElement).clientHeight && !height_capped)
      console.error ("ContextMenu assert_geometry: child vertical scrollbar despite fitting viewport");
  }
  // Only centered popups should avoid viewport edges; dropdowns and pointer menus are positioned there on purpose.
  if (expect_centered && b.width + CENTER_THRESHOLD < vw) {
    if (b.left <= 0) console.error ("ContextMenu assert_geometry: left-aligned despite fitting");
    if (b.right >= vw) console.error ("ContextMenu assert_geometry: right-aligned despite fitting");
  }
  if (expect_centered && b.height + CENTER_THRESHOLD < vh) {
    if (b.top <= 0) console.error ("ContextMenu assert_geometry: top-aligned despite fitting");
    if (b.bottom >= vh) console.error ("ContextMenu assert_geometry: bottom-aligned despite fitting");
  }
  // no-clipping check
  if (b.width <= vw && b.height <= vh) {
    if (b.left < 0 || b.top < 0 || b.right > vw || b.bottom > vh)
      console.error ("ContextMenu assert_geometry: dialog clipped despite fitting viewport");
  }
  // dialog width >= widest visible child
  let max_child_w = 0;
  for (const child of dialog.querySelectorAll (':scope > .b-contextmenu-inner > *')) {
    if (!Util.check_visibility (child)) continue;
    max_child_w = Math.max (max_child_w, (child as HTMLElement).offsetWidth);
  }
  if (dialog.offsetWidth < max_child_w)
    console.error ("ContextMenu assert_geometry: dialog narrower than widest child");
  // button rectangles don't overlap
  const btn_rects: DOMRect[] = [];
  for (const btn of dialog.querySelectorAll ('button, .asbutton')) {
    if (!Util.check_visibility (btn)) continue;
    btn_rects.push (btn.getBoundingClientRect());
  }
  if (!rects_no_overlap (btn_rects))
    console.error ("ContextMenu assert_geometry: button rectangles overlap");
}

export function provide_menudata (element)
{
  // find ContextMenu dialog
  const b_contextmenu = Util.closest (element, '.b-contextmenu');
  if (b_contextmenu && b_contextmenu.menudata)
    return b_contextmenu.menudata;
  // fallback
  return {
    close: () => undefined,
    isactive: uri => true,
    menu_stamp: 0,	// deduplicating frame_stamp() for contextmenu
    item_stamp: 0,	// deduplicating frame_stamp() for menuitem
    mapname: '',
    showicons: true,
  };
}

export function ContextMenu (props: {
  ref?: (el: HTMLDialogElement) => void;
  activate?: (uri: string, event?: Event) => void;
  isactive?: (uri: string) => (Promise<boolean> | boolean);
  showicons?: boolean;
  mapname?: string;
  class?: string;
  id?: string;
  xscale?: number;
  yscale?: number;
  onactivate?: (e: CustomEvent) => void;
  onclose?: (e: Event) => void;
  children?: any;
})
{
  let dialog_ref: HTMLDialogElement | undefined;
  let emit_close_ = 0;
  let page_x: number | undefined;
  let page_y: number | undefined;
  let origin_el: Element | null = null;
  let data_contextmenu: Element | null = null;
  let keymap_: Util.KeymapEntry[] = [];
  let keymap_active = false;
  let observer_: MutationObserver | null = null;
  // TODO: get rid of allowed_click legacy
  let allowed_click: Event | null = null;

  // === Methods ===

  // Keep a reference to the native close method before we overwrite it on the element
  const native_dialog_close = HTMLDialogElement.prototype.close;

  const close = () => {
    if (dialog_ref?.open) {
      native_dialog_close.call(dialog_ref);
    }
    toggle_force_children (true);
    origin_el = null;
    data_contextmenu?.removeAttribute ('data-contextmenu');
    data_contextmenu = null;
    (window as any).App?.zmove(); // force changes to be picked up
    if (emit_close_) {
      emit_close_--;
      const ev = new CustomEvent ('close', { detail: {} });
      props.onclose?.(ev);
      dialog_ref?.dispatchEvent (ev);
    }
  };

  // Context for descendant menu items; close is ready before the dialog ref is exposed.
  const menudata: any = {
    close,
    isactive: (uri: string) => valid_uri (uri) && (!props.isactive || props.isactive (uri)),
    menu_stamp: 0,
    item_stamp: 0,
    mapname: props.mapname || '',
    showicons: props.showicons !== false,
  };

  const popup = (event?: Event, popup_options: any = {}) => {
    Util.prevent_event (event);
    if (dialog_ref?.open || Util.frame_stamp() == menudata.menu_stamp)
      return false; // duplicate popup request, only popup once per frame
    const origin = popup_options.origin === null ? null : (popup_options.origin || (event as any)?.currentTarget);
    if (origin instanceof Element && !Util.check_visibility (origin))
      return false; // cannot popup around hidden origin
    toggle_force_children (false); // add [disabled] attribute to children
    const toggles = toggle_active_children(); // concurrently, enable active children
    origin_el = origin instanceof Element ? origin : null;
    menudata.menu_stamp = Util.frame_stamp(); // allows one popup per frame
    if (event && (event as any).pageX && (event as any).pageY) {
      page_x = (event as any).pageX;
      page_y = (event as any).pageY;
    } else {
      page_x = page_y = undefined;
    }
    data_contextmenu = popup_options['data-contextmenu'] || origin_el;
    data_contextmenu?.setAttribute ('data-contextmenu', 'true');
    emit_close_++;
    // Auto-focus a requested child, or the first visible focusable item.
    const focus_uri = popup_options.focus_uri;
    (async () => {
      if (!dialog_ref) return;
      Dom.show_modal (dialog_ref);
      // showModal() sets position:fixed which breaks intrinsic sizing on <dialog>.
      // Explicitly size the dialog to its content before repositioning.
      fit_and_reposition_dialog();
      dialog_ref.blur();
      (window as any).App?.zmove(); // force changes to be picked up
      // Check items before restoring focus; Chrome auto-focuses showModal(), make all browsers consistent.
      await toggles;
      fit_and_reposition_dialog();
      assert_geometry (dialog_ref!, !origin_el && !Util.valid_popup_coordinates (page_x, page_y));
      if (focus_uri) {
        const focus_item = find_menuitem (focus_uri);
        if (focus_item instanceof HTMLElement &&
            Util.check_visibility (focus_item) &&
            focus_item.matches ('button, .asbutton') &&
            !focus_item.hasAttribute ('disabled'))
          focus_item.focus();
        else
          Util.move_focus ('START');
      } else
        Util.move_focus ('START');
    })();
    return true;
  };

  const fit_and_reposition_dialog = () => {
    if (!dialog_ref) return;
    const inner = dialog_ref.querySelector ('.b-contextmenu-inner') as HTMLElement | null;
    if (inner) {
      const max_h = window.innerHeight - CONTEXTMENU_VIEWPORT_MARGIN;
      dialog_ref.style.maxHeight = max_h + 'px';
      // With box-sizing:border-box (Tailwind preflight), CSS height includes
      // border+padding, so the content area is reduced. Compensate accordingly.
      const cs = getComputedStyle (dialog_ref);
      const extra = cs.boxSizing === 'border-box' ?
        (parseFloat (cs.borderTopWidth) || 0) +
        (parseFloat (cs.borderBottomWidth) || 0) +
        (parseFloat (cs.paddingTop) || 0) +
        (parseFloat (cs.paddingBottom) || 0) : 0;
      const h = Math.min (inner.scrollHeight + extra, max_h);
      dialog_ref.style.height = h + 'px';
    }
    reposition_dialog();
  };

  const reposition_dialog = () => {
    if (!dialog_ref) return;
    const p = Util.popup_position (dialog_ref, {
      origin: origin_el, x: page_x, y: page_y,
      xscale: props.xscale ?? 1, yscale: props.yscale ?? 1,
    });
    dialog_ref.style.left = p.x + "px";
    dialog_ref.style.top = p.y + "px";
    dialog_ref.style.margin = "0";
  };

  const check_isactive = async (finduri: string | null = null) => {
    if (!dialog_ref) return null;
    const w = document.createTreeWalker (dialog_ref, NodeFilter.SHOW_ELEMENT);
    let hasuri: any = null, e: Node | null, a: Promise<any>[] = [];
    while ( (e = w.nextNode()) ) {
      const any_e: any = e;
      if (any_e.check_isactive) {
        if (get_uri (any_e) == finduri)
          hasuri = any_e;
        a.push (any_e.check_isactive());
      }
    }
    await Promise.all (a);
    return hasuri;
  };

  /// Find a menuitem via its URI.
  const find_menuitem = (uri: string) => {
    if (!dialog_ref) return null;
    const w = document.createTreeWalker (dialog_ref, NodeFilter.SHOW_ELEMENT);
    let e: Node | null;
    while ( (e = w.nextNode()) ) {
      if (get_uri (e as any) == uri)
        return e;
    }
    return null;
  };

  /// Activate or disable the `kbd=...` hotkeys in menu items.
  const map_kbd_hotkeys = (active = false) => {
    if (keymap_.length) {
      keymap_.length = 0;
      Util.remove_keymap (keymap_);
    }
    keymap_active = !!active;
    if (!keymap_active || !dialog_ref)
      return;
    const w = document.createTreeWalker (dialog_ref, NodeFilter.SHOW_ELEMENT);
    let e: Node | null;
    while ( (e = w.nextNode()) ) {
      const any_e: any = e;
      const keymap_entry = any_e['_keymap_entry'];
      if (keymap_entry instanceof Util.KeymapEntry)
        keymap_.push (keymap_entry);
    }
    if (keymap_.length)
      Util.add_keymap (keymap_);
  };

  const toggle_active_children = async () => {
    const this_isactive = props.isactive; // fetch function prop
    const isactive = async (uri: string) => !uri || !this_isactive || await this_isactive (uri);
    const proms: (Promise<boolean> & { element?: Element })[] = [];
    if (!dialog_ref) return;
    for (let b of dialog_ref.querySelectorAll ('button, .asbutton')) {
      const any_b = b as any;
      const uri = any_b.getAttribute ('uri');
      if (uri === null) continue;
      const promise = isactive (uri) as Promise<boolean> & { element?: Element };
      promise['element'] = b;
      proms.push (promise);
    }
    const toggles = await Promise.all (proms);
    stop_observer();
    for (let i = 0; i < proms.length; i++) {
      const element = proms[i]['element']!, disabled = !toggles[i];
      element.toggleAttribute ('disabled', disabled);
    }
    start_observer();
  };

  const toggle_force_children = (enabled: boolean) => {
    stop_observer();
    if (!dialog_ref) { start_observer(); return; }
    for (let b of dialog_ref.querySelectorAll ('button, .asbutton')) {
      const any_b = b as any;
      const uri = any_b.getAttribute ('uri');
      if (uri === null) continue;
      any_b.toggleAttribute ('disabled', !enabled);
    }
    start_observer();
  };

  const start_observer = () => {
    if (observer_ || !dialog_ref || !document.body.contains (dialog_ref)) return;
    observer_ = new MutationObserver (Util.debounce (() => {
      stop_observer();
      integrate_children();
      if (dialog_ref?.open)
        fit_and_reposition_dialog();
      start_observer();
    }));
    observer_.observe (dialog_ref, { childList: true, subtree: true, attributes: true });
  };

  const stop_observer = () => {
    if (!observer_) return;
    observer_.disconnect();
    observer_ = null;
  };

  /** Integrate a button or summary into ContextMenu handling. */
  function integrate_button (this: HTMLElement)
  {
    const btn = this;
    // Click activation is delegated by the dialog; focus menu items on hover.
    if (!btn.onmouseenter)
      btn.onmouseenter = () => btn.focus();
    // ContextMenu-owned spans must be recreated when `ic` changes so their classes and text update.
    // Do not alter application-owned Icon components, which manage their own reactive updates.
    const ic_value = btn.getAttribute ('ic');
    const contextmenu_icon = btn.querySelector ('.b-icon[data-contextmenu-icon]');
    if (ic_value) {
      if (contextmenu_icon?.getAttribute ('ic') != ic_value) {
        const icon = icon_element (ic_value);
        icon.classList.add ('pointer-events-none');
        icon.toggleAttribute ('data-contextmenu-icon', true);
        if (contextmenu_icon)
          contextmenu_icon.replaceWith (icon);
        else if (!btn.querySelector ('.b-icon'))
          btn.prepend (icon);
      }
    } else
      contextmenu_icon?.remove();
    // aria-label
    const aria_label = text_content (btn, false).trim();
    btn.setAttribute ('aria-label', aria_label);
    // menurow children - turn/noturn based on parent .b-menurow
    const turn = !!Util.closest (btn, '.b-menurow:not(.noturn)');
    btn.toggleAttribute ("turn", turn);
    const noturn = !!Util.closest (btn, '.b-menurow.noturn');
    btn.toggleAttribute ("noturn", noturn);
    // <kbd/>
    const kbds = btn.getAttribute ('kbd');
    if (kbds) {
      const kbd = btn.querySelector ('kbd') || document.createElement ('kbd');
      if (!kbd.parentElement) {
        kbd.className = "pointer-events-none";
        btn.appendChild (kbd);
        kbd.innerText = kbds;
      }
      // hotkey
      if (!(btn as any)['_keymap_entry'])
        (btn as any)['_keymap_entry'] = new Util.KeymapEntry ('', btn.click.bind (btn), btn);
      const menudata = provide_menudata (btn);
      const shortcut = Kbd.shortcut_lookup (menudata.mapname, aria_label, kbds);
      if (shortcut != (btn as any)['_keymap_entry'].key)
        (btn as any)['_keymap_entry'].key = shortcut;
      kbd.innerText = Util.display_keyname (shortcut);
    } else
      btn.querySelector ('kbd')?.remove();
  }

  const integrate_children = () => {
    if (!dialog_ref) return;
    for (let b of dialog_ref.querySelectorAll ('button, .asbutton, summary')) {
      integrate_button.call (b as HTMLElement);
    }
    // rebuild keymap
    map_kbd_hotkeys (keymap_active);
  };

  // === Event Handlers ===

  const handle_click = (event: MouseEvent) => {
    if (allowed_click === event)
      return;
    // Find the button that was clicked via event delegation
    const target = (event.target as Element).closest ('button, .asbutton, summary') as HTMLElement | null;
    if (!target) return;
    const uri = get_uri (target);
    if (!valid_uri (uri))
      return;
    Util.prevent_event (event);
    if (Util.frame_stamp() == menudata.menu_stamp)
      return;
    const isactive = !(target as any).check_isactive ? true : (target as any).check_isactive (false);
    if (isactive instanceof Promise) {
      (async () => (await isactive) && activate_item (event, uri)) ();
      return;
    }
    if (isactive)
      activate_item (event, uri);
  };

  const activate_item = (event: Event, uri: string) => {
    if (allowed_click)
      return;
    if (Util.frame_stamp() == menudata.menu_stamp)
      return;
    if (valid_uri (uri)) {
      menudata.menu_stamp = Util.frame_stamp();
      const proceed = true;
      if (proceed) {
        if (props.activate)
          props.activate (uri, event);
        else {
          const ev = new CustomEvent ('activate', { detail: { uri } });
          props.onactivate?.(ev);
          dialog_ref?.dispatchEvent (ev);
        }
      }
      close();
    } else
      console.error ("ContextMenu.activate_item: invalid uri:", uri);
  };

  const handle_keydown = (event: KeyboardEvent) => {
    if (event.keyCode === 27 && // Escape
        dialog_ref?.open && dialog_ref?.matches ('[open]:modal'))
      return; // bubble up to browser
    if (Util.keydown_move_focus (event))
      return; // handled, no-default
  };

  const handle_close = () => {
    // Called when dialog is closed natively (Escape, backdrop click)
    close();
  };

  // === Lifecycle ===

  onMount (() => {
    if (!dialog_ref) return;
    // Integrate button children (icons, kbd, etc.) after DOM is ready
    integrate_children();
    start_observer();
    // Close on backdrop clicks (regression from Lit migration)
    Util.dialog_backdrop_autoclose (dialog_ref, true);
  });

  onCleanup (() => {
    if (dialog_ref)
      Util.dialog_backdrop_autoclose (dialog_ref, false);
    stop_observer();
    map_kbd_hotkeys (false);
  });

  const set_ref = (el: HTMLDialogElement) => {
    dialog_ref = el;
    // Attach methods to dialog element for imperative access
    // This must happen immediately (not in onMount) because parent effects may call them
    (el as any).popup = popup;
    (el as any).close = close;
    (el as any).map_kbd_hotkeys = map_kbd_hotkeys;
    (el as any).check_isactive = check_isactive;
    (el as any).find_menuitem = find_menuitem;
    // Store menudata for child lookups
    (el as any).menudata = menudata;
    props.ref?.(el);
  };

  return (
    <dialog
      ref={set_ref}
      class={"b-contextmenu" + (props.class ? " " + props.class : "")}
      id={props.id}
      onClick={handle_click}
      onKeyDown={handle_keydown}
      onClose={handle_close}
    >
      <div class="b-contextmenu-inner">
        {props.children}
      </div>
    </dialog>
  );
}
