// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';
const package_json = require ('./package.json');
Object.defineProperty (globalThis, '__DEV__', { value: package_json.mode !== 'production' });
const Electron = require ('electron');
const Eapp = Electron.app;
const Eshell = Electron.shell;
const os = require ('os');
const ELECTRON_CONFIG = { quitstartup: false };

// CSS Defaults
const defaults = {
  backgroundColor: '#222222',
  defaultFontSize: 12,
  defaultMonospaceFontSize: 13,
};

// Processes
let win, ase_proc;

// End process and main_exit all dependent processes
function main_exit (exitcode, ...errmsgs) {
  if (main_exit.exit_code !== undefined)
    return; // recursion, might be in late destruction where await + setTimeout does *NOT* work anymore
  main_exit.exit_code = 0 | exitcode;
  if (errmsgs.length)
    console.error (...errmsgs);
  if (win)
    {
      win.destroy();
      win = null;
    }
  if (ase_proc)
    ase_proc.kill();
  if (main_exit.exit_code < 0)
    process.abort();
  Eapp.exit (main_exit.exit_code);
}

// Handle Electron application 'quit()' method
Eapp.once ('will-quit', e => {
  /* Note, Electron hijacks SIGINT, SIGHUP, SIGTERM to trigger app.quit()
   * which has a 0 exit status. We simply don't use quit() and force a
   * non-0 status if it's used. See also:
   * https://github.com/electron/electron/issues/5273
   * https://github.com/electron/electron/issues/19650
   * https://github.com/electron/electron/issues/29559
   */
  main_exit (130);	// assume SIGINT
});

// Terminate on Promise rejection
process.on ('unhandledRejection', (reason, promise) => {
  main_exit (-1, 'Unhandled Rejection at:', promise, 'reason:', reason);
});

// Valid exit
Eapp.once ('window-all-closed', () => {
  win = null; // this *may* be emitted before win.closed
  main_exit (0);
});

// == Browser Window ==
// create the main browser window
function create_window (onclose)
{
  const FS = require ('fs');
  // avoid menu flicker, leave menu construction to the window
  Electron.Menu.setApplicationMenu (null);
  // window configuraiton
  const options = {
    width:                              1820, // calling win.maximize() causes flicker
    height:                             1365, // using a big initial size avoids flickering
    backgroundColor:                    defaults.backgroundColor,
    autoHideMenuBar:                    false,
    icon:				__dirname + '/anklang.png',
    webPreferences: {
      preload:				__dirname + '/preload.js',
      sandbox:				true,
      contextIsolation:			true,
      nodeIntegration:                  false,
      enableRemoteModule:               false,
      devTools:                         __DEV__,
      defaultEncoding:                  'UTF-8',
      defaultFontSize:                  parseInt (defaults.defaultFontSize),
      defaultMonospaceFontSize:         parseInt (defaults.defaultMonospaceFontSize),
      defaultFontFamily: {
	standard:       'sans',         // 'Times New Roman',
	serif:          'Constantia',   // 'Times New Roman',
	sansSerif:      'Candara',      // 'Arial',
	monospace:      'Consolas',     // 'Courier New',
	cursive:        'Script',       // 'Script',
	fantasy:        'Impact',       // 'Impact',
      },
    },
    show: false, // avoid incremental load effect, see 'ready-to-show'
    darkTheme: true,
  };
  const w = new Electron.BrowserWindow (options);
  w.setMenu (Electron.Menu.buildFromTemplate ([]));
  w.webContents.once ('crashed', () => main_exit (129)); // crashed is SIGHUP or SIGTERM
  if (onclose)
    w.on ('closed', onclose);
  return w;
}

// asynchronously load URL into window `w`
async function load_and_show (w, winurl) {
  w = await w;
  const win_ready = new Promise (resolve => w.once ('ready-to-show', () => resolve ()));
  const origin = winurl.replace (/(\w\/).*/g, '$1');
  // allow reloads, disallow navigation
  w.webContents.addListener ('will-navigate', (ev, navurl) => {
    if (navurl !== winurl)              // not a reload
      ev.preventDefault();
  });
  // handle subwindow creation (via target=_blank or window.open)
  w.webContents.setWindowOpenHandler (({ url }) => {                    // Electron-12
    if (url.startsWith (origin))        // Anklang content
      ; // return { action: 'allow' };
    Eshell.openExternal (url);          // use xdg-open or similar
    return { action: 'deny' };
  });
  // customize child windows
  w.webContents.on ('did-create-window', (childwin) => {                // Electron-12
    childwin.webContents.on ('will-navigate', (ev, navurl) => {
      console.log ('SUBWINDOW: will-navigate', navurl);
    })
  });
  // load URL, show *afterwards*
  await w.loadURL (winurl);
  await win_ready;
  // w.toggleDevTools(); // start with DevTools enabled
  return w.show();
}

