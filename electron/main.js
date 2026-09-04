// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

// == Variables ==
let browser_window;

// == Helpers ==
// Wrap fn() to return [result, error], handle both sync throws and async rejections.
const trycatch = fn => {
  try { const pr = fn();
    if (pr && typeof pr.then === 'function')
      return pr.then (v => [v, null], ex => [undefined, ex]);
    else return [pr, null];
  } catch (ex) { return [undefined, ex]; }
};
// Attempt to execute fn(), silently catch errors or async rejections and return fallback.
const tryelse = (ev, fn) => {
  try { const pr = (fn || ev)();
    if (pr && typeof pr.then === 'function')
      return pr.catch (() => ev);
    else return pr;
  } catch (ex) { return ev; }
};
// Avoid any hangs, install handlers *before* imports
process.on ('unhandledRejection', (reason, promise) => main_exit (-1, 'Unhandled Rejection:', promise, 'reason:', reason));
// unhandledRejection: new Promise ((resolve, reject) => reject (new Error ('Testing unhandledRejection...')));
process.on ('uncaughtException', (error, s, d) => main_exit (-1, 'Uncaught Exception:', error, error.stack));
// uncaughtException: throw new Error ('Testing uncaughtException');

// == Imports & Globals ==
const package_json = require ('./package.json');
Object.defineProperty (globalThis, '__DEV__', { value: package_json.__DEV__ });
const Electron = require ('electron');
const Eapp = Electron.app;
const Esession = Electron.session;
const os = require ('os');
const fs = require ('fs');
const path = require ('path');

// == Config & Defaults ==
const ELECTRON_CONFIG = { quitstartup: false, };
const cli_args = [];
let devtools_option = false;
let headless_mode = false;
let console_stdout_fd = null;
let console_stderr_fd = null;
Eapp.commandLine.appendSwitch ('disk-cache-size', '0');
Eapp.commandLine.appendSwitch ('disable-http-cache'); // disk cache for HTTP

// == Caches ==
const xdg_cache_dir = process.env.XDG_CACHE_HOME || path.join (os.homedir(), '.cache');
const anklang_cache_root = path.join (xdg_cache_dir, 'anklang'); // path.join (os.tmpdir(), 'anklang-' + os.userInfo().uid);
const cache_dir = path.join (anklang_cache_root, "" + process.pid);
// Move cache out of ~/.config and clean up regularly
Eapp.setPath ('appData', cache_dir);
Eapp.setAppLogsPath (path.join (cache_dir, 'logs'));
Eapp.setPath ('userData', path.join (cache_dir, 'userData'));
Eapp.setPath ('userCache', path.join (cache_dir, 'userCache'));
Eapp.setPath ('crashDumps', path.join (cache_dir, 'crashDumps'));
// Clean up caches under ~/.cache/anklang/<PID> regularly
async function cleanup_cache_dirs()
{
  for (const entry of await tryelse ([], () => fs.promises.readdir (anklang_cache_root))) {
    const pid = parseInt (entry, 10);
    if (isNaN (pid)) continue;
    const pidpath = path.join (anklang_cache_root, entry);
    const stat = await tryelse (null, () => fs.promises.stat (pidpath));
    if (!stat?.isDirectory()) continue;
    if ("NoPID" == await tryelse ("NoPID", () => process.kill (pid, 0)))
      await trycatch (() => fs.promises.rm (pidpath, { recursive: true, force: true }));
  }
}
setTimeout (cleanup_cache_dirs, 3779);

// CSS Defaults
const defaults = {
  backgroundColor: '#222222',
  defaultFontSize: 12,
  defaultMonospaceFontSize: 13,
};

// == Exit Handling ==
// End process and main_exit all dependent processes
function main_exit (exitcode, ...errmsgs)
{
  if (main_exit.exit_code !== undefined)
    return; // recursion, might be in late destruction where await + setTimeout does *NOT* work anymore
  main_exit.exit_code = 0 | exitcode;
  if (errmsgs.length)
    console.error (...errmsgs);
  if (browser_window)
    {
      browser_window.destroy(); // might re-enter main_exit()
      browser_window = null;
    }
  // remove excessive electron caches
  tryelse (() => fs.rmSync (cache_dir, { recursive: true, force: true }));
  if (main_exit.exit_code < 0)
    process.abort();
  Eapp.exit (main_exit.exit_code); // calls process.exit()
}
Eapp.once ('will-quit', e => {
  /* Handle Electron application 'quit()' method.
   *
   * Note, Electron hijacks SIGINT, SIGHUP, SIGTERM to trigger app.quit()
   * which has a 0 exit status. We simply don't use quit() and force a
   * non-0 status if it's used. See also:
   * https://github.com/electron/electron/issues/5273
   * https://github.com/electron/electron/issues/19650
   * https://github.com/electron/electron/issues/29559
   */
  main_exit (130);	// assume SIGINT
});

