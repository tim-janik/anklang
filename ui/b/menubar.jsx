// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class MenuBar
 * @description
 * SolidJS component that displays the main menu bar with File, Edit, View, and Help menus.
 */

import { onMount } from 'solid-js';
import * as Util from "../util.js";
import * as Ase from '../../ase/gen/api-jsonipc.g.ts';
import { PositionView } from './positionview';
import { PlayControls } from './playcontrols';
import { basename, dirname, displayfs, displaybasename, displaydirname } from '../strings.js';
import { ButtonBar } from './buttonbar';
import { ContextMenu } from './contextmenu';
import { Icon } from './icon';

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
  const project = Shell.project;
  const project_activate = activate.bind (null, project);
  const project_isactive = isactive.bind (null, project);
  const menus = [];
  // Hotkeys check isactive when pressed, so closed menus keep their items enabled.
  onMount (() => menus.forEach (menu => menu.map_kbd_hotkeys (true)));

  function MenuButton (props)
  {
    let menu;
    return <div class="asbutton button-dim" data-tip={`**CLICK** ${props.label} Menu`} data-hotkey={`Alt+${props.hotkey}`}
      onClick={e => menu.popup (e)} onMouseDown={e => menu.popup (e)} id={`g-${props.id}menu`}>
      <div class="b-menubar-icon"><Icon ic={props.icon} /></div>
      <ContextMenu ref={value => { menu = value; menus.push (value); }}
        activate={project_activate} isactive={project_isactive} items={props.items} />
    </div>;
  }

  return (
    <div class={props.class ? 'hflex b-menubar m-2 ' + props.class : 'hflex b-menubar m-2'} style="justify-content: space-between">
      {/* menubar left */}
      <ButtonBar class="-menubar">
        <MenuButton id="file" label="File" icon="md-folder" hotkey="F" items={[
          { uri: 'loadnew', label: 'New Project', icon: 'fa-file_o', kbd: 'Ctrl+N' },
          { uri: 'load', label: 'Open Project…', icon: 'fa-file_audio_o', kbd: 'Ctrl+O' },
          { uri: 'save', label: 'Save Project', icon: 'fa-save', kbd: 'Ctrl+S' },
          { uri: 'saveas', label: 'Save As…', icon: 'fa-save', kbd: 'Shift+Ctrl+S' },
          { type: 'separator' },
          { uri: 'prefs', label: 'Preferences', icon: 'fa-cog', kbd: 'Ctrl+RawComma' },
          { type: 'separator' },
          { uri: 'quit', label: 'Quit', icon: 'md-close', kbd: 'Shift+Ctrl+Q' },
        ]} />

        <MenuButton id="edit" label="Edit" icon="md-playlist_edit" hotkey="E" items={[
          { uri: 'undo', label: 'Undo', icon: 'md-undo', kbd: 'Ctrl+Z' },
          { uri: 'redo', label: 'Redo', icon: 'md-redo', kbd: 'Shift+Ctrl+Z' },
        ]} />

        <MenuButton id="view" label="View" icon="fa-eye" hotkey="V" items={[
          { uri: 'fullscreen', label: 'Toggle Fullscreen', icon: 'md-fullscreen', kbd: 'F11', disabled: !document.fullscreenEnabled },
          ...electron_menuitems(),
        ]} />
      </ButtonBar>

      {/* playcontrols */}
      <div class="hflex">
        <PlayControls />
        <PositionView />
      </div>

      {/* menubar right */}
      <ButtonBar class="-menubar">
        <MenuButton id="help" label="Help" icon="fa-life_ring" hotkey="H" items={[
          { uri: 'anklang-docu', label: 'Anklang Documentation…', icon: 'fa-book_open' },
          { type: 'separator' },
          { uri: 'about', label: 'About…', icon: 'oct-id_badge' },
        ]} />
      </ButtonBar>
    </div>
  );
};

// == Electron-specific menu items ==
function electron_menuitems()
{
  return window['Electron'] ? [
    { uri: 'zoom-in', label: 'Zoom In', icon: 'cod-zoom_in', kbd: 'Ctrl++' },
    { uri: 'zoom-out', label: 'Zoom Out', icon: 'cod-zoom_out', kbd: 'Ctrl+-' },
    { uri: 'zoom-reset', label: 'Reset Zoom', icon: 'cod-screen_full', kbd: 'Ctrl+0' },
  ] : [];
};

// == Menu activation handlers ==
async function isactive (project, uri)
{
  switch (uri) {
    case 'undo':
      return project.can_undo();
    case 'redo':
      return project.can_redo();
    default:
      return true;
  }
}

async function activate (project, uri, event)
{
  const Electron = window["Electron"] || null;
  let u, v;
  switch (uri) {
    case 'quit_discard':
      return discard_and_close (project);
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
        return discard_and_close (project);
      if (v == 2 && await save_project (project))
        return App.close_window();
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
      await project.undo();
      break;
    case 'redo':
      await project.redo();
      break;
    case 'loadnew':
      if (await App.load_project_checked() === Ase.Error.NONE && Shell.project !== project)
        await project.discard();
      break;
    case 'load':
      open_file();
      break;
    case 'save':
      save_project (project);
      break;
    case 'saveas':
      save_project_as (project);
      break;
  }
}

async function discard_and_close (project)
{
  try {
    if (await project.$refetch (() => project.unnamed))
      await project.remove_project_dir();
  } finally {
    App.close_window();
  }
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
  const project = Shell.project;
  const err = await App.load_project_checked (filename);
  if (err === Ase.Error.NONE && Shell.project !== project)
    await project.discard();
}
let open_file_last_dir = "~MUSIC";

async function save_project (project)
{
  let name = '';
  if (await project.$refetch (() => project.unnamed))
    {
      const input = document.createElement ('input');
      input.type = 'text';
      input.autofocus = true;
      input.setAttribute ('aria-label', _('Project name'));
      input.value = project.name === 'Untitled' ? '' : project.name;
      const result = await Shell.async_modal_dialog ({
        title: _('Name Project'),
        text: _('Project name'),
        buttons: [ _('Cancel'), _('Save') ],
        div_handler: div => div.appendChild (input),
      });
      if (result !== 1 || !input.value.trim())
        return false;
      name = input.value.trim();
    }
  const err = await App.save_project (name, project);
  return show_save_result (project, err, name);
}

async function save_project_as (project)
{
  const path = await Shell.select_file ({
    title:  _('Save Project As'),
    button: _('Save As'),
    cwd:    save_project_last_dir,
    existing: false,
  });
  if (!path)
    return false;
  save_project_last_dir = dirname (path);
  Shell.show_spinner();
  const err = await project.save_project_as (path);
  Shell.hide_spinner();
  return show_save_result (project, err, path);
}
let save_project_last_dir = "~MUSIC";

async function show_save_result (project, err, target)
{
  const dir = await project.$refetch (() => project.project_dir);
  let msg;
  if (err === Ase.Error.NONE) {
    msg = '### Project Saved\n  \n  \n';
    msg += 'Project successfully saved to:\n\n`' + displayfs (dir) + '`\n';
  } else {
    const errblurb = await Ase.server.error_blurb (err);
    msg = '# File IO Error\n  \n  \n';
    msg += 'Failed to save project:\n\n';
    msg += '`' + displayfs (dir || target) + ': ' + errblurb + '`';
  }
  Shell.show_notice (msg);
  return err === Ase.Error.NONE;
}
