<!-- This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0 -->

<docs>
  # B-MENUBAR
  The main menu bar at the top of the window.
</docs>

<style lang="scss">
  @import 'mixins.scss';
  .b-menubar {
    margin: 5px;
    .-stack {
      display: inline-block;
      position: relative;
      vertical-align: middle;
      width: 1em; height: 1em;
      & :not(:first-child) { position: absolute; top: 0; left: 0; }
    }
  }
</style>

<template>

  <!-- main menu & controlbar -->
  <h-flex class="b-menubar" space-between >

    <!-- menubar left -->
    <b-button-bar class="-menubar" >
      <!-- File Menu -->
      <push-button data-tip="**CLICK** File Menu" data-hotkey="Alt+F" @click="Util.dropdown ($refs.filemenu, $event)" >
	<div class="-stack" >
	  <b-icon bc="folder" />
	  <b-icon bc="menumore" />
	</div>
	<b-contextmenu ref="filemenu" @click="activation" startfocus keepmounted >
	  <b-menuitem fa="file-o"	kbd="Ctrl+N"		uri="loadnew" >	New Project		</b-menuitem>
	  <b-menuitem fa="file-audio-o" kbd="Ctrl+O"		uri="load"    >	Open Project…		</b-menuitem>
	  <b-menuitem mi="save_alt"     kbd="Ctrl+S"		uri="save"    >	Save Project		</b-menuitem>
	  <b-menuitem fa="save"		kbd="Shift+Ctrl+S"	uri="saveas"  >	Save As…		</b-menuitem>
	  <b-menuseparator style="margin: 7px"  />
	  <b-menuitem fa="cog"         kbd="Ctrl+RawComma"	uri="prefs">	Preferences		</b-menuitem>
	  <b-menuseparator style="margin: 7px"  />
	  <b-menuitem mi="close" kbd="Shift+Ctrl+Q" uri="quit">	Quit			</b-menuitem>
	</b-contextmenu>
      </push-button>

      <!-- Edit Menu -->
      <push-button data-tip="**CLICK** Edit Menu" data-hotkey="Alt+E" @click="Util.dropdown ($refs.editmenu, $event)" >
	<div class="-stack" >
	  <b-icon mi="draw" />
	  <b-icon bc="menumore" />
	</div>
	<b-contextmenu ref="editmenu" @click="activation" :check="deactivation" startfocus keepmounted >
	  <b-menuitem mi="undo" :disabled="!true"
		      kbd="Ctrl+Z" uri="undo">	Undo	</b-menuitem>
	  <b-menuitem mi="redo" :disabled="!true"
		      kbd="Shift+Ctrl+Z" uri="redo">	Redo	</b-menuitem>
	</b-contextmenu>
      </push-button>

      <!-- View Menu -->
      <push-button data-tip="**CLICK** View Menu" data-hotkey="Alt+V" @click="Util.dropdown ($refs.viewmenu, $event)" >
	<div class="-stack" >
	  <b-icon fa="eye" />
	  <b-icon bc="menumore" />
	</div>
	<b-contextmenu ref="viewmenu" @click="activation" startfocus keepmounted >
	  <b-menuitem mi="fullscreen" :disabled="!document.fullscreenEnabled"
		      kbd="F11" uri="fullscreen">	Toggle Fullscreen	</b-menuitem>
	</b-contextmenu>
      </push-button>

    </b-button-bar>

    <!-- playcontrols -->
    <h-flex>
      <b-playcontrols :project="project"> </b-playcontrols>
      <b-positionview :project="project"> </b-positionview>
    </h-flex>

    <!-- menubar right -->
    <b-button-bar class="-menubar" >
      <!-- Help Menu -->
      <push-button data-tip="**CLICK** Help Menu" data-hotkey="Alt+H"
		@click="Util.dropdown ($refs.helpmenu, $event)" >
	<div class="-stack" >
	  <b-icon fa="life-ring" />
	  <b-icon bc="menumore" />
	</div>
	<b-contextmenu ref="helpmenu" @click="activation" startfocus >
	  <b-menuitem mi="chrome_reader_mode"	uri="manual">	Anklang Manual…		</b-menuitem>
	  <b-menuseparator style="margin: 7px"  />
	  <b-menuitem fa="id-card-o"		uri="about">	About…			</b-menuitem>
	</b-contextmenu>
      </push-button>
    </b-button-bar>

  </h-flex>

</template>

<script>
import * as Ase from '../aseapi.js';

