---
title: Development guardrails
---

Source audit of `b7e211ea`, 2026-09-27. These are proposed checks and development rules, not adopted policy.
The review covers first-party engine code, UI, Electron, build rules, and tests. It excludes vendored implementations,
including `trkn/`. It includes the ASE adapters and the Jsonipc client used to cross the UI boundary.
This is a targeted audit, not a claim that every source line or runtime path was checked.

The goal is to keep coding agents and humans from adding hacks, crossing boundaries, or hiding broken behavior.
Compiler warnings stay non-fatal, because Anklang supports many compilers. Gate new and changed code instead:
lint results, layering checks, and behavioral tests on the files a branch touches.

## Checks that currently let bad changes through

- Make ESLint failures fail `make check` and `make strict`. The ESLint recipe starts with `-`, so Make ignores its failure.
  Use a reviewed baseline for existing findings and reject new findings. Add a fixture that deliberately breaks a rule and
  proves the full Make target fails. [Makefile.mk:328](../Makefile.mk#L328)

- Repair the UI smell-check file list and reject empty input. `ui/synsmell.files:` defines a target, not a variable;
  the command expands `$(ui/synsmell.files)` to nothing. An isolated Make probe confirmed `origin=undefined files=[]`.
  Also check the filename patterns and stray closing parenthesis. Both UI and ASE invoke `synsmell` without `--errors`,
  so diagnostics alone do not fail them. [ui/Makefile.mk:117](../ui/Makefile.mk#L117),
  [ase/Makefile.mk:355](../ase/Makefile.mk#L355)

- Give Stylelint a real check target. `stylelint: FORCE` has no lint command, and the Vite plugin invocation is commented out.
  Check standalone CSS and extracted `Extra_css` templates, with source locations preserved. Enable duplicate-property and
  shorthand-overwrite checks, allowing intentional fallbacks explicitly. Account for the project's CSS extensions before
  rejecting unknown at-rules. [Makefile.mk:334](../Makefile.mk#L334), [vite.config.ts:105](../vite.config.ts#L105),
  [ui/stylelintrc.cjs:48](../ui/stylelintrc.cjs#L48)

- Include TS and TSX in semantic linting. ESLint explicitly ignores `.ts`, `.cts`, and `.mts`; its JS parser configuration
  does not establish a TSX lint path. The TypeScript check disables implicit-any and null checks despite `strict: true`,
  and `ui/*.js` is not type-checked at all. Start with public component props, backend proxies, and newly changed files.
  Add rules for discarded promises, async promise executors, unsafe casts, and Solid reactive ownership.
  `eslint-plugin-solid` and `eslint-plugin-unused-imports` are installed but not enabled. Prove each extension is checked
  with a failing fixture. [ui/eslintrc.js:72](../ui/eslintrc.js#L72), [ui/eslintrc.js:14](../ui/eslintrc.js#L14),
  [tsconfig.json:6](../tsconfig.json#L6)

- Keep Node and browser lint scopes separate. `electron/*.js` matches both the CommonJS block and the later browser ESM
  block. Check the effective configuration for Electron, build scripts, browser code, and tests. Do not silence missing
  imports by adding more application globals. Restore useful unused-variable, constant-expression, precision, and empty-block
  checks with narrow exceptions. [ui/eslintrc.js:29](../ui/eslintrc.js#L29)

- Make C++ analysis results and tool failures enforceable. `clang-tidy` is advisory, its invocation swallows errors, and
  `branch-check` defaults to exit zero. `clang-tidy-check` exists, but relies on finding warning/error text in logs.
  There is no committed `.clang-tidy`, so the check set depends on the local tool defaults. Commit a small check list
  (for example `bugprone-use-after-move`, `bugprone-dangling-handle`, `clang-analyzer-cplusplus.NewDelete`,
  `performance-unnecessary-copy-initialization`) and run it on changed files only. Preserve the analyzer's exit status
  separately and run it with the actual compilation flags. The CI job was disabled because it was advisory and duplicated
  a full build; reuse build outputs instead of restoring that cost unchanged.
  [misc/Makefile.mk:25](../misc/Makefile.mk#L25), [.github/workflows/ci.yml:61](../.github/workflows/ci.yml#L61)

- Test the guardrail tools themselves. Cover unknown options, zero files, each supported extension, malformed syntax,
  macro-heavy C++, and deliberate violations. `synsmell` warns on unknown options, accepts no files, and normally tolerates
  parser errors. Its macro stripping also leaves areas unexamined. Report that coverage and use compiler/parser checks for
  syntax that Tree-sitter cannot handle. [misc/synsmell.ts:215](../misc/synsmell.ts#L215),
  [misc/synsmell.ts:324](../misc/synsmell.ts#L324), [misc/synsmell.ts:371](../misc/synsmell.ts#L371)

- Track unfinished behavior, not just forbidden words. `synsmell` bans the fix-me marker but permits `TODO`; changing the word would
  satisfy the rule without fixing anything. `check-unfinished.sh` also only looks for the fix-me marker, only runs on pre-push, and
  excludes seven files that still contain it. Require a concrete limitation, a tracked follow-up, and a test or explicit
  unsupported result for incomplete behavior. Do not encourage agents to delete useful warnings from comments.
  [misc/synsmell.ts:54](../misc/synsmell.ts#L54), [misc/check-unfinished.sh](../misc/check-unfinished.sh),
  [.pre-commit-config.yaml:37](../.pre-commit-config.yaml#L37), [ase/monitor.cc:17](../ase/monitor.cc#L17)

## Proposed lint rules and layering checks

These are cheap text or Tree-sitter checks. Run them on changed lines first, with a reviewed baseline for old code.

- Layering, as grep checks next to the existing `grep-pch` rule [Makefile.mk:453](../Makefile.mk#L453):
  - No `juce`, `tracktion`, or `trkn/` names in `ase/api.hh`; it is the UI-facing contract.
  - Only `*.cc` files include `trkn/juce.hh` or `trkn/tracktion.hh`, as the first include (PCH); headers use
    `trkn/tracktion_decls.hh`. `grep-pch` already covers the header half.
  - No `import … from 'lit'` outside [ui/little.js](../ui/little.js); new UI components are SolidJS `.tsx` only.
  - No new `.js` or `.jsx` files under `ui/b/`.
  - Every `emit_notify ("x")` names a property declared in `ase/api.hh`. A typo there is silent today.
    The same holds for `APPLY_IDL_PROPERTY`, which requires the property name to equal `__func__`.
    [ase/internal.hh:22](../ase/internal.hh#L22)

- `synsmell` additions:
  - Ban `printf`, `printerr`, `std::cerr`, and `std::cout` in `ase/*.cc` outside `main.cc`, `logging.cc`, `testing.cc`,
    and `benchmarks.cc`. Use `Ase::debug()` with a key or `Ase::diag()`.
  - Ban `if (0)`, `#if 0`, and `0 &&` in new code. Dead code belongs in history, not in the tree.
  - Ban `(window as any)`, `(globalThis as any)`, and `(this as any)` in `ui/b/`. There are 88 `as any` casts today;
    the worst reach into application globals. [ui/b/contextmenu.tsx:252](../ui/b/contextmenu.tsx#L252),
    [ui/b/playcontrols.tsx:23](../ui/b/playcontrols.tsx#L23)
  - Ban `typeof obj.method === 'function'` on Ase objects; the generated types say whether a method exists.
    [ui/b/partlist.tsx:33](../ui/b/partlist.tsx#L33)
  - Ban assigning methods onto DOM elements through `any`; expose a typed handle instead.
    [ui/b/contextmenu.tsx:594](../ui/b/contextmenu.tsx#L594), [ui/b/editable.tsx:101](../ui/b/editable.tsx#L101)
  - Ban `setTimeout (r, <number>)` sleeps in `ui/tests/` and `ui/dom.js`. [ui/dom.js:204](../ui/dom.js#L204),
    [ui/dom.js:235](../ui/dom.js#L235)
  - Ban `new Function` and `eval` outside the `--ui-js` runner. [ui/startup.js:258](../ui/startup.js#L258),
    [ui/util.js:848](../ui/util.js#L848)
  - Require `onCleanup` in any component that calls `setTimeout`, `setInterval`, `addEventListener`, or `new *Observer`.
  - Ban `new Promise (async …)`, see below.

- Every `ASE_DEBUG` key used in code must be listed in `--help`. About 25 keys exist; `--help` lists two, and the key
  case is mixed (`Platform`, `AtQuit`, `Test`). [ase/main.cc:93](../ase/main.cc#L93)

## Behavioral contracts that need tests

- Never report a successful save or load without preserving project state. Save writes `project.json` as `{}`, snapshot
  returns `{}`, and load reads the JSON but does not restore it before returning success. Require a round trip containing
  tracks, notes, plugins, parameters, and referenced assets. Until implemented, return an explicit unsupported result.
  [ase/project.cc:546](../ase/project.cc#L546), [ase/project.cc:579](../ase/project.cc#L579),
  [ase/project.cc:719](../ase/project.cc#L719)

- Make note identity stable and independent of pitch and position. `all_notes()` gives every note ID `1`;
  `change_batch()` finds an existing note by the incoming tick and key. Moving or transposing a note can therefore miss
  the original, and overlapping equal-pitch notes are ambiguous. Test duplicate notes, moves, transposes, deletes,
  selection, and undo while retaining the same identity. [ase/clip.cc:174](../ase/clip.cc#L174),
  [ase/clip.cc:317](../ase/clip.cc#L317)

- Reject fabricated support at API boundaries. Note reads currently force `selected=false` and `fine_tune=0`;
  clip telemetry is empty; monitor methods and engine statistics are stubs; property hint checks in serialization
  return a fixed `true` or `false`. Define supported capabilities and explicit unsupported outcomes. A plausible default
  must not masquerade as a working feature or hide a failed lookup.
  [ase/clip.cc:181](../ase/clip.cc#L181), [ase/clip.cc:279](../ase/clip.cc#L279),
  [ase/monitor.cc:17](../ase/monitor.cc#L17), [ase/server.cc:239](../ase/server.cc#L239),
  [ase/serialize.cc:126](../ase/serialize.cc#L126)

- Ban `new Promise(async ...)`. `Jsonipc.add_prop_promise()` uses it: a rejected awaited operation rejects the executor's
  separate promise while leaving the returned promise pending. An isolated probe of the actual method reproduced this.
  Chain promises directly and test rejection in both the previous and next operation, including cleanup of `$promise`.
  [jsonipc/jsonipc.ts:72](../jsonipc/jsonipc.ts#L72)

- Settle pending requests when a connection or worker dies. The Jsonipc request map and script-host request map store
  resolve callbacks; their request paths do not provide cancellation or a reject callback for lost replies. Test disconnect
  before handshake, disconnect during a setter, a throwing script function, and an unknown handle. A timeout should report
  the failed operation, not retry a mutation that may already have run. [jsonipc/jsonipc.ts:156](../jsonipc/jsonipc.ts#L156),
  [jsonipc/jsonipc.ts:271](../jsonipc/jsonipc.ts#L271), [ui/host.js:21](../ui/host.js#L21)

- Pair temporary runtime state with unconditional cleanup. `telemetry_blocked++` is followed by an awaited RPC and a
  decrement without `finally`; a rejection leaves telemetry blocked. The JSON request handler also sets and manually clears
  a global current-connection pointer. Use scoped cleanup and test failure and reentry between acquisition and release.
  [ui/util.js:1400](../ui/util.js#L1400), [ase/jsonapi.cc:87](../ase/jsonapi.cc#L87)

- Give timers an explicit owner and cancellation path. `TelemetryPlan::setup()` captures a strong `tplan` in its repeating
  timer, while cancellation is in the plan destructor. Dropping the connection's reference alone cannot destroy a plan
  still owned by that callback. Test connect/subscribe/disconnect loops, empty subscriptions, and interval changes; require
  active timer and plan counts to return to baseline. [ase/server.cc:556](../ase/server.cc#L556)

- Check untrusted sizes before adding, allocating, or copying. Telemetry offsets and lengths are signed 32-bit fields;
  validation adds them before converting to `size_t`. Use checked arithmetic and test overflow, negative values, overlap,
  alignment, and the last byte of the arena. Keep frontend field widths and backend segment rules in one tested contract.
  [ase/api.hh:384](../ase/api.hh#L384), [ase/server.cc:497](../ase/server.cc#L497),
  [ui/util.js:1413](../ui/util.js#L1413)

- Apply archive limits before reading or decompressing. `StorageReader::stringread(maxlength)` truncates after the full
  entry has been read and possibly decompressed. It is not an allocation bound. Test oversized declared lengths, truncated
  archives, corrupt compressed data, and expansion limits. Preserve the existing restriction on listed entry names with
  slashes or backslashes. [ase/storage.cc:420](../ase/storage.cc#L420), [ase/storage.cc:451](../ase/storage.cc#L451),
  [ase/storage.cc:521](../ase/storage.cc#L521), [ase/compress.cc:84](../ase/compress.cc#L84)

- Make saves recoverable after each failure point. The current save path renames the previous file into a backup before
  opening the new archive, and continues if backup creation fails. Define the desired failure behavior explicitly.
  Prefer writing and closing a temporary file before replacing the destination. Inject write, close, rename, and disk-full
  failures; check both the destination and backup contents. [ase/project.cc:513](../ase/project.cc#L513)

## Engine and UI boundaries

These rules name who owns state and when it may change.

- Keep audio callbacks free of blocking locks, filesystem work, allocation, and synchronous logging. The LiquidSFZ adapter
  deliberately uses an atomic flag because even `std::mutex::try_lock()` and unlock were considered unsuitable here.
  Contention produces silence rather than waiting. Test that fallback and move loading work off the UI thread through a
  defined state handoff. Add callback-time allocation/lock checks and bounded timing counters.
  [devices/liquidsfz/liquidsfzplugin.cc:7](../devices/liquidsfz/liquidsfzplugin.cc#L7)

- Preserve the unusual allocator and lock-free lifetime rules. `MpmcStack` requires popped nodes to remain readable,
  an empty stack at destruction, and no concurrent push/pop during destruction. It is lock-free, not wait-free.
  Do not substitute a conventional reclaiming allocator or call it bounded-time without proof. Keep the lock-free static
  assertions and add concurrent ownership and shutdown tests. [ase/atomics.hh:83](../ase/atomics.hh#L83)

- Declare the thread for each engine entry point. Several transport callbacks assert `this_thread_is_ase()`, while JUCE
  message posting crosses threads through a locked queue and wakes the ASE loop. Require main-thread assertions for model
  mutation and documented handoffs from workers. Preserve dispatch outside the queue lock. Do not add nested JUCE loops
  to compensate for an unimplemented callback adapter. `this_thread_is_main()` is only another name for
  `this_thread_is_ase()`; do not read it as a separate JUCE message thread check. A JUCE message callback that throws
  currently unwinds through the ASE main loop ("TODO: catch exceptions"). [ase/project.cc:64](../ase/project.cc#L64),
  [ase/juce-linux.cc:53](../ase/juce-linux.cc#L53), [ase/juce-linux.cc:78](../ase/juce-linux.cc#L78),
  [ase/platform.hh:178](../ase/platform.hh#L178)

- Callbacks must keep their owner alive, and every listener needs a matching removal in the destructor. The use-after-free
  fixes in the WebSocket I/O handler, the transport listener, and `TelemetryPlan` all came from breaking this. Capture a
  `shared_ptr` to the owner in main-loop handlers; do not capture raw `this` into a callback that can outlive it.
  [ase/websocket.cc:661](../ase/websocket.cc#L661), [ase/project.cc:64](../ase/project.cc#L64),
  history `c0d03f05`, `6273e4de`, `6a0080f4`

- Confine Tracktion/JUCE details to the existing ASE adapters and device implementations. The `ase_obj_` backpointer and
  `SelectableWeakref` already bridge identity and lifetime; map objects only through `register_ase_obj()`,
  `unregister_ase_obj()`, and `find_ase_obj()`. Reject new direct accesses outside an explicit adapter list;
  do not duplicate the engine model in UI state. Test one wrapper per engine object and access after removal, undo, and
  project discard. Check both size and alignment where opaque storage wraps a third-party type.
  [ase/trkn-utils.hh:10](../ase/trkn-utils.hh#L10), [ase/trkn-utils.hh:46](../ase/trkn-utils.hh#L46),
  [ase/trkn-utils.cc:17](../ase/trkn-utils.cc#L17), [ase/track.cc:129](../ase/track.cc#L129)

- Change the public API and its generators together. `ase/api.hh` produces C++ registrations and TypeScript bindings;
  patching a generated binding is not a fix. Generated `*.g.*` files are committed and checked for drift; update them with
  `make codegen`. Compile a small client that uses the changed interface. Keep generated sources out of ordinary style
  baselines but still type-check their output. [ase/Makefile.mk:63](../ase/Makefile.mk#L63),
  [Makefile.mk:214](../Makefile.mk#L214)

- The UI talks to ASE only through the generated types. Know the Jsonipc caching rules: `$get()` returns the default on
  first access and queues a fetch; use `$refetch (getter)` when the real value is needed now. `$rpc()` relies on the
  backend sending change notifications before the method result. A `Class&` argument in `from_json` throws on `nullptr`.
  [jsonipc/jsonipc.ts:207](../jsonipc/jsonipc.ts#L207), [doc/ch-development.md:77](ch-development.md)

- Specify units and legal ranges at every conversion. Notes cross ticks, beats, seconds, MIDI channel bases, and velocity
  scales. LiquidSFZ explicitly converts channels from 1-based to 0-based. Validate at the engine boundary as well as in
  widgets, with NaN, infinity, negative duration, and end-of-buffer cases. Do not infer ranges from a convenient cast.
  [ase/clip.cc:158](../ase/clip.cc#L158), [ase/clip.cc:343](../ase/clip.cc#L343),
  [devices/liquidsfz/liquidsfzplugin.cc:105](../devices/liquidsfz/liquidsfzplugin.cc#L105)

- Ban bitwise integer coercion for values whose contract exceeds 32 bits. `NumberInput` defaults to safe-integer bounds
  but rounds with `0 | value`, which wraps outside signed 32-bit range. Use range-aware rounding and test large positive
  and negative values. Keep intentional masks and packed fields as explicit exceptions.
  [ui/b/numberinput.tsx:73](../ui/b/numberinput.tsx#L73)

- Convert musical ranges using their endpoints under the tempo map. `assign_range()` first sets a length computed from
  duration beats as a position, then overwrites it with `end_time - start_time`. Avoid the extra mutation and notification.
  Test a clip that starts after a tempo change and one that spans it, negative pre-roll, unusual time signatures, and long
  timelines. Preserve the transport precision assertions. [ase/clip.cc:114](../ase/clip.cc#L114),
  [ase/transport.cc:10](../ase/transport.cc#L10)

- Treat stop completion as asynchronous. `stop_playback()` waits for the stopped callback before resetting position so
  the reset persists. Test stop, immediate restart, repeated stop, and project disposal with a stop callback pending.
  Keep cursor position distinct from latency-compensated audible time in APIs and displays.
  [ase/project.cc:168](../ase/project.cc#L168), [ase/project.cc:191](../ase/project.cc#L191),
  [ase/project.cc:947](../ase/project.cc#L947)

- Test each mutation as a full contract: model value, notification, undo, redo, and UI refresh. The JSON API deliberately
  sends coalesced notifications before the method reply. Preserve that order and notify list reordering as well as insertion
  and removal. Note tests currently verify notification on insertion but only the note count after undo/redo; extend them
  to verify notifications and cached UI values too. [ase/jsonapi.cc:252](../ase/jsonapi.cc#L252),
  [ase/track.cc:109](../ase/track.cc#L109), [ase/tests/prjtests.cc:644](../ase/tests/prjtests.cc#L644)

- Keep undo transaction ownership explicit. UI grouping calls and backend setters both begin transactions. Specify which
  layer groups a gesture, then test a drag with many setter calls as one undo step, a cancelled gesture, and two consecutive
  edits. Do not fix grouping bugs by silently clearing undo history. [ase/project.cc:757](../ase/project.cc#L757),
  [ase/project.cc:850](../ase/project.cc#L850), [ase/clip.cc:320](../ase/clip.cc#L320)

## UI rules learned from experience

- Use the shared input edit grace period. Show the local edit immediately, send it once, and then show the latest backend
  value even if no new notification arrives. Backend updates must not emit another edit. Rapid toggles use the displayed
  value. The recent input changes replaced longer-lived optimistic state and write counting; do not recreate those schemes
  in individual widgets. [ui/input.ts:5](../ui/input.ts#L5), [ui/b/toggle.tsx](../ui/b/toggle.tsx),
  history `0d4f5760`, `93d29add`, `9dbda751`

- Keep identity changes separate from value changes in reactive effects. A piano-roll clip replacement resets scrolling
  and registers playback callbacks; editing notes must only update layout and paint. Test a note edit while scrolled away
  from the origin, and rapid replacement while a previous fetch is pending.
  [ui/b/pianoroll.tsx:265](../ui/b/pianoroll.tsx#L265)

- Preserve deliberate UI rebuild boundaries. `App.assign_project()` rebuilds the tree on project switches; `relayout()`
  rebuilds it on window/DPR changes while preserving selected state. `Shell` persists across both. Device editors use a
  keyed child per device and load a fixed field list. Do not add per-component replacement machinery without a
  demonstrated requirement. Test the state that must survive each rebuild. [ui/b/app.ts:88](../ui/b/app.ts#L88),
  [ui/b/app.ts:139](../ui/b/app.ts#L139), [ui/b/deviceeditor.tsx:205](../ui/b/deviceeditor.tsx#L205)

- Load dialog contents before opening, so the dialog starts at its final size and does not jump while fields load.
  [ui/b/aboutdialog.jsx](../ui/b/aboutdialog.jsx), [ui/b/preferencesdialog.jsx](../ui/b/preferencesdialog.jsx)

- Require typed component handles instead of reaching through legacy internals. Sidebar resizing still casts `this` to
  `any` and queries a `shadowRoot`; the piano controller reads `note_cache.notes` beside a comment saying that cache does
  not exist. Migrations must update callers and remove obsolete paths. Flag new `$refs`, framework-instance tricks, and
  casts used to bypass missing members. [ui/b/shell.tsx:352](../ui/b/shell.tsx#L352),
  [ui/b/piano-ctrl.js:302](../ui/b/piano-ctrl.js#L302)

- Let components own their rendered children. Context menus explicitly avoid modifying application-owned Icon components
  because those have their own reactive updates. Pass state through props or a documented component handle; avoid selectors
  that mutate another component's private DOM. Test live icon and label changes while a menu is open and closed.
  [ui/b/contextmenu.tsx:461](../ui/b/contextmenu.tsx#L461)

- Pair every listener, timer, observer, reactive root, and telemetry subscription with disposal. Late async completion must
  either belong to the current owner or clean up without touching the replacement. `App` already checks a generation after
  awaits, and the piano roll refuses to repaint a detached root. Add repeated mount/unmount and failed-load tests;
  `signal.js` still has a tracking wrapper with no destroy path. Finalization is a fallback, not timely cleanup.
  [ui/b/app.ts:94](../ui/b/app.ts#L94), [ui/b/pianoroll.tsx:214](../ui/b/pianoroll.tsx#L214),
  [ui/b/trackview.tsx:246](../ui/b/trackview.tsx#L246), [ui/signal.js:36](../ui/signal.js#L36)

- Preserve modal interaction edge cases. A backdrop close requires an outside pointer start and finish; text-selection
  drags, keyboard activation at zero coordinates, Escape after pointerdown, and clicking the opener again must not cause
  false closes or reopen loops. Close callbacks must run once, including unmount and immediate reopen. Keep these tests
  around the shared helper rather than adding separate menu-specific timing fixes. [ui/dom.js:90](../ui/dom.js#L90),
  [ui/tests/modals_test.ts](../ui/tests/modals_test.ts), history `f2e2e3dd`

- Define hidden, closed, disabled, and unmounted separately for keyboard handling. Menu shortcut buttons intentionally
  remain mounted when a popup closes; their panel decides when shortcuts are active. A hidden piano roll must not keep
  handling keys. Test focus restoration, pointer cancellation, disabled controls, and repeated open/close cycles.
  [ui/b/contextmenu.tsx:380](../ui/b/contextmenu.tsx#L380), [ui/b/pianoroll.tsx:346](../ui/b/pianoroll.tsx#L346),
  [ui/tests/toggle_test.ts:311](../ui/tests/toggle_test.ts#L311)

- Keep focusable controls in the light DOM where the existing focus and keyboard code can find them. The component guide
  explicitly records failures with controls buried in shadow DOM. Its Lit/Vue implementation guidance is now partly stale
  beside the Solid migration; update it to distinguish preserved behavioral rules from obsolete framework advice.
  New components are SolidJS only; `ui/little.js` still exports Lit for remaining users and should shrink, not grow.
  Do not add components solely to wrap layout or styling. [ui/ch-component.md:29](../ui/ch-component.md#L29),
  [ui/little.js:33](../ui/little.js#L33)

- Centralize pointer and wheel normalization. `mouse.js` accounts for differing CSS/device pixel coordinates, Firefox's
  delta-read ordering, wheel units, and unreliable coalesced movement values. Require callers to use those helpers.
  Test zoom, fractional DPR, monitor changes, line/page wheel modes, and pointer lock. Remove browser workarounds only after
  a reproducer passes on the supported browsers. [ui/mouse.js:20](../ui/mouse.js#L20)

- Treat zero as a valid value when the contract allows it. `get_uri()` explicitly permits numeric zero, but menu positioning
  tests page coordinates for truthiness. Use absent-value checks for IDs and coordinates. Add tests at the top/left edge,
  URI `0`, empty choices, and a selected value that disappears from a refreshed choice list.
  [ui/dom.js:32](../ui/dom.js#L32), [ui/b/contextmenu.tsx:282](../ui/b/contextmenu.tsx#L282)

- Test timing through the existing timer helpers. Immediate input events and display changes should be checked synchronously;
  a frame wait can outlast the edit timer and test a different state. Use captured timers for expiry and explicit completion
  for async loads. Keep real-browser tests for layout, focus, and pointer behavior. Do not fix flaky tests by increasing
  sleeps. [ui/tests/timers.ts:3](../ui/tests/timers.ts#L3), [ui/dom.js:217](../ui/dom.js#L217)

## Desktop, filesystem, and process boundaries

- Keep Electron sandboxing, context isolation, and disabled Node integration as checked defaults. The preload exposes a
  generic named-call bridge and main dispatch forwards arguments without checking the sender frame. Define allowed callers
  and argument shapes per IPC method. Test rejected frames and malformed arguments before adding more privileged methods.
  [electron/main.js:148](../electron/main.js#L148), [electron/main.js:307](../electron/main.js#L307),
  [electron/preload.js:5](../electron/preload.js#L5)

- Use a shared, tested URL policy for windows, child windows, navigation, and external opening. Electron accepts every
  `file:///` URL and a range of localhost URLs; the child-window navigation hook only logs. Backend Origin validation
  accepts localhost on any port, with its `port` argument unused. Specify required development exceptions before narrowing
  this. Test unrelated local services, alternate schemes, child navigation, and the authentication redirect.
  [electron/main.js:124](../electron/main.js#L124), [electron/main.js:230](../electron/main.js#L230),
  [ase/jsonapi.cc:27](../ase/jsonapi.cc#L27)

- Preserve the actual authentication boundary. The running server uses a cookie token; its WebSocket subprotocol is
  currently empty. `--unauth-dev` deliberately disables token authentication. Test missing/wrong cookies, disallowed
  Origins, the explicit development mode, and token redaction in logs. Do not weaken these checks to fix a development
  connection failure, or describe the subprotocol as providing protection it currently does not provide.
  [ase/main.cc:594](../ase/main.cc#L594), [ase/websocket.cc:350](../ase/websocket.cc#L350),
  [ase/websocket.cc:520](../ase/websocket.cc#L520)

- Test path confinement after URL decoding and filesystem resolution. The server decodes, simplifies, and applies aliases;
  aliases deliberately expose additional roots. Specify which symlinks may leave each root, then test encoded `..`, encoded
  separators, alias-prefix collisions, and symlink escapes. Lexical path cleanup alone is not a confinement test.
  [ase/websocket.cc:124](../ase/websocket.cc#L124)

- Preserve filesystem names across the UTF-8 UI boundary. The crawler separates `encodefs`, `decodefs`, and display labels;
  project save/load decodes the incoming path. Do not replace encoded path identity with a human-readable label or assume
  every filename is valid UTF-8. Test invalid byte sequences, spaces, percent signs, and round trips through dialogs.
  [ase/crawler.cc:63](../ase/crawler.cc#L63), [ase/project.cc:472](../ase/project.cc#L472),
  [ase/unicode.hh:23](../ase/unicode.hh#L23)

- Require proof of ownership before recursive cache deletion. ASE uses a directory prefix and a PID guard containing boot
  and executable information. Electron instead treats any failed `kill(pid, 0)` as a missing process. Test permission denial,
  PID reuse, malformed names, symlinks, and two running instances. Share the deletion contract instead of broadening catches
  or treating every probe failure as permission to delete. [ase/storage.cc:25](../ase/storage.cc#L25),
  [ase/storage.cc:159](../ase/storage.cc#L159), [electron/main.js:62](../electron/main.js#L62)

- Keep platform workarounds narrow and explain their exit condition. Snap browsers may not read the normal cache redirect,
  JACK thread creation requires restoring the caller's thread name, and a Darwin account lookup has extra buffer padding.
  Keep each workaround in its adapter with the affected platform, reproducer, and removal condition. A blanket ban on all
  workarounds would remove known compatibility behavior. [ase/webui.cc:20](../ase/webui.cc#L20),
  [ase/driver-jack.cc:286](../ase/driver-jack.cc#L286), [ase/path.cc:1084](../ase/path.cc#L1084)

- Preserve failure exit status and shutdown ordering. Electron avoids ordinary `quit()` because intercepted termination
  signals can otherwise yield success, and its exit handler may reenter. ASE disables core dumps by default because dumping
  a large child can stall failure exit. Test child crashes, signals, pending requests, and repeated cleanup; preserve the
  explicit `ASE_DEBUG=coredump` opt-in. [electron/main.js:87](../electron/main.js#L87),
  [ase/main.cc:402](../ase/main.cc#L402), [ase/trkn.cc:86](../ase/trkn.cc#L86)

- Keep the headless Electron settings: the ozone headless platform, GPU disabled in headless mode, and no `/dev/shm` inside
  containers. Each fixed a startup hang or crash in CI. [electron/main.js:420](../electron/main.js#L420)

## Build, platform, and vendor constraints

- g++ ≥ 14 is required, and `CC` and `CXX` must be the same compiler family. [misc/config-uname.mk:18](../misc/config-uname.mk#L18)

- JUCE is pinned to 7.0.12 because it is the last GPL-3.0 release. Never upgrade JUCE past it; this is a license limit,
  not a technical one. [misc/import-trkn.sh:10](../misc/import-trkn.sh#L10)

- Edits to `trkn/` are rare, one commit each, named `trkn/<path>: …`. Current local patches include the
  `Ase::VirtualBase *ase_obj_` field in `tracktion_Selectable.h`, disabled static singleton destructors in JUCE
  `MidiDevices` and Tracktion `CrashTracer`, log redirection, and C++23 build fixes. Keep a list of them so a re-import
  does not drop one silently.

- Keep clean parallel builds and distribution builds as required evidence for build changes. Recent fixes added the missing
  dependency from `jsonapi.o` to generated registration code and fetched external headers before building the Tracktion PCH.
  Do not fix these races with sleeps, serial builds, or blanket dependencies. Preserve the PCH-first include requirement
  and test from a fresh tarball without `.git` or old generated outputs. [ase/Makefile.mk:98](../ase/Makefile.mk#L98),
  [ase/project.cc:2](../ase/project.cc#L2), [Makefile.mk:400](../Makefile.mk#L400), history `14ad1aab`, `95f47120`

- Preserve numerical behavior in optimized builds. The build enables fast math but explicitly restores NaN/infinity
  handling. Test DSP and range checks under production flags as well as sanitizers. Review floating-point flag changes,
  integer narrowing, and C-style casts at boundary code; use local exceptions for measured DSP needs rather than weakening
  warnings for the whole tree. [misc/config-uname.mk:71](../misc/config-uname.mk#L71)

- Code that is not built rots. Only LiquidSFZ is built among the devices; BlepSynth, Freeverb, Saturation, and ColoredNoise
  are commented out. Port or build-check them before relying on their code. [devices/Makefile.mk:8](../devices/Makefile.mk#L8)

## Known hacks and stubs, the initial baseline

Existing occurrences are not mandatory rewrites. They are the baseline new code must not add to.

- Unconditional debug output: every transport change prints through `printerr`. [ase/project.cc:180](../ase/project.cc#L180)
- Thread-safety gap marked in the code: "TODO: not thread-safe". [ase/websocket.cc:211](../ase/websocket.cc#L211)
- Dead switches: `ASE_DEBUG=jsipc` and `jsbin` sit behind `if (0)`; nine `#if 0` blocks remain.
  [ase/main.cc:129](../ase/main.cc#L129)
- Unfinished engine results: `engine_stats()` returns `"Unused"`, monitor getters are stubs, UI `discard` only logs.
  [ase/server.cc:236](../ase/server.cc#L236), [ui/util.js:1331](../ui/util.js#L1331)
- Placeholder UI actions: play controls dispatch `'-todo-Record'` and similar strings.
  [ui/b/playcontrols.tsx:46](../ui/b/playcontrols.tsx#L46)
- Duck typing against removed API methods, global reach-through, DOM method patching, and fixed test sleeps, as listed
  in the lint section above.
- Stale guidance that misleads contributors and agents: `ui/ch-component.md` teaches Lit and Vue; `doc/ch-development.md`
  links to `blob/master` while the main branch is `trunk`; a Jsonipc comment says `this` is frozen when it is not.
  [ui/ch-component.md](../ui/ch-component.md), [doc/ch-development.md:28](ch-development.md),
  [jsonipc/jsonipc.ts:199](../jsonipc/jsonipc.ts#L199)

## Lessons learned and rejected approaches

Keep a short record of rejected approaches and compatibility decisions, linked to the reproducer and relevant test or
commit. Distinguish a documented failure from an approach that simply has not been tried.

- UI frameworks churned from Vue 2 to Vue 3 to Lit to SolidJS. Finish the SolidJS migration instead of mixing frameworks.
- Replacing the in-house DSP engine with Tracktion brought regressions. Porting a device means checking its sound with
  `check-audio`, not only that it builds.
- Persistent optimistic widget state and write counting were replaced by a shared edit grace period and a final
  backend read. [ui/input.ts](../ui/input.ts)
- Per-component project replacement was replaced by rebuilding the tree on project switches. [ui/b/app.ts:90](../ui/b/app.ts#L90)
- Focusable controls inside shadow DOM broke focus and keyboard handling. [ui/ch-component.md:43](../ui/ch-component.md#L43)
- Rewinding the transport before stop completed lost the reset; it now waits for the stop callback.
- Removing modal handlers raced a quick reopen. History `f2e2e3dd`
- Fixed sleeps and frame waits in tests were replaced by synchronous checks, captured timers, and explicit completion.
- An advisory clang-tidy CI job that duplicated a full build was dropped. History `38b9d152`
- Killing the process group at exit was replaced by a subreaper with step-wise termination. History `1e8edc03`
- Static singleton destructors in JUCE and Tracktion crash at exit and are disabled. History `ffbc7a92`, `4ab0702a`
- Tailwind's internal lightningcss optimization is off; Vite build warnings now fail the build. History `78180777`,
  [vite.config.ts:52](../vite.config.ts#L52)
- Test-call extraction dropped the TypeScript compiler for a regex; simpler won. History `fe733959`

## Visibility and debugging

Visibility should expose state and ownership without disturbing audio processing.

- Replace `engine_stats()` returning `Unused` with a typed snapshot: device and sample rate, block size, callback duration
  distribution, underruns, load/lock-contention silence, and queued work. Collect bounded counters in callbacks and format
  them elsewhere. Include missing/unavailable status so zero does not imply a healthy device.
  [ase/server.cc:239](../ase/server.cc#L239),
  [devices/liquidsfz/liquidsfzplugin.cc:87](../devices/liquidsfz/liquidsfzplugin.cc#L87)

- Add a telemetry inspector with field names, types, arena ranges, owners, subscription counts, active timers, bytes sent,
  and time since the last update. Report rejected plans and blocked-reschedule state. Add a generation identifier if plans
  can change while payloads are in flight. This would make leaks and stale meter layouts visible without reading raw memory.
  [ase/server.cc:497](../ase/server.cc#L497), [ui/util.js:1339](../ui/util.js#L1339)

- Extend existing RPC logging with request age, pending count, object identity, notification order, and completion/error
  status. Add a bounded event trace that can be exported on test failure. Redact authentication material and make argument
  capture opt-in. Reuse existing JSON IPC log switches and `ASE_DEBUG` categories instead of scattered unconditional prints;
  revive the dead `ASE_DEBUG=jsipc` switch. [ase/main.cc:119](../ase/main.cc#L119),
  [ase/jsonapi.cc:252](../ase/jsonapi.cc#L252), [ase/logging.hh:62](../ase/logging.hh#L62)

- Make debug keys discoverable. Add `--list-debug-keys`, use lowercase keys consistently, and list each key with one line
  of meaning. [ase/main.cc:93](../ase/main.cc#L93)

- Make project inspection reflect the live engine model. Alt+F12 opens a grep dialog using `match_serialized()`, but that
  currently depends on the empty snapshot implementation. Expose a read-only tree of projects, tracks, clips, plugins,
  properties, IDs, undo state, and object lifetimes, including the Tracktion peer of each ASE object, main-loop sources,
  WebSocket connections, and registered callbacks. Keep a diagnostic snapshot distinct from the persistence format unless
  the latter is complete and tested. [ui/startup.js:66](../ui/startup.js#L66),
  [ui/grepdialog.js:34](../ui/grepdialog.js#L34), [ase/project.cc:571](../ase/project.cc#L571)

- Add a thread-role assertion, for example `ASE_ASSERT_THREAD (main|audio|any)`, plus a debug-build guard that reports
  allocation or lock calls from the audio thread.

- In development builds, warn on notifications for unknown property names and on properties nobody observes.
  This catches `emit_notify` typos at runtime.

- Log exceptions from JUCE message callbacks with a cpptrace backtrace instead of letting them unwind the main loop.
  [ase/juce-linux.cc:78](../ase/juce-linux.cc#L78)

- Add ownership counts and UI state to failure reports. Include mounted components, listeners, timers, pending loads,
  active keymaps, modal stack, focus, DPR, and recent geometry assertions. Extend the existing DOM query helpers and
  source-mapped Electron console output. Capture a screenshot and relevant state when a browser test fails, before cleanup
  destroys the evidence. A `__DEV__` panel could show the same counts live. [ui/dom.js:175](../ui/dom.js#L175),
  [ui/b/contextmenu.tsx:156](../ui/b/contextmenu.tsx#L156), [electron/main.js:187](../electron/main.js#L187)

- Add a leak check: switch projects and relayout N times, then assert that object, listener, and timer counts on both
  sides return to baseline. [ui/tests/relayout_test.ts](../ui/tests/relayout_test.ts),
  [ui/tests/app_test.ts](../ui/tests/app_test.ts)

- Use existing fatal-warning and sanitizer modes as scheduled checks. `MODE=asan`, `ubsan`, `tsan`, and `lsan` already exist;
  run focused lifetime, malformed-input, and concurrent shutdown cases in addition to ordinary tests, for example from
  the nightly workflow. Account explicitly for intentional process-lifetime objects and custom allocators. Do not suppress
  whole directories to hide a new failure. [misc/config-uname.mk:56](../misc/config-uname.mk#L56),
  [ase/main.cc:144](../ase/main.cc#L144), [ase/server.cc:172](../ase/server.cc#L172),
  [.github/workflows/nightly.yml](../.github/workflows/nightly.yml)

## Review triggers

Require boundary-changing patches to state the invariant, owner, failure result, and behavioral test. Check generated
bindings, consumers, notifications, cleanup, and persistence together. Treat new `any` casts, lint disables, warning-flag
changes, sleeps, duplicated caches, dead `if (0)` code, unconditional prints, and vendored edits as review triggers.
Use a small reviewed exception list with a reason; do not turn every legacy occurrence into a mandatory rewrite.
The examples above provide the initial baseline.

## Validation

Validation for this audit was limited to source and recent-history inspection, an isolated Make expansion probe, and a
Node probe of the Jsonipc promise method with only its TypeScript signature removed. The Make probe returned
`origin=undefined files=[]` with exit status zero. The promise probe reported a pending outer promise and an unhandled
`audit rejection` after a 30 ms observation window. No application build or browser/audio suite was run.
Runtime impacts beyond those probes still need the proposed regression tests.
