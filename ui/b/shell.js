// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-SHELL
 * User interface shell for editing and display of a Ase.Project.
 * Instance access is provided via the global `Shell` constant.
 *
 * ### Properties:
 * *project*
 * : Implicit *Ase.Project*, using App.project().
 */

import { createSignal, createStore, createEffect, onMount, onCleanup, For, Show } from 'solid-js';
import { render } from 'solid-js/web';
import { css, JsExtract } from '../little.js';
import * as Signal from "../signal.js";
import * as Util from "../util.js";
import * as Ase from '/gen/aseapi.js';
import * as Dom from "../dom.js";
import DataBubbleIface from '../b/databubble.js';
import spinner_svg from '/gen/assets/spinner.svg'

// Import make_reactive from app.js or use global
const make_reactive = window.make_reactive;

// == STYLE ==
Extra_css`
/* global.scss includes @import 'spinner.css'; */

b-shell {
  display: grid;
  position: relative;
  --b-resize-handle-thickness: var(--b-resize-handle-thickness);
  --b-transition-fast-slide: var(--b-transition-fast-slide);
  width: 100%;
  height: 100%;
  justify-content: space-between;
  align-items: stretch;
  user-select: none;
}
.b-shell-sidebar {
  padding: 3px;
  /* overflow: hidden scroll; */
  text-overflow: ellipsis; white-space: nowrap;
}
.b-shell-resizer {
  width: var(--b-resize-handle-thickness);
  background: var(--b-resize-handle-bgcolor);
  border-left: var(--b-resize-handle-border);
  border-right: var(--b-resize-handle-border);
  cursor: col-resize;
}
html.b-shell-during-drag .b-app {
  .b-shell-resizer { background: var(--b-resize-handle-hvcolor); }
  * { cursor: col-resize !important; user-select: none !important; }
}

b-shell .-modal-message {
  .-hfooter {
    justify-content: space-between;
    button, .asbutton {
      white-space: nowrap;
      --hpadding: 0.75em;
      padding-left: var(--hpadding); padding-right: var(--hpadding);
    }
    &.-manybuttons {
      width: 100%;
      button, .asbutton {
	width: 100%;
      }
    }
  }
}

b-shell {
  .-fullcoverage {
    position: fixed; inset: 0;
    width: 100%; height: 100%;
    display: flex;
    pointer-events: none;
  }
}
#b-shell-spinner-layer {
  display: flex;
  img {
    --size: 4em;
    width: var(--size); height: var(--size);
    margin: auto; display: inline-block; vertical-align: middle;
    animation: 1.1s linear infinite reverse spinner-svg-rotation-steps;
  }
}

b-shell {
  grid-template-columns: min(10em,12%) 1fr min(10em,12%);
  grid-template-rows:
      [menubar] 0fr
      [tracklist] 1fr
      [devices] min-content
      [pianoroll] min-content
      [row5] 0fr
      [row6] 0fr
      [row7] 0fr
      [row8] 0fr
      [statusbar] 0fr;
  place-items: stretch;
  place-content: space-between;
  .-col123		{ grid-column: 1/4; }
  .-col12		{ grid-column: 1/3; }
  .-col1		{ grid-column: 1/2; }
  .-col2		{ grid-column: 2/3; }
  .-col3		{ grid-column: 3/4; }
  .-row1 		{ grid-row: 1/2; }
  .-row2 		{ grid-row: 2/3; }
  .-row3 		{ grid-row: 3/4; }
  .-row4 		{ grid-row: 4/5; }
  .-row5 		{ grid-row: 5/6; }
  .-row6 		{ grid-row: 6/7; }
  .-row7 		{ grid-row: 7/8; }
  .-row8 		{ grid-row: 8/9; }
  .-row9 		{ grid-row: 9/10; }
  .-row28 		{ grid-row: 2/9; }
  .-small		{ max-height: 10em; }
}
`;

