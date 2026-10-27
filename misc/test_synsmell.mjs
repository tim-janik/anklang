// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, mkdirSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { test } from 'node:test';

const synsmell = resolve ('misc/synsmell.ts');

function lint (filename, code, absolute = false)
{
  const root = mkdtempSync (join (tmpdir(), 'synsmell-'));
  try {
    mkdirSync (dirname (join (root, filename)), { recursive: true });
    writeFileSync (join (root, filename), code);
    const result = spawnSync (process.execPath, [synsmell, '--none', '--ban-event-pumping=1', '--errors',
                                               absolute ? join (root, filename) : filename],
                              { cwd: root, encoding: 'utf8' });
    assert.ifError (result.error);
    assert.equal (result.signal, null, result.stderr);
    return result;
  } finally {
    rmSync (root, { recursive: true, force: true });
  }
}

test ('reject event pumping', () => {
  for (const call of ['loop->iterate (false)', 'loop.iterate_pending()', 'main_loop->run()',
                      'juce::MessageManager::getInstance()->runDispatchLoop()', 'manager.runDispatchLoopUntil (1)',
                      'juce::detail::dispatchNextMessageOnSystemQueue (true)', 'g_main_context_iteration (ctx, false)',
                      'g_main_loop_run (ctx)']) {
    const result = lint ('ase/tests/pumping.cc', `void test() { ${call}; }`);
    assert.equal (result.status, 1, result.stdout + result.stderr);
    assert.match (result.stdout, /event pumping is forbidden/);
  }
});

test ('allow scheduling, declarations, comments and strings', () => {
  const result = lint ('ase/tests/async.cc', `
    bool iterate (bool block);
    void test() {
      // loop->iterate (false);
      const char *text = "loop->iterate_pending()";
      loop->add (callback);
      co_await loop->delay (interval);
    }
  `);
  assert.equal (result.status, 0, result.stdout + result.stderr);
});

test ('limit startup and shutdown exceptions to their own functions', () => {
  for (const [file, name, call] of [['ase/main.cc', 'main', 'main_loop->run()'],
                                  ['ase/main.cc', 'main', 'main_loop->iterate_pending()'],
                                  ['ase/trkn.cc', 'trkn_init', 'main_loop->iterate_pending()'],
                                  ['ase/trkn.cc', 'trkn_shutdown', 'main_loop->iterate_pending()']]) {
    assert.equal (lint (file, `void ${name}() { ${call}; }`).status, 0);
    assert.equal (lint (file, `void ${name}() { ${call}; }`, true).status, 0);
    assert.equal (lint (file, `void other() { ${call}; }`).status, 1);
    assert.equal (lint (file, `void ${name}() { loop->add ([] { ${call}; }); }`).status, 1);
    assert.equal (lint ('ase/tests/startup.cc', `void ${name}() { ${call}; }`).status, 1);
  }
  assert.equal (lint ('ase/main.cc', 'void main() { main_loop->iterate (false); }').status, 1);
  assert.equal (lint ('ase/trkn.cc', 'void trkn_init() { main_loop->run(); }').status, 1);
});
