// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

const { BrowserWindow, app } = require ("electron");
const pie = require ("puppeteer-in-electron");
const puppeteer = require ("puppeteer-core");
const { createRunner, parse, PuppeteerRunnerExtension } = require ('@puppeteer/replay');

// Maximum timeout per step
const TIMEOUT = 1500;
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
    devTools:                         true,
    defaultEncoding:                  'UTF-8',
  },
  // darkTheme: true,
};

// Helpers
const delay = ms => new Promise (resolve => setTimeout(resolve, ms));

// == TestRunnerExtension ==
class TestRunnerExtension extends PuppeteerRunnerExtension {
  constructor (browser, page, opts, scriptname)
  {
    super (browser, page, opts);
    this.REPLAY = (scriptname ? scriptname + ': ' : '') + 'REPLAY:';
  }
  async beforeAllSteps (flow)
  {
    await super.beforeAllSteps (flow);
    console.log ('\n' + this.REPLAY, 'START...');
  }
  async beforeEachStep (step, flow)
  {
    if (click_types.indexOf (step.type) >= 0 &&
	!step.duration)
      step.duration = 99;	// allow clicks to have a UI effect
    // chain super
    await super.beforeEachStep (step, flow);
    const s = step.selectors ? step.selectors.flat() : [];
    let msg = this.REPLAY + ' ' + step.type;
    if (step.url)
      msg += ': ' + step.url;
    else if (s.length)
      msg += ': ' + s[0];
    else
      msg += '...';
    console.log (msg);
    // Give time to let clickable elements emerge
    if (click_types.indexOf (step.type) >= 0)
      await delay (DELAY);
  }
  async afterEachStep (step, flow)
  {
    await super.afterEachStep (step, flow);
    // Give time to let clicks take effect
    if (click_types.indexOf (step.type) >= 0)
      await delay (2 * DELAY);
  }
  async afterAllSteps (flow)
  {
    await super.afterAllSteps(flow);
    console.log (this.REPLAY, 'DONE.\n');
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
  const runner = await createRunner (flow, new TestRunnerExtension (browser, page, { timeout: TIMEOUT }, jsonfile));

  // start running steps
  await runner.run();

  // close down
  await browser.close();
  window.destroy();
}

// run main...
main();