// == DIALOG COMPONENT ==
const DialogComponent = (props) => {
  const { dialog: d } = props;
  
  return (
    <dialog 
      class="-modal-message"
      id={`MDialog_${d.dialogid}`} 
      classList={{ [d.class]: !!d.class }}
      shown={d.visible} 
      onClose={(event) => d.input(event)}
      exclusive={true} 
      bwidth="9em" 
      style="z-index: 93">
      
      {/* Header */}
      <div slot="header">
        {d.header}
      </div>
      
      {/* Default content */}
      <div slot="default">
        <div class="hflex" style="justify-content: flex-start; align-items: center;">
          <b-icon {...d.icon}></b-icon>
          <div style="flex-grow: 1; white-space: pre-line;">{d.body}</div>
          <div style="flex-grow: 1; white-space: pre-line;" innerHTML={d.vhtml}></div>
        </div>
        <Show when={d.proplist}>
          <b-fed-object class="-modal-fed" value={d.proplist}></b-fed-object>
        </Show>
        <Show when={d.div_handler}>
          <div class="-div-handler"></div>
        </Show>
      </div>
      
      {/* Footer */}
      <div slot="footer">
        <div class="hflex -hfooter" classList={{ [d.footerclass]: !!d.footerclass }}>
          <For each={d.buttons}>
            {(b, i) => {
              const Component = b.canfocus ? 'button' : 'asbutton';
              return (
                <Component
                  onClick={(ev) => d.click(i())}
                  disabled={b.disabled}
                  autofocus={b.autofocus}>
                  {b.label}
                </Component>
              );
            }}
          </For>
        </div>
      </div>
    </dialog>
  );
};

// == JSX TEMPLATE ==
const ShellTemplate = (props) => {
  const { m, fs, pianoRollRef, aboutdialogRef, show_about_dialog_, setShowAboutDialog } = props;
  
  const show_about_dialog = (onof = undefined) => {
    if (undefined === onof)
      return show_about_dialog_();
    onof = !!onof;
    if (onof !== show_about_dialog_()) {
      document.startViewTransition(async () => {
        setShowAboutDialog(onof);
        // In SolidJS, updates are automatic, no need for requestUpdate
      });
    }
  };
  
  return (
    <>
      {/* Menus and Transport */}
      <b-menubar class="-row1 -col123" project={Data.project}></b-menubar>

      {/* tracks and clips */}
      <b-tracklist class="-row2 -col2" style="overflow: hidden" project={Data.project}></b-tracklist>

      {/* devices */}
      <b-devicepanel 
        class="-row3 -col2" 
        hidden={Data.panel2 !== 'd'} 
        track={App.current_track}>
      </b-devicepanel>

      {/* piano roll */}
      <b-piano-roll 
        class="-row4 -col2" 
        style="overflow: hidden; height:50vh" 
        clip={Data.piano_roll_source}
        ref={pianoRollRef}
        hidden={Data.panel2 !== 'p'}>
      </b-piano-roll>

      {/* browser */}
      <div class="b-shell-sidebar vflex -row28 -col1">
        Browser <br />
        <b-treebrowser tree={m.filetree} hidden={Data.panel3 == 'b'}></b-treebrowser>
        <Show when={Data.panel3 !== 'i'}>
          <span><a href="">Info Panel</a></span>
        </Show>
      </div>

      {/* Inspector */}
      <div class="vflex -row28 -col3" style="margin-left: 3em">
        ||| <br />
        Editor <br />
        ||| <br />
      </div>

      {/* status bar */}
      <b-statusbar class="-row9 -col123"></b-statusbar>

      {/* Modal Dialogs */}
      <b-aboutdialog 
        ref={aboutdialogRef} 
        shown={show_about_dialog_()} 
        onClose={(ev) => show_about_dialog(0)}>
      </b-aboutdialog>

      <b-preferencesdialog 
        shown={Data.show_preferences_dialog} 
        onClose={(ev) => (Data.show_preferences_dialog = false)}>
      </b-preferencesdialog>

      <b-crawlerdialog 
        shown={!!fs.resolve} 
        title={fs.title} 
        filters={fs.filters} 
        button={fs.button}
        existing={fs.existing} 
        cwd={fs.cwd} 
        onClose={(ev) => fs.resolve()} 
        onSelect={(event) => fs.resolve(event.detail?.uri)}>
      </b-crawlerdialog>

      <div class="-fullcoverage" style="z-index: 90" id="b-app-shell-modaldialogs">
        {/* Modal Message Popups */}
        <For each={m.modal_dialogs}>
          {(d) => <DialogComponent dialog={d} />}
        </For>
      </div>

      {/* Noticeboard */}
      <b-noticeboard style="z-index: 95"></b-noticeboard>

      {/* Bubbles */}
      <div class="-fullcoverage" style="z-index: 96" id="b-shell-bubble-layer"></div>

      {/* Spinners (busy indicator) */}
      <Show when={m.show_spinner_count > 0}>
        <div class="-fullcoverage" style="z-index: 98" id="b-shell-spinner-layer">
          <img src={spinner_svg} />
        </div>
      </Show>
    </>
  );
};
// == SCRIPT ==
async function list_sample_files() {
  // TODO: const crawler = await Ase.server.resource_crawler();
  const entries = []; // TODO: await crawler.list_files ('wave', 'user-downloads');
  return Object.freeze ({ entries: entries });
}

