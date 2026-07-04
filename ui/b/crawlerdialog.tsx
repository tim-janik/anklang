// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class BCrawlerDialog
 * A modal [dialog] that allows file and directory selections.
 *
 * ## Properties:
 * *title*
 * : The dialog title.
 * *button*
 * : Title of the activation button.
 * *cwd*
 * : Initial path to start out with.
 * *filters*
 * : List of file type constraints.
 *
 * ## Events:
 * *select (path)*
 * : This event is emitted when a specific path is selected via clicks or focus activation.
 * *close*
 * : A *close* event is emitted once the "Close" button activated.
 */

import { LitComponent, html, render, noChange, JsExtract, docs, ref, repeat } from '../little.js';
import { hex, basename, dirname, displayfs, displaybasename, displaydirname } from '../strings.js';
import { Signal, State, Computed, Watcher, tracking_wrapper } from "../signal.js";
import { get_uri } from '../dom.js';
import * as Util from "../util.js";
import * as Kbd from "../kbd.js";
import * as Dom from "../dom.js";

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
b-crawlerdialog {
  dialog {
    width: unset; /* <- leave width to INPUT.-file, see below */
    max-width: 95%;
    height: 45em; max-height: 95%;
    overflow-y: hidden;
  }
  input.-direntry {
    @apply pl-[var(--b-button-radius)] pr-[var(--b-button-radius)] text-left outline-0;
    &::selection { background: #2d53c4; }
    z-index: 1;	/* push onto its own layer */
    @apply b-style-inset;
    /* @include b-focus-outline; */
  }
  input.-pathentry {
    @apply rounded-[var(--b-button-radius)] pl-[var(--b-button-radius)] pr-[var(--b-button-radius)] text-left outline-0;
    &::selection { background: #2d53c4; }
    /* <INPUT/> change causes re-layout: https://bugs.chromium.org/p/chromium/issues/detail?id=1116001 */
    z-index: 1;	/* push onto its own layer */
    @apply b-style-inset;
    /* @include b-focus-outline; */
  }
  .-entry-grid {
    @apply grid grow grid-flow-col justify-start justify-items-start border border-solid border-[#222] text-[#eee];
    gap: 5px 10px;
    overflow: scroll hidden;
    grid-template-rows: repeat(auto-fit, 1.5em);
    grid-auto-columns: max-content;
    background: #111;
    color: #eee;
  }
  .-entry-grid > button {
    @apply m-0 inline-block inline-flex cursor-pointer flex-col truncate rounded-none border-[none] p-0 pr-1 text-[unset] no-underline;
    flex-flow: row nowrap;
    min-width: 10em;
    background: unset; font: unset;
    -webkit-appearance: none; -moz-appearance: none;
    &:active { border: none; }
    @include b-focus-outline;
    b-icon {
      width: 1.9rem;
      vertical-align: middle;
      @include b-font-weight-bold();
      &[ic="md-folder"] { color: #bba460; }
    }
  }
}`;

// == HTML ==
const HTML = (t, d) => html`
<dialog class="floating-dialog [&:not([open])]:hidden" ${ref (h => t.dialog = h)} @close=${t.close_click} style="overflow: hidden">
  <div class="dialog-header">${t.title}</div>

  <input class="-direntry pointer-events-none mb-4 select-none outline outline-2 outline-offset-2" ${ref (h => t.direntry = h)} .value="${t.folder}"
	 tabindex="-1" readonly @focus=${e => t.pathentry.focus()} inert
	 type="text" @select=${Util.prevent_event} >

  <div data-subfocus="*" class="-entry-grid grid" ${ref (h => t.entrygrid = h)}
    @keydown=${t.entrygrid_keydown} >
    ${repeat (d.entries, e => e.uri, e => ENTRY_HTML (t, e))}
    <div class="-spin-wrapper hflex"
      style="height: 100%; width: 100%; text-align: center; align-items: center; justify-content: center">
      <div style="text-align: center" > ⥁ </div> </div>
  </div>

  <input class="-pathentry mt-4 outline outline-2 outline-offset-2" ${ref (h => t.pathentry = h)} .value=""
	 type="text" @keydown=${t.pathentry_keydown} @select=${Util.prevent_event} >

  <div class="dialog-footer">
    <button class="button-xl" @click=${e => t.select_entry (null)} @keydown=${Kbd.keydown_move_focus_up}
      ?disabled=${t.update_inflight} > ${t.button} </button>
    <button class="button-xl" @click=${t.close_click}  @keydown=${Kbd.keydown_move_focus_up} > Close </button>
  </div>
</dialog>
`;
const ENTRY_HTML = (t, e) => html`
<button
  @focus=${ev => t.entry_event (ev, e)} @click=${ev => t.entry_event (ev, e)} @dblclick=${ev => t.entry_event (ev, e)} >
  <b-icon ic=${e.type == Ase.ResourceType.FOLDER && "md-folder" || "fa-file_o"} ></b-icon>
  ${e.label}
</button>
`;

// == SCRIPT ==
/** Browse Ase::Crawler contents */
class BCrawlerDialog extends LitComponent {
  createRenderRoot() { return this; }
  render()
  {
    const d = { entries: this.update_inflight ? [] : this.filtered_entries() };
    return HTML (this, d);
  }
  /// properties - override to declare properties
  static properties = {
    title:	{ type: String,  reflect: true }, // sync attribute with property
    button:	{ type: String,  reflect: true }, // sync attribute with property
    cwd:	{ type: String,  state:   true }, // private property
    shown:	{ type: Boolean, reflect: true }, // sync attribute with property
    existing:	{ type: Boolean, reflect: true }, // sync attribute with property
    filters:	{ type: Array,   reflect: true }, // sync attribute with property
    promise:	{ type: Promise, state:   true }, // private property
  };
  /// Ctrl_L - hotkey for focus on path entry
  Ctrl_L = "Ctrl+L";
  constructor()
  {
    super();
    this.title = "File Dialog";
    this.button = "Select";
    this.cwd = "~MUSIC";
    this.shown = false;
    this.existing = true;
    this.filters = [];
    this.dialog = null;
    this.last_cwd = this.cwd;
    this.focus_after_refill = true;
    this.promise = null;
    // this.mkstate ('promise');
    this.current = {};
    this.direntry = null;
    this.pathentry = null;
    this.entrygrid = null;
    // template for reactive Proxy object
    this.ctrl_l_grab_focus = () => {
      this.pathentry.focus();
      this.pathentry.select();
    };
    this.crawler = null;
    (async () => {
      this.crawler = await Ase.server.dir_crawler (this.cwd);
      this.request_update();
    }) ();
  }
  /// folder - getter for the current folder without protocol
  get folder()
  {
    let path = this.crawler?.folder?.uri || '/';
    path = path.replace (/^file:\/+/, '/');	// strip protocol
    return displayfs (path);
  }
  get entries()
  {
    return this.crawler?.entries || [];
  }
  /// update_inflight - indicates if the crawler is asynchronously updating
  get update_inflight()
  {
    return this.promise || this.crawler?.$props?.$promise;
  }
  updated (changed_props)
  {
    if (this.last_cwd != this.cwd && this.crawler)
      this.assign_utf8path (this.last_cwd = this.cwd);
    if (!this.crawler || !this.crawler.entries.length)
      this.focus_after_refill = true;
    if (this.focus_after_refill && this.crawler?.entries.length &&
	document.activeElement === document.body)
      {
	this.focus_after_refill = false;
	this.pathentry.focus();
      }
    if (!this.shown && this.dialog.open)
      this.dialog.close();
    if (this.shown && !this.dialog.open)
      Dom.show_modal (this.dialog);
  }
  connectedCallback()
  {
    super.connectedCallback();
    Kbd.add_hotkey (this.Ctrl_L, this.ctrl_l_grab_focus, this);
  }
  disconnectedCallback()
  {
    Kbd.remove_hotkey (this.Ctrl_L, this.ctrl_l_grab_focus);
    super.disconnectedCallback();
  }
  /// assign_utf8path - assign a path in UTF-8 encoding and possibly select it
  async assign_utf8path (filepath, pickfile = false)
  {
    if (this.promise) return;
    const crawler_assign = async displaypath => {
      const [dir,file] = await this.crawler.assign (displaypath, this.existing);
      if (this.pathentry.value !== file)
	this.pathentry.value = file;
      await this.crawler?.$props?.$promise;
      this.promise = null;
      if (pickfile)
	this.select_entry (null);
    };
    this.promise = crawler_assign (filepath);
    return this.promise;
  }
  entrygrid_keydown (event)
  {
    if (Kbd.match_key_event (event, 'Tab')) {
      Util.prevent_event (event);
      this.pathentry.focus();
    } else
      Kbd.keydown_move_focus (event);
  }
  pathentry_keydown (event)
  {
    if (Kbd.match_key_event (event, 'Enter'))
      this.assign_utf8path (event.target.value, true);
    else if (Kbd.match_key_event (event, 'Shift+Tab')) {
      const entries = Kbd.list_focusables (this.entrygrid);
      if (entries.length) {
	Util.prevent_event (event);
	entries[0].focus();
      }
    }
    //  this.assign_utf8path (event.target.value, false);
    // else Kbd.keydown_move_focus_up (event);
  }
  focus_entry (entry)
  {
    assert (entry.uri && entry.label);
    this.current = entry;
    this.pathentry.value = this.current.label;
  }
  current_is_dir ()
  {
    const uri = this.current?.uri;
    return uri && uri[uri.length - 1] === '/';
  }
  /// entry_event - handle events on the file entries
  entry_event (event, entry)
  {
    if (entry.uri && entry.uri != this.current?.uri)
      this.focus_entry (entry);
    switch (event.type) {
      case 'focus':
	// this.focus_entry
	break;
      case 'dblclick':
	if (!this.promise && this.current?.uri) {
	  if (this.current_is_dir())
	    this.assign_utf8path (this.current.uri);
	  else
	    this.select_entry (entry);
	}
	break;
      case 'click':
	if (event.detail === 0 && // focus + ENTER causes click with detail=0
	    !this.promise && this.current?.uri) {
	  if (this.current_is_dir()) {
	    this.assign_utf8path (this.current.uri);
	    this.pathentry.focus();
	  } else
	    this.select_entry (entry);
	}
	break;
    }
  }
  /// filtered_entries - filter hidden entries
  filtered_entries()
  {
    let e = this.entries;
    // e = e.slice (0, 500); // limit number of entries
    e = e.filter (a => a.label && (a.label == '..' || a.label[0] != '.'));
    e.sort (function (a, b) {
      if (a.type != b.type)
	return a.type > b.type ? -1 : +1;
      if (a.mtime != b.mtime)
	{ /* return a.mtime - b.mtime; */ }
      const al = a.label.toLowerCase(), bl = b.label.toLowerCase();
      if (al != bl)
	return al < bl ? -1 : +1;
      if (a.label != b.label)
	return a.label < b.label ? -1 : +1;
      return 0;
    });
    return e;
  }
  /// select_entry  - send 'select' event for `entry` or `pathentry.value`
  select_entry (entry)
  {
    if (this.update_inflight)
      return false;					// in async update
    // select existing entry
    if (entry?.uri) {
      if (entry.uri[entry.uri.length - 1] === '/')
	return false;					// is_dir
      return this.dispatchEvent (new CustomEvent ('select', { detail: entry }));
    }
    // select pathentry (pathentry.value==='' iff !this.existing)
    const pvalue = ('' + this.pathentry.value).trim();
    if (pvalue && pvalue.search ('/') < 0)
      return this.dispatchEvent (new CustomEvent ('select', { detail: { uri: this.folder + '/' + pvalue } }));
  }
  /// close_click - send 'close' event for the dialog
  close_click (ev = null)
  {
    ev && ev.preventDefault();
    this.dispatchEvent (new CustomEvent ('close'));
  }
}
customElements.define ('b-crawlerdialog', BCrawlerDialog);