Eapp.once ('window-all-closed', () => {
  // Valid exit
  browser_window = null; // this *may* be emitted before browser_window.closed
  main_exit (0);
});
// == URL Policy ==
/// Check if URL is local and allowed
function allowed_url (url)
{
  const pattern =
    `^file:///` +
    `|` +
    `^(http|ws)s?://localhost` + `(:[1-9][0-9][0-9][0-9]+)?` + `/`;
  const regex = new RegExp (pattern, 'i');
  if (url.match (regex))
    return true;
  return false;
}

// == Browser Window ==
// create the main browser window
function create_window (onclose)
{
  // window configuraiton
  const ideal_width = 2048, ideal_ratio = 16 / 10;
  const xalign = 0.5, yalign = 0;
  const options = {
    width:                              ideal_width,
    height:                             Math.round (ideal_width / ideal_ratio),
    backgroundColor:                    defaults.backgroundColor,
    autoHideMenuBar:                    false,
    icon:				__dirname + '/htmlgui.svg',
    webPreferences: {
      preload:				__dirname + '/preload.js',
      sandbox:				true,
      contextIsolation:			true,
      nodeIntegration:                  false,
      nodeIntegrationInWorker:		false,
      enableRemoteModule:               false,
      devTools:                         __DEV__ || devtools_option,
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
      offscreen: headless_mode,		// Force frame generation without a GUI window
    },
    show: false, // avoid incremental load effect, see 'ready-to-show'
    darkTheme: true,
  };
  // align to screen
  const primary_display = Electron.screen.getPrimaryDisplay();
  const area = primary_display?.workArea;
  if (area) {
    options.width = Math.min (options.width, area.width);
    options.height = Math.min (options.height, area.height);
    options.x = Math.round (area.x + xalign * (area.width - options.width));
    options.y = Math.round (area.y + yalign * (area.height - options.height));
    // console.log ("screen:", primary_display);
  }
  // avoid menu flicker, leave menu construction to the window
  Electron.Menu.setApplicationMenu (null);
  const w = new Electron.BrowserWindow (options);
  if (headless_mode)
    w.webContents.setFrameRate (25);
  w.setMenu (Electron.Menu.buildFromTemplate ([]));
  w.webContents.once ('crashed', () => main_exit (129)); // 'crashed' is SIGHUP or SIGTERM
  w.webContents.on ('console-message', (event) => {
    // https://www.electronjs.org/docs/latest/api/web-contents
    const { resolve_source_location, resolve_stack_frames } = require ('./sourcemapping');
    const source_id = event.sourceId;
    let message = event.message;
    const line_number = event.lineNumber;
    let prefix = `${source_id}:${line_number}: `;

    // Try to resolve to original source location via sourcemap
    const resolved = resolve_source_location (source_id, line_number);
    if (resolved) {
      const pathname = 1 ? resolved.source : path.basename (resolved.source);
      if (resolved.column > 0)
	prefix = `${pathname}:${resolved.line}:${resolved.column}: `;
      else
	prefix = `${pathname}:${resolved.line}: `;
      message = resolve_stack_frames (message);
    }

    switch (event._level) {
    case 0: // VERBOSE
    case 1: // INFO
    default:
      if (console_stdout_fd !== null) {
	fs.writeSync (console_stdout_fd, `${prefix}${message}\n`);
      } else {
	process.stdout.write (`${prefix}${message}\n`);
      }
      break;
    case 2: // WARNING
    case 3: // ERROR
      if (console_stderr_fd !== null) {
	fs.writeSync (console_stderr_fd, `${prefix}${message}\n`);
      } else {
	process.stderr.write (`${prefix}${message}\n`);
      }
      break;
    }
  });
  if (onclose)
    w.on ('closed', onclose);
  return w;
}

// asynchronously load URL into window `w`
async function load_and_show (w, winurl)
{
  const win_ready = new Promise (resolve => w.once ('ready-to-show', () => resolve ()));
  // allow reloads, disallow navigation
  w.webContents.addListener ('will-navigate', (ev, navurl) => {
    if (!allowed_url (navurl)) {
      console.warn (`BLOCKING Navigation Attempt: ${winurl} -> ${navurl}`);
      ev.preventDefault();
    }
  });
  // handle subwindow creation (via target=_blank or window.open)
  w.webContents.setWindowOpenHandler (({ url }) => {                    // Electron-12
    if (allowed_url (url))
      return { action: 'allow' };
    Electron.shell.openExternal (url);          // use xdg-open or similar
    return { action: 'deny' };
  });
  // customize child windows
  w.webContents.on ('did-create-window', (childwin) => {                // Electron-12
    childwin.webContents.on ('will-navigate', (ev, navurl) => {
      console.log ('SUBWINDOW: will-navigate', navurl);
    });
  });
  // load URL, show *afterwards*
  await w.loadURL (winurl);
  await win_ready;
  // reset zoom: w.webContents.zoomFactor = 1;
  if (devtools_option)
    w.toggleDevTools(); // start with DevTools enabled
  if (!headless_mode)
    w.show();
  return w;
}