// == Sound Engine ==
function start_sound_engine (config, datacb)
{
  const { spawn, spawnSync } = require ('child_process');
  const sound_engine = __dirname + '/../../../lib/AnklangSynthEngine-' + package_json.version;
  const args = [ '--embed', '3' ];
  if (config.verbose)
    args.push ('--verbose');
  if (config.binary)
    args.push ('--binary');
  if (config.norc)
    args.push ('--norc');
  const subproc = spawn (sound_engine, args, { stdio: [ 'pipe', 'inherit', 'inherit', 'pipe' ] });
  if (config.gdb)
    {
      console.log ('DEBUGGING:\n  gdb --pid', subproc.pid, '#', sound_engine);
      spawnSync ('/usr/bin/sleep', [ 5 ]);
    }
  subproc.stdio[3].once ('data', (bytes) => datacb (bytes.toString()));
  // handle errors and exit
  const prefix = sound_engine.split ('/').pop() + ':';
  const slog = (...args) => console.error (prefix, ...args);
  subproc.on ('exit', async (code, sig) => {
    // NOTE: await + setTimeout might *NOT* work at this point
    const reason = sig || subproc.signalCode || code || subproc.exitCode;
    if (reason)
      main_exit (143, prefix, 'exit:', reason);
    else
      main_exit (0);
  });
  subproc.stdio[3].once ('end', async (x) => {
    setTimeout (() => { // fallback, other handlers should exit earlier
      console.error (prefix, 'connection closed');
      main_exit (-1);
    }, 444);
  });
  subproc.stdio[3].once ('error', (err) => {
    main_exit (-1, prefix, 'io-error:', err);
  });
  subproc.once ('error', (err) => {
    main_exit (-1, prefix, 'error:', err);
  });
  return subproc;
}

// == IPC Messages ==
// IPC calls available to the Renderer
const ipc_handler = {
  electron_versions (browserwindow, ...args)   { return { platform: process.platform,
							  config: ELECTRON_CONFIG,
	                                                  arch: process.arch,
                                                          os_release: os.release(),
	                                                  versions: process.versions, }; },
  toggle_dev_tools (browserwindow, ...args)	{ browserwindow.toggleDevTools(); },
  exit (browserwindow, status)			{ Electron.app.exit (0 | status); },
};
// Dispatch Renderer->Main message events
for (const func in ipc_handler)
  Electron.ipcMain.handle (func, async (event, args) => await ipc_handler[func] (event.sender, ...args));

// == Usage ==
let cmdname = Eapp.getName();
function usage (what, exitcode = false) {
  const name = cmdname.replace (/.*\//, '');
  if (what === 'version')
    console.log (name + ' ' + package_json.version); // Eapp.getVersion();
  if (what === 'usage' || what === 'help')
    console.log (`Usage: ${name} [OPTIONS] [--] [ProjectFiles...]`);
  let o = [
    'Options:',
    '--version         Print program version',
    '-h, --help        Print command line option help',
    '-v, --verbose     Print runtime information',
    '--binary          Print binary IPC messages',
    '--gdb             Print command to debug sound engine',
    '--norc            Skip reading of anklangrc',
    '-U <ASEURL>       Run with URL of external sound engine',
  ];
  const d = [
    '--inspect         Open nodejs debugging port',
  ];
  if (__DEV__)
    o = o.concat (d);
  if (what === 'help')
    console.log (o.join ('\n  '));
  if (exitcode !== false)
    main_exit (exitcode);
}

// == Arguments ==
function parse_args (argv)
{
  argv = argv.slice (1); // take [1,...]
  const c = { files: [], verbose: false, binary: false, gdb: false, norc: false, };
  const sep = false;
  while (argv.length)
    {
      const arg = argv.splice (0, 1)[0];
      if (sep)
	{
	  c.files.push (arg);
	  continue;
	}
      switch (arg) {
	case '--help': case '-h':
	  usage ('help', 0);
	case '--version':
	  usage ('version', 0);
	case '--verbose': case '-v':
	  c.verbose = true;
	  break;
	case '--quitstartup':
	  ELECTRON_CONFIG.quitstartup = true;
	  break;
	case '--binary':
	  c.binary = true;
	  break;
	case '--gdb':
	  c.gdb = true;
	  break;
	case '--norc':
	  c.norc = true;
	  break;
	case '-U':
	  c.xurl = argv.splice (0, 1)[0];
	  break;
	case '--inspect':	// nodejs debugging option
	  break;		// open chrome://inspect/
	case '--':
	  sep = true;
	  break;
	default:
	  c.files.push (arg);
	  break;
      }
    }
  return c;
}

// == Start Components ==
// Create SoundEngine and BrowserWindow once everything is loaded
async function startup_components (config) {
  // start rendering process
  const onclose = () => win = null;
  win = create_window (onclose);
  // start sound engine
  let winurl = config.xurl; // external URL?
  if (!winurl)
    {
      const sndmsg = new Promise (resolve => ase_proc = start_sound_engine (config, msg => resolve (msg)));
      // retrieve sound engine URL
      const auth = JSON.parse (await sndmsg); // yields { "url": "http://127.0.0.1:<PORT>/?subprotocol=<STRING>" }
      if (!auth.url)
	main_exit (-1, 'Failed to launch sound engine');
      winurl = auth.url;
    }
  // load URL in renderer
  await load_and_show (win, winurl);
}

// == Run ==
cmdname = process.argv[0];
const config = parse_args (process.argv);
Eapp.once ('ready', _ => startup_components (config));
