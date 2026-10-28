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
    const result = spawnSync (process.execPath, [synsmell, '--none', '--ban-event-pumping=1', '--test-api-only=1', '--errors',
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

test ('reject direct Tracktion and JUCE access in tests', () => {
  for (const code of ['#include "trkn/tracktion.hh"', '#include <ase/trkn.hh>', '#include "../trkn.hh"',
                      '#include <ase/../trkn/juce.hh>', '#include <juce_audio_basics/juce_audio_basics.h>',
                      '#include <tracktion_engine/tracktion_engine.h>', '#include <juce.h>',
                      'tracktion::Edit *edit;', 'namespace te = tracktion::engine;', 'using namespace juce;',
                      'void test() { Ase::trkn_engine(); }', 'void test() { auto &edit = project->edit_; }']) {
    const result = lint ('ase/tests/direct.cc', code);
    assert.equal (result.status, 1, code + result.stdout + result.stderr);
    assert.match (result.stdout, /tests must use Ase APIs/);
    assert.equal (lint ('ase/tests/helpers/direct.hh', code, true).status, 1, code);
  }
});

test ('allow API tests, comments, strings and engine implementation', () => {
  const result = lint ('ase/tests/public.cc', `
    #include <ase/api.hh>
    #include <ase/path.hh>
    void test() {
      // tracktion::Edit, juce::File, trkn_engine(), project->edit_
      const char *text = "tracktion::Edit";
      auto project = Ase::Server::instance().create_project ("");
      project->start_playback();
    }
  `);
  assert.equal (result.status, 0, result.stdout + result.stderr);
  assert.equal (lint ('ase/project.cc', 'void test() { tracktion::Edit *edit; trkn_engine(); }').status, 0);
});

test ('limit temporary audio exceptions to their named scopes and headers', () => {
  for (const [file, scope] of [['ase/tests/audio.cc', 'class AudioTestDevice'],
                              ['ase/tests/test-setup.cc', 'void load_audio_file_as_clip()'],
                              ['ase/tests/test-setup.cc', 'void loop_around_clip()'],
                              ['ase/tests/test-setup.cc', 'void test_audio_sample_load()'],
                              ['ase/tests/test-setup.cc', 'void test_project_directory_audio()']]) {
    const body = '[] { tracktion::Edit *edit; juce::File file; trkn_engine(); }();';
    const code = scope.startsWith ('class') ? `${scope} { void test() { ${body} } };` : `${scope} { ${body} }`;
    assert.equal (lint (file, code).status, 0, code);
    assert.equal (lint (file, code, true).status, 0, code);
    assert.equal (lint ('ase/tests/other.cc', code).status, 1, code);
    assert.equal (lint (file, code + '\nvoid other() { tracktion::Edit *edit; }').status, 1);
    assert.equal (lint (file, 'juce::File file;').status, 1);
    assert.equal (lint (file, '#include "trkn/tracktion.hh"').status, 0);
    assert.equal (lint (file, '#include "trkn/juce.hh"').status, 1);
    assert.equal (lint (file, '#include <ase/trkn.hh>').status, file.endsWith ('audio.cc') ? 0 : 1);
  }
});

test ('audio exceptions never permit event pumping', () => {
  assert.equal (lint ('ase/tests/audio.cc', 'class AudioTestDevice { void test() { main_loop->iterate (false); } };').status, 1);
  assert.equal (lint ('ase/tests/test-setup.cc', 'void test_project_directory_audio() { main_loop->iterate_pending(); }').status, 1);
});
