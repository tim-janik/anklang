// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @class ContextMenu
 * @description
 * The ContextMenu component implements a modal popup that displays contextmenu choices,
 * using menu-entry lists and MenuItem children for custom content.
 * Menu actions are identified via URI attributes, they can be activated by calling a handler
 * which is assigned via the `.activate` property, or the actions can be checked for being disabled
 * by calling a handler which is assigned via the `.isactive` property.
 * Menu items render their own icons and shortcut labels.
 * Using the `popup()` method, the menu can be shown via
 * [HTMLDialogElement.showModal](https://developer.mozilla.org/en-US/docs/Web/API/HTMLDialogElement/showModal).
 * Example:
 * ```tsx
 * <div onContextMenu={e => cm_ref.popup(e)}>
 *   <ContextMenu ref={cm_ref} activate={menuactivation}
 *     items={[{ uri: "quit", label: "Quit", icon: "md-close", kbd: "Shift+Ctrl+Q" }]} />
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
 * : Availability is checked again when an item is activated.
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
 * : Hotkeys use the same availability check as clicks.
 */

import { onMount, onCleanup } from 'solid-js';
import * as Util from "../util.js";
import { get_uri, valid_uri } from '../dom.js';
import * as Dom from "../dom.js";
import { MenuContext, MenuItems, type MenuEntry } from './menuitems';

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
  items?: MenuEntry[];
})
{
  let dialog_ref: HTMLDialogElement | undefined;
  let page_x: number | undefined;
  let page_y: number | undefined;
  let origin_el: Element | null = null;
  let data_contextmenu: Element | null = null;
  const keymap_: Util.KeymapEntry[] = [];
  let resize_observer: ResizeObserver;
  let menu_stamp = 0;

  // === Methods ===

  // Keep a reference to the native close method before we overwrite it on the element
  const native_dialog_close = HTMLDialogElement.prototype.close;

  const close = () => {
    if (dialog_ref?.open) {
      native_dialog_close.call(dialog_ref);
    }
  };

  const menu_context = {
    get mapname () { return props.mapname ?? ''; },
    get showicons () { return props.showicons !== false; },
    isactive: (uri: string) => valid_uri (uri) && (!props.isactive || props.isactive (uri)),
    add_hotkey: (entry: Util.KeymapEntry) => {
      keymap_.push (entry);
      return () => Util.array_remove (keymap_, entry);
    },
  };

  const popup = (event?: Event, popup_options: any = {}) => {
    Util.prevent_event (event);
    if (dialog_ref?.open || Util.frame_stamp() == menu_stamp)
      return false; // duplicate popup request, only popup once per frame
    const origin = popup_options.origin === null ? null : (popup_options.origin || (event as any)?.currentTarget);
    if (origin instanceof Element && !Util.check_visibility (origin))
      return false; // cannot popup around hidden origin
    toggle_force_children (false); // add [disabled] attribute to children
    const toggles = check_isactive(); // concurrently, enable active children
    origin_el = origin instanceof Element ? origin : null;
    menu_stamp = Util.frame_stamp(); // allows one popup per frame
    if (event && (event as any).pageX && (event as any).pageY) {
      page_x = (event as any).pageX;
      page_y = (event as any).pageY;
    } else {
      page_x = page_y = undefined;
    }
    data_contextmenu = popup_options['data-contextmenu'] || origin_el;
    data_contextmenu?.setAttribute ('data-contextmenu', 'true');
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

  // Shortcut buttons stay mounted; their panel controls whether the keymap is active.
  const map_kbd_hotkeys = (active = false) => {
    Util.remove_keymap (keymap_);
    if (active)
      Util.add_keymap (keymap_);
  };

  const toggle_force_children = (enabled: boolean) => {
    for (const button of dialog_ref?.querySelectorAll ('button') ?? [])
      (button as any).set_menu_active (enabled);
  };

  // === Event Handlers ===

  const handle_click = (event: MouseEvent) => {
    // Find the button that was clicked via event delegation
    const target = (event.target as Element).closest ('button, .asbutton, summary') as HTMLElement | null;
    if (!target) return;
    const uri = get_uri (target);
    if (!valid_uri (uri))
      return;
    Util.prevent_event (event);
    if (Util.frame_stamp() == menu_stamp)
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
    if (Util.frame_stamp() == menu_stamp)
      return;
    if (valid_uri (uri)) {
      menu_stamp = Util.frame_stamp();
      if (props.activate)
        props.activate (uri, event);
      else {
        const ev = new CustomEvent ('activate', { detail: { uri } });
        props.onactivate?.(ev);
        dialog_ref?.dispatchEvent (ev);
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

  const handle_close = (event: Event) => {
    toggle_force_children (true);
    origin_el = null;
    data_contextmenu?.removeAttribute ('data-contextmenu');
    data_contextmenu = null;
    props.onclose?.(event);
  };

  // === Lifecycle ===

  onMount (() => {
    if (!dialog_ref) return;
    resize_observer = new ResizeObserver (() => {
      if (dialog_ref.open)
        fit_and_reposition_dialog();
    });
    resize_observer.observe (dialog_ref.querySelector ('.b-contextmenu-inner'));
    // Close on backdrop clicks (regression from Lit migration)
    Util.dialog_backdrop_autoclose (dialog_ref, true);
  });

  onCleanup (() => {
    if (dialog_ref)
      Util.dialog_backdrop_autoclose (dialog_ref, false);
    resize_observer?.disconnect();
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
    props.ref?.(el);
  };

  return (
    <MenuContext.Provider value={menu_context}><dialog
      ref={set_ref}
      class={"b-contextmenu" + (props.class ? " " + props.class : "")}
      id={props.id}
      onClick={handle_click}
      onKeyDown={handle_keydown}
      onClose={handle_close}
    >
      <div class="b-contextmenu-inner">
        <MenuItems items={props.items ?? []} />
        {props.children}
      </div>
    </dialog></MenuContext.Provider>
  );
}
