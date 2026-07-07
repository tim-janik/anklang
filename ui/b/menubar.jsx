// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class B-MenuBar
 * SolidJS component that displays the main menu bar with File, Edit, View, and Help menus.
 *
 * ### Props:
 * *project*
 * : The Ase.Project instance to monitor for dirty state changes.
 */

import { createSignal, createEffect, onMount, onCleanup } from 'solid-js';
import * as Util from "../util.js";
import * as Ase from '../../ase/gen/api-jsonipc.g.ts';
import { PositionView } from './positionview';
import { basename, dirname, displayfs, displaybasename, displaydirname } from '../strings.js';

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
.b-menubar {
  .asbutton {
    @apply button-down-within size-10;
    position: relative;
  }
}
.b-menubar-icon {
  position: absolute; inset: 0;
  display: inline-flex; justify-content: center;
  [ic] { inset: 0; position: absolute; vertical-align: middle; }

  &::after {
    content: ''; 		/* force rendering */
    position: absolute; width: 0; height: 0;
    right: 0px;			/* Position at the right */
    bottom: 0px;		/* Position at the bottom */
    border-left: 6px solid transparent;
    border-right: 6px solid transparent;
    border-top: 6px solid white; /* Create a downward-pointing arrow */
    border-bottom: 0;
    transform-origin: 100% 100%;
    transform: rotate(-45deg) translate(6px, -2px); /* Rotate to point into corner */
  }
}`;

// == Component ==
export function MenuBar (props)
{
  // refs for contextmenu elements
  let filemenu, editmenu, viewmenu, helpmenu;
  // track whether keyboard hotkeys have been mapped (once per mount)
  let kbd_mapped = false;
  // cleanup function for project notification
  let project_cleanup = null;

  onMount (() => {
    // subscribe to project dirty notifications
    project_cleanup = Shell.project.on ("notify:dirty", () => {
      check_isactive();
    });
  });

  onCleanup (() => {
    project_cleanup?.();
    project_cleanup = null;
    kbd_mapped = false;
  });

  // after first render, map keyboard hotkeys and check menu state
  createEffect (() => {
    if (!kbd_mapped && filemenu) {
      filemenu.map_kbd_hotkeys (true);
      editmenu.map_kbd_hotkeys (true);
      viewmenu.map_kbd_hotkeys (true);
      helpmenu.map_kbd_hotkeys (true);
      kbd_mapped = true;
    }
    check_isactive();
  });

  const check_isactive = () => {
    filemenu?.check_isactive();
    editmenu?.check_isactive();
    viewmenu?.check_isactive();
    helpmenu?.check_isactive();
  };

  return (
    <div class={props.class ? 'hflex b-menubar m-2 ' + props.class : 'hflex b-menubar m-2'} style="justify-content: space-between">
      {/* menubar left */}
      <b-buttonbar class="-menubar">
        {/* File Menu */}
        <div class="asbutton button-dim" data-tip="**CLICK** File Menu" data-hotkey="Alt+F"
             onClick={e => filemenu.popup (e)}
             onMouseDown={e => filemenu.popup (e)}
             id="g-filemenu">
          <div class="b-menubar-icon">
            <b-icon ic="md-folder"></b-icon>
          </div>
          <b-contextmenu ref={filemenu} activate={activate} isactive={isactive}>
            <button ic="fa-file_o"       kbd="Ctrl+N"         uri="loadnew">  New Project       </button>
            <button ic="fa-file_audio_o"  kbd="Ctrl+O"         uri="load">    Open Project…       </button>
            <button ic="fa-save"          kbd="Ctrl+S"         uri="save">    Save Project        </button>
            <button ic="fa-save"           kbd="Shift+Ctrl+S"   uri="saveas">  Save As…            </button>
            <b-menuseparator></b-menuseparator>
            <button ic="fa-cog"            kbd="Ctrl+RawComma"  uri="prefs">   Preferences         </button>
            <b-menuseparator></b-menuseparator>
            <button ic="md-close"          kbd="Shift+Ctrl+Q"   uri="quit">    Quit                </button>
          </b-contextmenu>
        </div>

        {/* Edit Menu */}
        <div class="asbutton button-dim" data-tip="**CLICK** Edit Menu" data-hotkey="Alt+E"
             onClick={e => editmenu.popup (e)}
             onMouseDown={e => editmenu.popup (e)}
             id="g-editmenu">
          <div class="b-menubar-icon">
            <b-icon ic="md-playlist_edit"></b-icon>
          </div>
          <b-contextmenu ref={editmenu} activate={activate} isactive={isactive}>
            <button ic="md-undo" kbd="Ctrl+Z"        uri="undo">  Undo  </button>
            <button ic="md-redo" kbd="Shift+Ctrl+Z"  uri="redo">  Redo  </button>
          </b-contextmenu>
        </div>

        {/* View Menu */}
        <div class="asbutton button-dim" data-tip="**CLICK** View Menu" data-hotkey="Alt+V"
             onClick={e => viewmenu.popup (e)}
             onMouseDown={e => viewmenu.popup (e)}
             id="g-viewmenu">
          <div class="b-menubar-icon">
            <b-icon ic="fa-eye"></b-icon>
          </div>
          <b-contextmenu ref={viewmenu} activate={activate} isactive={isactive}>
            <button ic="md-fullscreen" disabled={!document.fullscreenEnabled}
                    kbd="F11" uri="fullscreen">  Toggle Fullscreen  </button>
            {electron_menuitems()}
          </b-contextmenu>
        </div>
      </b-buttonbar>

      {/* playcontrols */}
      <div class="hflex">
        <b-playcontrols></b-playcontrols>
        <PositionView />
      </div>

      {/* menubar right */}
      <b-buttonbar class="-menubar">
        {/* Help Menu */}
        <div class="asbutton button-dim" data-tip="**CLICK** Help Menu" data-hotkey="Alt+H"
             onClick={e => helpmenu.popup (e)}
             onMouseDown={e => helpmenu.popup (e)}
             id="g-helpmenu">
          <div class="b-menubar-icon">
            <b-icon ic="fa-life_ring"></b-icon>
          </div>
          <b-contextmenu ref={helpmenu} activate={activate} isactive={isactive}>
            <button ic="fa-book_open"   uri="anklang-docu">  Anklang Documentation…  </button>
            <b-menuseparator></b-menuseparator>
            <button ic="oct-id_badge"    uri="about">         About…                  </button>
          </b-contextmenu>
        </div>
      </b-buttonbar>
    </div>
  );
};

// == Electron-specific menu items ==
function electron_menuitems()
{
  if (!window['Electron'])
    return null;
  return (
    <>
      <button ic="cod-zoom_in"   kbd="Ctrl++"   uri="zoom-in">    Zoom In       </button>
      <button ic="cod-zoom_out"  kbd="Ctrl+-"   uri="zoom-out">   Zoom Out      </button>
      <button ic="cod-screen_full" kbd="Ctrl+0"  uri="zoom-reset"> Reset Zoom    </button>
    </>
  );
};

// == Menu activation handlers ==
async function isactive (uri)
{
  switch (uri) {
    case 'undo':
      return Shell.project.can_undo();
    case 'redo':
      return Shell.project.can_redo();
    default:
      return true;
  }
}

async function activate (uri, event)
{
  const Electron = window["Electron"] || null;
  let u, v;
  switch (uri) {
    case 'quit_discard':
      window.close();
      break;
    case 'quit':
      v = App.async_button_dialog ("Save Project?",
                                   "The current project may contain unsaved changes.\n" +
                                   "Save changes to the project before closing?",
                                   (Electron ?
                                    [ 'Discard Changes', { label: 'Cancel', autofocus: true }, 'Save' ] :
                                    [ 'Discard Changes', { label: 'Cancel', autofocus: true },
                                      { label: 'Save', disabled: false } ]),
                                   'QUESTION');
      v = await v;
      if (v == 0)
        return window_close();
      if (v == 2 && await save_project())
        return window_close();
      break;
    case 'about':
      Shell.show_about_dialog (true);
      break;
    case 'anklang-docu':
      u = location.origin + '/anklang/index.html';
      window.open (u, '_blank');
      break;
    case 'prefs':
      Shell.r.show_preferences_dialog = !Shell.r.show_preferences_dialog;
      break;
    case 'zoom-reset':
      await Electron.call ('zoom_level', 0.0);
      break;
    case 'zoom-in':
      await Electron.call ('zoom_level', (await Electron.call ('zoom_level') + 1));
      break;
    case 'zoom-out':
      await Electron.call ('zoom_level', (await Electron.call ('zoom_level')) - 1);
      break;
    case 'fullscreen':
      if (document.fullscreen)
        document.exitFullscreen();
      else
        document.body.requestFullscreen();
      break;
    case 'undo':
      await Shell.project.undo();
      break;
    case 'redo':
      await Shell.project.redo();
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
}

function window_close()
{
  window.close();
  // when we're running in the browser, window.close() might not work, so...
  setTimeout (() => {
    window.location.href = 'about:blank';
  }, 0);
}

async function open_file() {
  const opt = {
    title:  _('Open Project'),
    button: _('Open File'),
    cwd:    open_file_last_dir,
    filters: [ { name: 'Projects', extensions: ['anklang'] }, // TODO: filters
               { name: 'Audio Files', extensions: [ 'anklang', 'mid', 'wav', 'mp3', 'ogg' ] },
               { name: 'All Files', extensions: [ '*' ] }, ],
  };
  const filename = await Shell.select_file (opt);
  if (!filename)
    return;
  open_file_last_dir = dirname (filename);
  const err = await App.load_project_checked (filename);
  if (err != Ase.Error.NONE) {
    // load_project_checked() displays a dialog
  }
}
let open_file_last_dir = "~MUSIC";

async function save_project (asnew = false) {
  const opt = {
    title:  _('Save Project'),
    button: _('Save As'),
    cwd:    save_project_last_dir || "~MUSIC",
    existing: false,
    filters: [ { name: 'Projects', extensions: ['anklang'] }, ],
  };
  let filename = await Shell.project.saved_filename();
  let replace = asnew ? 0 : !!filename;
  if (asnew || !filename)
    filename = await Shell.select_file (opt);
  if (!filename)
    return false;
  save_project_last_dir = dirname (filename);
  if (0 && !replace) // TODO: if file_exists (filename)
    {
      replace = await App.async_button_dialog ("Replace Project?",
                                               "Replace existing project file?\n" +
                                               displayfs (filename) + ": File exists", [
                                                 'Cancel',
                                                 { label: 'Replace', autofocus: true }, ],
                                               'QUESTION');
      if (replace != 1)
        return false;
    }
  let msg, err = await App.save_project (filename);
  if (err === Ase.Error.NONE) {
    filename = await Shell.project.saved_filename(); // get canonicalized form
    msg = '### Project Saved\n  \n  \n';
    msg += 'Project successfully saved to:\n\n`' + displayfs (filename) + '`\n';
  } else {
    let errblurb = Ase.server.error_blurb (err);
    msg = '# File IO Error\n  \n  \n';
    msg += 'Failed to save project:\n\n';
    msg += '`' + displayfs (filename) + ": " + await errblurb + '`';
  }
  Shell.show_notice (msg);
  return err === Ase.Error.NONE;
}
let save_project_last_dir = "~MUSIC";