// == SOLIDJS SHELL COMPONENT ==
function BShell(input_m = {}) {
  // State signals 
  const [fs, setFs] = createSignal({ 
    title: 'File Selector', 
    button: 'Select', 
    cwd: '~MUSIC', 
    filters: [], 
    resolve: null 
  });
  
  const [show_about_dialog_, setShowAboutDialog] = createSignal(false);
  const [note_cache, setNoteCache] = createSignal({});
  const [piano_current_clip, setPianoCurrentClip] = createSignal(null);
  const [piano_current_tick, setPianoCurrentTick] = createSignal(null);
  
  // Initialize m with reactive properties
  const initial_m = Object.assign({
    modal_dialogs: [],
    show_spinner_count: 0,
    filetree: list_sample_files(),
    show_about_dialog_: false
  }, input_m);
  
  const m = make_reactive(initial_m);
  
  // Component refs
  let piano_roll_ = null;
  let aboutdialog_ = null;
  let data_bubble = null;
  
  // Internal state for sidebar resizing
  let listening = null;
  let startx = 0;
  let startwidth = 0;
  
  // Component methods converted to functions
  const show_spinner = () => {
    m.show_spinner_count++;
  };
  
  const hide_spinner = () => {
    console.assert(m.show_spinner_count > 0);
    m.show_spinner_count--;
  };
  
  const show_about_dialog = (onof = undefined) => {
    if (undefined === onof)
      return show_about_dialog_();
    onof = !!onof;
    if (onof !== show_about_dialog_()) {
      document.startViewTransition(async () => {
        setShowAboutDialog(onof);
        const aboutdialog_promise = onof ? null : aboutdialog_?.close_dialog?.();
        // In SolidJS, updates are automatic, no need for requestUpdate
        await Promise.all([aboutdialog_promise].filter(Boolean));
      });
    }
  };
  
  const usernote = (user_note_event) => {
    App.show_notice(user_note_event.text);
  };
  
  const f1_help = (event) => {
    const zlast = App.zmove_last();
    const el_f1 = Util.find_element_from_point(document, zlast.pageX, zlast.pageY, el => {
      const str = el.getAttribute('data-f1');
      return str && str[0] === '#'; // treat as anchor into manual
    });
    const data_f1 = el_f1 && el_f1.getAttribute('data-f1') || '#using-anklang';
    const u = location.origin + '/doc/anklang-manual.html' + data_f1;
    window.open(u, '_blank');
    Util.prevent_event(event);
    return true;
  };
  
  const sidebar_mouse = (e) => {
    const sidebar = document.querySelector('.b-shell-sidebar'); // Direct DOM query since no shadow root
    console.assert(sidebar);
    const html_classes = document.documentElement.classList;
    if (e.type == 'mousedown' && !listening) {
      listening = Util.debounce(sidebar_mouse);
      document.addEventListener('mousemove', listening);
      document.addEventListener('mouseup', listening);
      startx = e.clientX;
      startwidth = sidebar.getBoundingClientRect().width;
      html_classes.add('b-shell-during-drag');
    }
    if (listening && e.type == 'mouseup') {
      document.removeEventListener('mousemove', listening);
      document.removeEventListener('mouseup', listening);
      listening = undefined;
      html_classes.remove('b-shell-during-drag');
    }
    let newwidth = startwidth - (e.clientX - startx);
    const pwidth = sidebar.parentElement.getBoundingClientRect().width;
    const maxwidth = pwidth * 0.6 | 0, minwidth = 120;
    if (newwidth < minwidth / 2) {
      const cs = getComputedStyle(sidebar);
      newwidth = parseInt(cs.getPropertyValue('--b-resize-handle-thickness'), 10);
    } else {
      newwidth = Util.clamp(newwidth, minwidth, maxwidth);
    }
    sidebar.style.transition = newwidth > minwidth ? "" : "width var(--b-transition-fast-slide)";
    const flexwidth = '0 0 ' + (newwidth / pwidth) * 100 + '%';
    if (flexwidth != sidebar.style.flex)
      sidebar.style.flex = flexwidth;
    e.preventDefault();
  };
  
  const select_file = async (opt = {}) => {
    if (fs().resolve)
      return undefined;
    const newFs = Object.assign({}, fs(), opt);
    newFs.existing === false || (newFs.existing = true);
    let resolve;
    const fileselector_promise = new Promise(r => resolve = r);
    newFs.resolve = path => { // assignment shows file selector
      setFs(prev => ({ ...prev, resolve: null })); // reset hides file selector
      resolve(path);
    };
    setFs(newFs);
    return await fileselector_promise;
  };
  
  // Note cache methods
  const _update_note_cache = async (clip) => {
    const cache = note_cache()[clip.$id];
    while (cache.dirty) {
      cache.dirty = 0;
      const notes = await clip.list_all_notes();
      cache.notes = Util.freeze_deep(notes);
      cache.rgen.value += 1; // trigger reactivity hooks
      for (const cb of cache.callbacks_)
        cb();
    }
    cache.promise = null;
  };
  
  const get_note_cache = (clip) => {
    const currentCache = note_cache();
    if (!currentCache[clip.$id]) {
      const cache = {
        rgen: { value: 1 },
        destroynotify: null, promise: null, dirty: 0,
        callbacks_: [],
        notes: Object.freeze([])
      };
      const update_note_cache = () => {
        cache.dirty++;
        if (!cache.promise)
          cache.promise = _update_note_cache(clip);
      };
      cache.destroynotify = clip.on("notify:notes", update_note_cache);
      setNoteCache(prev => ({ ...prev, [clip.$id]: cache }));
      update_note_cache();
    }
    const cache = note_cache()[clip.$id];
    return Object.freeze({
      get gen() {
        return cache.rgen.value;
      },
      get notes() {
        return cache.rgen.value && cache.notes;
      },
      add_callback(cb) { cache.callbacks_.push(cb); },
      del_callback(cb) { return Util.array_remove(cache.callbacks_, cb); }
    });
  };
  
  const note_cache_notes = async (clip) => {
    get_note_cache(clip);
    const cache = note_cache()[clip.$id];
    await cache.promise;
    return cache.rgen.value && cache.notes;
  };
  
  const old_cache_notes = (clip) => {
    return get_note_cache(clip).notes;
  };
  
  // Lifecycle hooks
  onMount(() => {
    // Initialize data bubble
    data_bubble = new DataBubbleIface({ 
      // Pass necessary shell methods to DataBubbleIface
      m,
      shadowRoot: null // No shadow root in SolidJS
    });
    
    // Setup hotkeys
    const switch_panel2 = App.switch_panel2.bind(App);
    Util.add_hotkey('RawBackquote', switch_panel2);
    const switch_panel3 = App.switch_panel3.bind(App);
    Util.add_hotkey('I', switch_panel3);
    Util.add_key_filter(112, f1_help); // F1
    
    // Setup user note hook
    m.usernotehook = Ase.server.on("usernote", usernote);
    
    // Create shell API object for global access
    const shellAPI = {
      // Properties
      get piano_roll() { return piano_roll_; },
      get fs() { return fs(); },
      m,
      get note_cache() { return note_cache(); },
      get piano_current_clip() { return piano_current_clip(); },
      get piano_current_tick() { return piano_current_tick(); },
      
      // Methods
      show_spinner,
      hide_spinner,
      show_about_dialog,
      usernote,
      f1_help,
      sidebar_mouse,
      select_file,
      get_note_cache,
      note_cache_notes,
      old_cache_notes,
      async_modal_dialog: function(dialog_setup) {
        return async_modal_dialog.call(this, dialog_setup);
      }
    };
    
    // Make shell globally accessible
    if (!window.Shell) {
      Object.defineProperty(window, 'Shell', { 
        value: shellAPI, 
        enumerable: true, 
        configurable: true 
      });
    }
    
    // Cleanup function
    onCleanup(() => {
      Util.remove_hotkey('RawBackquote', switch_panel2);
      Util.remove_hotkey('I', switch_panel3);
      Util.remove_key_filter(112, f1_help); // F1
      App.shell_unmounted();
    });
  });
  
  // Effect for handling modal dialog div handlers
  createEffect(() => {
    for (const d of m.modal_dialogs) {
      if (d.div_handler) {
        const dialog = document.querySelector(`#MDialog_${d.dialogid}`);
        if (dialog) {
          const div = dialog.querySelector(`.-div-handler`);
          if (div)
            d.div_handler(div, dialog);
        }
      }
    }
  });
  
  return (
    <ShellTemplate 
      m={m}
      fs={fs()}
      pianoRollRef={(el) => piano_roll_ = el}
      aboutdialogRef={(el) => aboutdialog_ = el}
      show_about_dialog_={show_about_dialog_}
      setShowAboutDialog={setShowAboutDialog}
    />
  );
}
// Custom element registration for SolidJS component
customElements.define('b-shell', class extends HTMLElement {
  constructor() {
    super();
    this.dispose = null;
  }
  
  connectedCallback() {
    // Create a SolidJS root and render the component
    this.classList.add("b-shell");
    
    // Render the shell component
    this.dispose = render(() => BShell(), this);
  }
  
  disconnectedCallback() {
    // Cleanup SolidJS component
    if (this.dispose) {
      this.dispose();
      this.dispose = null;
    }
  }
});

