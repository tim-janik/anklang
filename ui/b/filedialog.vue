<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-FILEDIALOG
  A [b-modaldialog] that allows file and directory selections.
  ## Properties:
  *title*
  : The dialog title.
  *button*
  : Title of the activation button.
  *cwd*
  : Initial path to start out with.
  *filters*
  : List of file type constraints.
  ## Events:
  *select (path)*
  : This event is emitted when a specific path is selected via clicks or focus activation.
  *close*
  : A *close* event is emitted once the "Close" button activated.
</docs>

<style lang="scss">
  @import 'mixins.scss';
  .b-filedialog {
    &.b-modaldialog {
      width: 60em; max-width: 95%;
      height: 45em; max-height: 95%;
      overflow-y: hidden;
    }
    & > * { //* avoid visible overflow for worst-case resizing */
      overflow-wrap: break-word;
      min-width: 0; }
    .-body,
    .-browser {
      max-height: 100%; flex-shrink: 1; flex-grow: 1; overflow-y: hidden;
    }
    c-grid {
      column-gap: 10px;
      row-gap: 5px;
    }
    span.-dir {
    }
    span.-file {
      align-self: center;
    }
    .b-folderview {
      max-height: 100%; flex-shrink: 1; flex-grow: 1; overflow-y: hidden;
    }

    input {
      outline: 1px solid red;
      outline-width: 0;
      border: none; border-radius: $b-button-radius;
      text-align: left;
      background-color: rgba(255,255,255,.3); color: #fff;
      padding-left: $b-button-radius; padding-right: $b-button-radius;
      @include b-style-inset;
      @include b-focus-outline;

      &::selection { background: #2d53c4; }
    }
    html[chrome] & input.-file {
      /* Unfortunately chrome <input/> causes re-layout on every value change, which
       * can badly affect editing performance for complex/large document setups:
       *   https://bugs.chromium.org/p/chromium/issues/detail?id=1116001
       * As workaround, push the element onto its own layer and use fixed sizes.
       */
      height: 1.34em; 	//* fixed size */
      width: 40em;	//* fixed size */
      max-width: 50vw;	//* avoid h-scrolling if 40emis too large */
      transform: translate(0px, 0px); //* push onto its own layer */
    }
  }
</style>

<template>
  <b-modaldialog class="b-filedialog" :shown="true" @close="emit_close()" >
    <template v-slot:header>
      <div>File Selector</div>
    </template>

    <c-grid class="-browser" style="grid-template-columns: auto 1fr; grid-template-rows: auto auto 1fr" >
      <span class="-col1 -row1 -dir"> Directory: </span>
      <span class="-col2 -row1 -dir"> {{ directory() }} </span>

      <span  class="-col1 -row2 -file" > File: </span>
      <input class="-col2 -row2 -file" ref="fileentry" type="text" :value="r.filename"
	     @keydown.enter="fileentry_enter ($event)" @change="fileentry_change()"
	     @keydown.down="Util.keydown_move_focus ($event)" >

      <span class="-col1 -row3 -places" > Places </span>
      <b-folderview class="-col2 -row3" ref="folderview" :entries="r.entries"
		    @select="entry_select ($event)" @click="entry_click ($event)" />
    </c-grid>

    <slot></slot>
    <template v-slot:footer>
      <h-flex>
	<button v-if="button" @click="emit_select()" > {{ button }} </button>
	<button @click="emit_close()" > Close </button>
      </h-flex>
    </template>
  </b-modaldialog>
</template>

<script>
import * as Envue from './envue.js';

class FileDialog extends Envue.Component {
  constructor (vm) {
    super (vm);
    this.last_cwd = this.$vm.cwd;
    this.focus_after_refill = true;
    this.select_dir = false;
    // template for reactive Proxy object
    const r = {
      __update__: Util.observable_force_update,
      folder:  { notify: n => this.crawler.on ("notify:current", n),
		 getter: async c => Object.freeze (await this.crawler.current_folder()),
		 default: '', },
      entries: { notify: n => this.crawler.on ("notify:entries", n),
		 getter: async c => Object.freeze (await this.crawler.list_entries()),
		 default: [], },
      filename: '',
    };
    this.r = this.observable_from_getters (r, () => this.crawler);
    this.crawler = null;
    (async () => {
      const cwd = this.last_cwd || this.$vm.cwd;
      this.crawler = await Ase.server.dir_crawler (cwd); // non-reactive assignment
      this.r.__update__(); // so force update
    }) ();
    this.focus_fileentry = () => {
      this.$vm.$refs.fileentry.focus();
      this.$vm.$refs.fileentry.select();
    };
  }
  ctrl_l = "Ctrl+KeyL";
  directory() {
    let path = this.r?.folder?.uri || '/';
    path = path.replace (/^file:\/+/, '/');
    return path;
  }
  async canonify_input (pathfragment, constraindirs = true) {
    // strip file:/// prefix if any
    let path = pathfragment.replace (/^file:\/+/, '/'); // strip protocol
    // canonify and make absolute
    const root = (this.r.folder?.uri || "/").replace (/^file:\/+/, '/'); // strip protocol
    if (this.select_dir && path && !path.endsWith ("/"))
      path += "/";
    path = await this.crawler.canonify (root, path, constraindirs, false);
    // handle directory vs file path
    if (!path.endsWith ('/') && // path is file (maybe notexisting)
	this.$vm.ext)           // ensure extension
      {
	path = path.replace (/[.]+$/, ''); // strip trailing dots
	if (path && !path.endsWith (this.$vm.ext))
	  path += this.$vm.ext;
      }
    return path;
  }
  async assign_path (filepath) {
    let path = await this.canonify_input (filepath);
    // apply directory / file path
    if (path.endsWith ('/')) // filepath is a directory
      this.r.filename = '';
    else // handle filename (maybe notexisting)
      {
	this.r.filename = path.replace (/^.*\//, ''); // basename
	path = path.replace (/[^\/]*$/, ''); // dirname
      }
    // crawler ensures existing directories
    await this.crawler.assign (path);
  }
  file_navigate (newpath) {
    // force Vue to update <input/> even if r.folder.uri stays the same
    if (this.$vm.$refs?.folderview)
      this.$vm.$refs?.folderview.clear_entries(); // performance hack
    this.r.entries = []; // this.r.folder.uri = '⥁';
    // update paths
    return this.assign_path (newpath); // async
  }
  fileentry_enter (event) {
    // <input/> emits @keydown.enter *before* @change, in_async_select handles synchronization
    this.emit_select (async () => {
      // Submit the *unconstrained* input value, it is possible the current value is entered on purpose
      const path = this.$vm.$refs.fileentry.value;
      return await this.canonify_input (path, false);
    });
  }
  async fileentry_change() {
    await this.in_async_select; // synchronize with concurrent emit_select() handling
    if (this.$vm.$refs.fileentry) // $refs.fileentry might be gone after await
      await this.file_navigate (this.$vm.$refs.fileentry.value);
  }
  async entry_select (entry) {
    if (entry.type == Ase.ResourceType.FILE && entry?.uri)
      this.r.filename = entry?.uri.replace (/^.*\//, ''); // basename
  }
  async entry_click (entry) {
    if (entry?.uri)
      await this.emit_select (() => this.canonify_input (entry.uri), true);
  }
  emit_select (pathfunc = undefined, navigate_to_dir = false) {
    if (this.in_async_select)
      return this.in_async_select;
    // allow other parts to synchronize with concurent emit_select() handling
    this.in_async_select = (async () => {
      let path = pathfunc ? pathfunc() : this.canonify_input (this.r.filename);
      path = await path;
      if (this.select_dir || !path.endsWith ('/'))
	await this.$vm.$emit ('select', path);
      else if (navigate_to_dir)
	await this.file_navigate (path);
      this.in_async_select = null;
    }) ();
    return this.in_async_select;
  }
  emit_close (event) {
    event && event.preventDefault();
    this.$vm.$emit ('close');
  }
  mounted() {
    Util.add_hotkey (this.ctrl_l, this.focus_fileentry, this.$vm.$el);
  }
  unmounted() {
    Util.remove_hotkey (this.ctrl_l, this.focus_fileentry, this.$vm.$el);
  }
  dom_update() {
    if (this.last_cwd != this.$vm.cwd && this.crawler)
      this.crawler.assign (this.last_cwd = this.$vm.cwd);
    if (!this.r?.entries?.length)
      this.focus_after_refill = true;
    if (this.focus_after_refill && this.r?.entries?.length &&
	document.activeElement === document.body)
      {
	this.focus_after_refill = false;
	this.$vm.$refs.fileentry.focus();
      }
  }
}

export default FileDialog.vue_export ({
  sfc_template,
  props: { title:	{ type: String, default: "File Dialog" },
	   button:	{ type: String, default: "Select" },
	   cwd:		{ type: String, default: "" },
	   filters:	{ type: Array, default: [] }, },
  emits: { select: null, close: null, },
});
</script>