// == IPC Messages ==
// IPC calls available to the Renderer
const ipc_handler = {
  electron_versions (browserwindow, ...args)
  {
    return { platform: process.platform,
	     config: ELECTRON_CONFIG,
	     arch: process.arch,
             os_release: os.release(),
	     versions: process.versions, };
  },
  toggle_dev_tools (browserwindow, ...args)
  {
    browserwindow.toggleDevTools();
  },
  exit (browserwindow, status)
  {
    main_exit (0 | status);
  },
  zoom_level (browserwindow, newval)
  {
    if (newval >= -9 && newval <= +9)
      browserwindow.setZoomLevel (newval);
    return browserwindow.getZoomLevel();
  },
  async screenshot (_browserwindow, seq)
  {
    seq = 0 | seq;
    if (seq < 1 || seq > 99)
      return;
    if (!browser_window)
      return;
    const filepath = '/tmp/Anklang-screenshot-' + String (seq).padStart (2, '0') + '.png';
    const image = await browser_window.webContents.capturePage();
    fs.writeFileSync (filepath, image.toPNG());
    console.log ('Screenshot saved: ' + filepath);
  },
};
// Dispatch Renderer->Main message events
for (const func in ipc_handler)
  Electron.ipcMain.handle (func, async (event, args) => await ipc_handler[func] (event.sender, ...args));

// == Usage ==
function usage (what, exitcode = false)
{
  const name = Eapp.getName();
  if (what === 'version')
    console.log (name + ' ' + Eapp.getVersion());
  if (what === 'usage' || what === 'help')
    console.log (`Usage: ${name} [OPTIONS] [--] <WEBUI-URL>`);
  let o = [
    'Options:',
    '--version         Print program version',
    '-h, --help        Print command line option help',
    '-v, --verbose     Print runtime information',
    '--dev             Start with DevTools (for developers)',
  ];
  if (what === 'help')
    console.log (o.join ('\n  '));
  if (exitcode !== false)
    main_exit (exitcode);
}

// == Arguments ==
function parse_args (argv)
{
  argv = argv.slice (1); // take [1,...]
  const c = { verbose: false, args: [] };
  let sep = false;
  while (argv.length)
    {
      const arg = argv.splice (0, 1)[0];
      if (sep) {
	cli_args.push (arg);
	continue;
      }
      // Split at = to normalize --opt=value to --opt value
      const eq_index = arg.indexOf ('=');
      let value = undefined;
      if (eq_index >= 0)
	value = arg.slice (eq_index + 1);
      switch (eq_index >= 0 ? arg.slice (0, eq_index) : arg) {
	case '--help': case '-h':
	  usage ('help', 0);
	  break;
	case '--version':
	  usage ('version', 0);
	  break;
	case '--verbose': case '-v':
	  c.verbose = true;
	  break;
	case '--console-logs':
	  if (eq_index < 0 && argv.length)
	    value = argv.shift ();
	  if (value !== undefined) {
	    const fds = value.split (',');
	    if (fds.length === 2) {
	      console_stdout_fd = parseInt (fds[0], 10);
	      console_stderr_fd = parseInt (fds[1], 10);
	    }
	  }
	  break;
	case '--dev':
	  devtools_option = true;
	  break;
	case '--headless':
	  headless_mode = true;
	  break;
	case '--quitstartup':
	  ELECTRON_CONFIG.quitstartup = true;
	  break;
	case '--no-sandbox':	// fall-through
	case '--inspect':	// open chrome://inspect/
	  break;		// nodejs debugging options
	case '--':
	  sep = true;
	  break;
	default:
	  cli_args.push (arg);
	  break;
      }
    }
  return c;
}

// == Start Components ==
// Create SoundEngine and BrowserWindow once everything is loaded
async function startup_components (config)
{
  Esession.defaultSession.clearCache();
  // start rendering process
  const onclose = () => browser_window = null;
  browser_window = create_window (onclose);
  // start sound engine
  let winurl = cli_args && cli_args[0];
  if (!winurl || !allowed_url (winurl))
    main_exit (-1, Eapp.getName() + ': Missing valid URL for GUI');
  // load URL in renderer
  await load_and_show (browser_window, winurl);
}

// == Run ==
const config = parse_args (process.argv);
// Headless mode runs without a visible window; the GPU process still probes the
// DRI3/DRM hardware path via the X display though. When that access is denied
// (e.g. container without /dev/dri: 'DRM_IOCTL_MODE_CREATE_DUMB failed: Permission
// denied'), its hardware->software fallback races and can wedge, so window teardown
// after window.close() never completes, 'window-all-closed' never fires, and the
// process hangs. Force software rendering for headless mode to avoid the DRI
// startup path entirely.
if (headless_mode)
  Eapp.commandLine.appendSwitch ('disable-gpu');
Eapp.once ('ready', () => startup_components (config));
