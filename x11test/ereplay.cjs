// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

const { BrowserWindow, app } = require ("electron");
const pie = require ("puppeteer-in-electron");
const puppeteer = require ("puppeteer-core");
const { createRunner, parse, PuppeteerRunnerExtension } = require ('@puppeteer/replay');

// Maximum timeout per step
const TIMEOUT = 2500;
// Delay per step, too fast replay interferes with animations
const DELAY = 101;

// Abort on any error, this is a test runner
function test_error (arg) {
  console.error ("Aborting...", arg);
  app.exit (-1);
}
process.on ('uncaughtException', test_error);
process.on ('unhandledRejection', test_error);

// Limited CLI parsing, figure replay.json file argument
let jsonfile;
for (const arg of process.argv)
  if (arg.endsWith ('.json'))
    jsonfile = arg;
if (!jsonfile)
  throw new Error ('missing JSON replay file');

// Single BrowserWindow setup
const window_options = {
  width:                              1920, // calling win.maximize() causes flicker
  height:                             1080, // using a big initial size avoids flickering
  backgroundColor:                    '#000',
  autoHideMenuBar:                    true,
  webPreferences: {
    sandbox:                          true,
    contextIsolation:                 true,
    nodeIntegration:                  false,
    enableRemoteModule:               false,
    devTools:                         false, // true,
    defaultEncoding:                  'UTF-8',
  },
  // darkTheme: true,
};

// Helpers
const delay = ms => new Promise (resolve => setTimeout (resolve, ms));

// == TestRunnerExtension ==
class TestRunnerExtension extends PuppeteerRunnerExtension {
  constructor (browser, page, opts, scriptname)
  {
    super (browser, page, opts);
    this.REPLAY = (scriptname ? scriptname + ': ' : '') + 'REPLAY:';
  }
  log (...args)
  {
    console.log (this.REPLAY, ...args);
  }
  async beforeAllSteps (flow)
  {
    await super.beforeAllSteps (flow);
    this.log ('START...');
  }
  async beforeEachStep (step, flow)
  {
    if (click_types.indexOf (step.type) >= 0 &&
	!step.duration)
      step.duration = 99;	// allow clicks to have a UI effect
    // chain super
    await super.beforeEachStep (step, flow);
    const s = step.selectors ? step.selectors.flat() : [];
    let msg = step.type;
    if (step.url)
      msg += ': ' + step.url;
    else if (s.length)
      msg += ': ' + s[0];
    else
      msg += '...';
    this.log (msg);
    // Give time to let clickable elements emerge
    if (click_types.indexOf (step.type) >= 0)
      await delay (DELAY);
  }
  async afterEachStep (step, flow)
  {
    await super.afterEachStep (step, flow);
    // Give time to let clicks take effect
    if (click_types.indexOf (step.type) >= 0) {
      await delay (2 * DELAY);
      /* Workaround: Add post-click timeout while Anklang uses intermittent reloads on project changes.
       * Atm, with Anklang based on Vue, once a new project is loaded or created, Anklang does a full UI reload
       * If the afterEachStep-delay for button clicks is too short (e.g. 0.2 seconds) for a full reload, consecutive
       * events may get lost or fail to find their UI components while the UI is still rebuilding.
       * For now, the only remedy we have is to add an extra delay that'd usually only be needed after
       * a 'navigate' step.
       */
      await delay (1 * 333); // TODO: remove once "New Project" does not cause a reload.
    } else if ('navigate' === step.type) {
      const secs = 0.5;
      await delay (secs * 1000);
      this.log ("continuing after 'navigate' grace period of" + secs + " seconds");
    }
  }
  async afterAllSteps (flow)
  {
    await super.afterAllSteps(flow);
    this.log ('DONE.\n');
    // Give time to inspect/view final state
    await delay (TIMEOUT);
  }
}
const click_types = [ 'click', 'doubleClick' ];

// == main ==
async function
main()
{
  await pie.initialize (app);				// needed before app.isReady()
  const browser = await pie.connect (app, puppeteer);	// awaits app.whenReady
  const window = new BrowserWindow (window_options);	// callable after app.whenReady

  // Parse replay JSON
  const fs = require ('fs');
  const flow_json = fs.readFileSync (jsonfile, 'utf8');
  const flow = parse (JSON.parse (flow_json));

  // setup window, tester page and runner
  const page = await pie.getPage (browser, window);
  page.setDefaultTimeout (TIMEOUT);
  const runner_extension = new TestRunnerExtension (browser, page, { timeout: TIMEOUT }, jsonfile);
  const runner = await createRunner (flow, runner_extension);

  // start running steps
  runner_extension.log ("ereplay.cjs: runner.run()");
  await runner.run();
  runner_extension.log ("ereplay.cjs: browser.close()");

  // close down
  await browser.close();
  window.destroy();
}

// run main...
main();