export default {
  sfc_template,
  props: {
    song: { type: Ase.Song, },
    project: { type: Ase.Project, },
  },
  mounted() {
    this.$refs.filemenu.map_kbd_hotkeys (true);
    this.$refs.editmenu.map_kbd_hotkeys (true);
    this.$refs.viewmenu.map_kbd_hotkeys (true);
  },
  methods: {
    window_close() {
      window.close();
      // when we're running in the browser, window.close() might not work, so...
      setTimeout (() => {
	window.location.href = 'about:blank';
      }, 0);
    },
    async deactivation (uri) {
      switch (uri) {
	case 'undo':
	  return this.project.can_undo();
	case 'redo':
	  return this.project.can_redo();
	default:
	  return true;
      }
    },
    async activation (uri, event) {
      let u, v;
      switch (uri) {
	case 'quit_discard':
	  window.close();
	  break;
	case 'quit':
	  v = App.async_button_dialog ("Save Project?",
				       "The current project may contain unsaved changes.\n" +
				       "Save changes to the project before closing?",
				       (window.Electron ?
					[ 'Discard Changes', { label: 'Cancel', autofocus: true }, 'Save' ] :
					[ 'Discard Changes', { label: 'Cancel', autofocus: true },
					  { label: 'Save', disabled: true } ]),
				       'QUESTION');
	  v = await v;
	  if (v == 0)
	    return this.window_close();
	  if (v == 2 && await save_project())
	    return this.window_close();
	  break;
	case 'about':
	  Data.show_about_dialog = !Data.show_about_dialog;
	  break;
	case 'manual':
	  u = location.origin + '/doc/anklang-manual.html';
	  window.open (u, '_blank');
	  break;
	case 'prefs':
	  Data.show_preferences_dialog = !Data.show_preferences_dialog;
	  break;
	case 'fullscreen':
	  if (document.fullscreen)
	    document.exitFullscreen();
	  else
	    document.body.requestFullscreen();
	  break;
	case 'undo':
	  await this.project.undo();
	  break;
	case 'redo':
	  await this.project.redo();
	  break;
	case 'loadnew':
	  App.load_project_checked();
	  break;
	case 'load':
	  open_file();
	  break;
	case 'save':
	  save_project();
	  break;
	case 'saveas':
	  save_project (true);
	  break;
      }
    },
  },
};

async function open_file() {
  const opt = {
    title:  Util.format_title ('Anklang', 'Select File To Open'),
    button: 'Open File',
    cwd:    open_file.last_dir || "MUSIC",
    filters: [ { name: 'Projects', extensions: ['anklang'] }, // TODO: filters
	       { name: 'Audio Files', extensions: [ 'anklang', 'mid', 'wav', 'mp3', 'ogg' ] },
	       { name: 'All Files', extensions: [ '*' ] }, ],
  };
  const filename = await App.shell.select_file (opt);
  if (filename)
    {
      open_file.last_dir = filename.replace (/[^\/]*$/, ''); // dirname
      const lresult = await App.load_project_checked (filename);
      if (lresult == Ase.Error.NONE)
	{
	  let project_filename = filename;
          Data.project.set_custom ('filename', project_filename);
	}
      // else console.error ('Failed to load:', filename); // TODO: warning notice?
    }
}

async function save_project (asnew = false) {
  const opt = {
    title:  Util.format_title ('Anklang', 'Save Project'),
    button: 'Save As',
    cwd:    save_project.last_dir || "MUSIC",
    filters: [ { name: 'Projects', extensions: ['anklang'] }, ],
  };
  let filename = ''; // TODO: await Data.project.get_custom ('filename');
  let replace = asnew ? 0 : !!filename;
  if (asnew || !filename)
    filename = await App.shell.select_file (opt);
  if (!filename)
    return false;
  save_project.last_dir = filename.replace (/[^\/]*$/, ''); // dirname
  //if (!filename.endsWith ('.anklang'))
  //  filename += '.anklang';
  if (0 && !replace) // TODO: if file_exists (filename)
    {
      replace = await App.async_button_dialog ("Replace Project?",
					       "Replace existing project file?\n" +
					       filename + ": File exists", [
						 'Cancel',
						 { label: 'Replace', autofocus: true }, ],
					       'QUESTION');
      if (replace != 1)
	return false;
    }
  const err = await App.save_project (filename);
  if (err === Ase.Error.NONE)
    {
      Data.project.set_custom ('filename', filename);
      let msg = '# Project Saved\n';
      msg += '  \n  \nProject saved to: ``' + filename + '``\n';
      Util.create_note (msg);
      return true;
    }
  App.async_button_dialog ("File IO Error",
			   "Failed to save project.\n" +
			   filename + ": " +
			   await Ase.server.error_blurb (err), [
			     { label: 'Dismiss', autofocus: true }
			   ], 'ERROR');
  return false;
}

</script>
