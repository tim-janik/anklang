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
      this.crawler = await Ase.server.cwd_crawler(); // non-reactive assignment
      if (this.last_cwd)
	this.crawler.assign (this.last_cwd);
      this.r.__update__(); // so force update
    }) ();
    this.focus_fileentry = () => {
      this.$vm.$refs.fileentry.focus();
      this.$vm.$refs.fileentry.select();
    };
  }
  ctrl_l = "Ctrl+KeyL";
  directory() {
    let path = "" + this.r?.folder?.uri;
    path = path.replace (/^file:\/+/, '/');
    return path;
  }
  async assign_path (filepath) {
    // file:-URI to abspath
    filepath = filepath.replace (/^file:\/+/, '/'); // strip protocol
    // special expansions
    if (filepath.startsWith ('~/') || filepath == '~') {
      filepath = (await this.crawler.get_dir ("home")) + filepath.substr (1);
    }
    // make absolute
    if (!filepath.startsWith ('/'))
      {
	filepath = this.r.folder.uri + '/' + filepath;
	filepath = filepath.replace (/^file:\/+/, '/'); // strip protocol
      }
    // handle directory selection
    const asdir = await this.crawler.asdir (filepath);
    if (asdir) // filepath is a directory
      {
	filepath = asdir;
	this.r.filename = '';
      }
    else // dealing with a (maybe notexisting) filename
      {
	this.r.filename = filepath.replace (/^.*\//, ''); // basename
	if (this.$vm.ext) {
	  this.r.filename = filepath.replace (/[.]+$/, ''); // strip trailing dots
	  if (this.r.filename && !this.r.filename.endsWith (this.$vm.ext))
	    this.r.filename += this.$vm.ext;
	}
	filepath = filepath.replace (/[^\/]*$/, ''); // dirname
      }
    // canonify dirname, ensure directory exists
    filepath = await this.crawler.canonify (filepath, "e");
    if (!filepath.endsWith ('/'))
      filepath += '/';
    await this.crawler.assign (filepath);
    return !asdir; // true if assignment is a file, not dir
  }
  fileentry_enter (event) {
    this.fileentry_enter_pressed = document.activeElement === this.$vm.$refs.fileentry;
    // <input/> emits @keydown.enter *before* @change, so set a flag
  }
  async fileentry_change() {
    await this.file_navigate (this.$vm.$refs.fileentry.value);
    const submit = this.fileentry_enter_pressed && document.activeElement === this.$vm.$refs.fileentry;
    this.fileentry_enter_pressed = false;
    if (submit)
      this.emit_select();
  }
  async file_navigate (newpath) {
    // force Vue to update <input/> even if r.folder.uri stays the same
    if (this.$vm.$refs?.folderview)
      this.$vm.$refs?.folderview.clear_entries(); // performance hack
    this.r.entries = []; // this.r.folder.uri = '⥁';
    // update paths
    return await this.assign_path (newpath);
  }
  async entry_select (entry) {
    if (entry.type == Ase.ResourceType.FILE && entry?.uri)
      this.r.filename = entry?.uri.replace (/^.*\//, ''); // basename
  }
  async entry_click (entry) {
    let has_file = false;
    if (entry?.uri)
      has_file = await this.file_navigate (entry.uri);
    if (has_file)
      await this.emit_select();
  }
  async emit_select (allow_dir = false) {
    const dir = await this.crawler.get_dir (".");
    const path = (dir.endsWith ('/') ? dir : dir + '/') + this.r.filename;
    if (allow_dir || this.r.filename)
      this.$vm.$emit ('select', path);
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
