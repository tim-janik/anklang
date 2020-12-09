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
      <push-button data-tip="**CLICK** File Menu" data-hotkey="Alt+KeyF" @click="Util.dropdown ($refs.filemenu, $event)" >
	<div class="-stack" >
	  <b-icon bc="folder" />
	  <b-icon bc="menumore" />
	</div>
	<b-contextmenu ref="filemenu" @click="activation" startfocus keepmounted >
	  <b-menuitem fa="file-o"	kbd="Ctrl+KeyN"		uri="loadnew"			   >	New Project		</b-menuitem>
	  <b-menuitem fa="file-audio-o" kbd="Ctrl+KeyO"		uri="load"   :disabled="!window.Electron" >	Open Project…		</b-menuitem>
	  <b-menuitem mi="save_alt"     kbd="Ctrl+KeyS"		uri="save"   :disabled="!window.Electron" >	Save Project		</b-menuitem>
	  <b-menuitem fa="save"		kbd="Shift+Ctrl+KeyS"	uri="saveas" :disabled="!window.Electron" >	Save As…		</b-menuitem>
	  <b-menuseparator style="margin: 7px"  />
	  <b-menuitem fa="cog"         kbd="Ctrl+Comma"	uri="prefs">	Preferences		</b-menuitem>
	  <b-menuseparator style="margin: 7px"  />
	  <b-menuitem mi="close" kbd="Shift+Ctrl+KeyQ" uri="quit">	Quit			</b-menuitem>
	</b-contextmenu>
      </push-button>

      <!-- View Menu -->
      <push-button data-tip="**CLICK** View Menu" data-hotkey="Alt+KeyV" @click="Util.dropdown ($refs.viewmenu, $event)" >
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
      <b-positionview :song="song"> </b-positionview>
    </h-flex>

    <!-- menubar right -->
    <b-button-bar class="-menubar" >
      <!-- Help Menu -->
      <push-button data-tip="**CLICK** Help Menu" data-hotkey="Alt+KeyH"
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
    this.$refs.viewmenu.map_kbd_hotkeys (true);
  },
  methods: {
    async activation (uri, event) {
      let u, v;
      switch (uri) {
	case 'quit_discard':
	  window.close();
	  break;
	case 'quit':
	  v = App.async_modal_dialog ("Save Project?",
				      "The current project may contain unsaved changes.\n" +
				      "Save changes to the project before closing?",
				      (window.Electron ?
				       [ 'Discard Changes', { label: 'Cancel', autofocus: true }, 'Save' ] :
				       [ 'Discard Changes', { label: 'Cancel', autofocus: true },
					 { label: 'Save', disabled: true } ]),
				      'QUESTION');
	  v = await v;
	  if (v == 0)
	    return window.close();
	  if (v == 2 && await save_project())
	    return window.close();
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
	case 'loadnew':
	  App.load_project();
	  break;
	case 'load':
	  openfile();
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

let open_dialog_lastdir = undefined;

async function openfile()
{
  if (!window.Electron)
    return;
  const Electron = window.Electron;
  if (!open_dialog_lastdir)
    open_dialog_lastdir = await Ase.server.get_demo_path();
  const opt = {
    title: Util.format_title ('Anklang', 'Select File To Open'),
    buttonLabel: 'Open File',
    defaultPath: open_dialog_lastdir,
    properties: ['openFile', ], // 'multiSelections' 'openDirectory' 'showHiddenFiles'
    filters: [ { name: 'Projects', extensions: ['anklang'] },
	       { name: 'Audio Files', extensions: [ 'anklang', 'mid', 'wav', 'mp3', 'ogg' ] },
	       { name: 'All Files', extensions: [ '*' ] }, ],
  };
  const dresult = await Electron.dialog.showOpenDialog (Electron.getCurrentWindow(), opt);
  if (!dresult.canceled && dresult.filePaths.length == 1)
    {
      const filename = dresult.filePaths[0];
      open_dialog_lastdir = filename.replace (/.*[\/\\]/, ''); // dirname
      const lresult = await App.load_project (filename);
      if (lresult == Ase.Error.NONE)
	{
	  let project_filename = filename;
          Data.project.set_custom ('filename', project_filename);
	}
      else
	console.error ('Failed to load:', filename); // FIXME : warning
    }
}

let save_dialog_lastdir = undefined;

async function save_project (asnew = false)
{
  if (!window.Electron)
    return false;
  const Electron = window.Electron;
  const Path = require ('path');
  if (!save_dialog_lastdir)
    save_dialog_lastdir = Electron.app.getPath ('music') || Path.resolve ('.');
  let savepath = asnew ? '' : await Data.project.get_custom ('filename');
  if (!savepath)
    {
      const opt = {
	title: Util.format_title ('Anklang', 'Save Project'),
	buttonLabel: 'Save As',
	defaultPath: save_dialog_lastdir,
	properties: [], // 'showOverwriteConfirmation'
	filters: [ { name: 'Projects', extensions: ['anklang'] }, ],
      };
      const dresult = await Electron.dialog.showSaveDialog (Electron.getCurrentWindow(), opt);
      if (dresult.canceled || !dresult.filePath)
	return false;
      savepath = dresult.filePath;
    }
  const Fs = require ('fs');
  if (!savepath.endsWith ('.anklang'))
    {
      savepath += '.anklang';
      let replace = 1;
      if (Fs.existsSync (savepath))
	replace = await App.async_modal_dialog ("Replace Project?",
						"Replace existing project file?\n" +
						savepath + ": File exists",
						[ 'Cancel', { label: 'Replace', autofocus: true } ],
						'QUESTION');
      if (replace != 1)
	return false;
    }
  save_dialog_lastdir = Path.dirname (savepath);
  if (Fs.existsSync (savepath))
    Fs.unlinkSync (savepath);
  const err = await App.save_project (savepath);
  if (err == Ase.Error.NONE)
    {
      Data.project.set_custom ('filename', savepath);
      let msg = '# Project Saved\n';
      msg += '  \n  \nProject saved to: ``' + savepath + '``\n';
      Util.show_note (msg);
      return true;
    }
  App.async_modal_dialog ("File IO Error",
			  "Failed to save project.\n" +
			  savepath + ": " +
			  await Ase.server.describe_error (err),
			  [ { label: 'Dismiss', autofocus: true } ],
			  'ERROR');
  // <b-icon fa="warning" style="display: none; font-size: 250%; padding-right: 1em; float: left; color: #d92" />
  return false;
}

</script>