// == modal dialog creation ==
let modal_dialog_counter = 1;
function async_modal_dialog(dialog_setup) {
  const shell = window.Shell || this; // Use global Shell or fallback to this
  let resolve;
  const promise = new Promise(r => resolve = r);
  const [get_visible, set_visible] = Signal.createSignal(false);
  const m = {
    dialogid: modal_dialog_counter++,
    div_handler: dialog_setup.div_handler,
    class: dialog_setup.class,
    proplist: dialog_setup.proplist || [],
    get visible() { return get_visible(); },
    set visible(v) { set_visible(v); },
    input(v) {
      if (!this.visible || v)
        return;
      this.visible = false;
      if (dialog_setup.destroy)
        dialog_setup.destroy();
      resolve(this.result);
      setTimeout(_ => Util.array_remove(shell.m.modal_dialogs, this), CONFIG?.transitiondelay || 300);
    },
    result: -1,
    click(r) {
      this.result = r;
      this.input(false);
    },
    header: dialog_setup.title,
    body: dialog_setup.text,
    vhtml: dialog_setup.html,
    icon: dialog_emblems[dialog_setup.emblem] || {},
    footerclass: '',
    buttons: []
  };
  const is_string = s => typeof s === 'string' || s instanceof String;
  const check_bool = (v, dflt) => v !== undefined ? !!v : dflt;
  const buttons = dialog_setup.buttons;
  for (let i = 0; i < buttons.length; i++)
    {
      const label = is_string (buttons[i]) ? buttons[i] : buttons[i].label;
      const disabled = check_bool (buttons[i].disabled, false);
      const canfocus = check_bool (buttons[i].canfocus, true);
      const autofocus = check_bool (buttons[i].autofocus, false);
      const button = { label, disabled, autofocus, canfocus };
      m.buttons.push (button);
    }
  if (m.buttons.length >= 2)
    m.footerclass = '-manybuttons';
  shell.m.modal_dialogs.push (m);
  setTimeout (_ => m.visible = true, 0); // changing value triggers animation
  return promise;
}
const dialog_emblems = {
  PIANO:	{ mi: "piano",			style: "font-size: 300%; padding-right: 1rem; float: left; color: #ffbbbb" },
  QUESTION:	{ fa: "question-circle",	style: "font-size: 300%; padding-right: 1rem; float: left; color: #538cc1" },
  ERROR:	{ fa: "times-circle",		style: "font-size: 300%; padding-right: 1rem; float: left; color: #cc2f2a" },
  KEYBOARD:	{ mi: "keyboard",		style: "font-size: 300%; padding-right: 1rem; float: left; color: #538cc1" },
};
