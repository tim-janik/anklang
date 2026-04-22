// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

const fs = require ('fs');
const path = require ('path');
const { TraceMap, originalPositionFor } = require ('@jridgewell/trace-mapping');

// Resolve source map and trace location back to original source
function resolve_source_location (source_id, line_number)
{
  try {
    // Extract URL path from source_id (e.g., http://localhost:12345/assets/test.js -> /assets/test.js)
    const url_match = source_id.match (/^https?:\/\/[^\/]+(\/.*)$/);
    if (!url_match)
      return null;
    const url_path = url_match[1]; // e.g., /assets/testcalls-CMjwyc_b.js

    // Determine the base directory for UI files
    // The htmlgui executable is in <builddir>/electron/, and UI files are in <builddir>/ui/
    const electron_dir = path.dirname (process.execPath);
    const ui_dir = path.join (path.dirname (electron_dir), 'ui');

    // Construct the full file path
    const js_path = path.join (ui_dir, url_path);
    const map_path_gz = js_path + '.map.gz';
    const map_path = js_path + '.map';

    let map_data = null;
    let map_path_used = '';

    // Try .map.gz first, then .map
    if (fs.existsSync (map_path_gz)) {
      const zlib = require ('zlib');
      map_data = zlib.gunzipSync (fs.readFileSync (map_path_gz));
      map_path_used = map_path_gz;
    } else if (fs.existsSync (map_path)) {
      map_data = fs.readFileSync (map_path);
      map_path_used = map_path;
    }
    if (!map_data)
      return null;

    // Parse and use the sourcemap
    const map_json = JSON.parse (map_data.toString ('utf8'));
    const tracer = new TraceMap (map_json);
    const traced = originalPositionFor (tracer, { line: line_number, column: 0 });

    if (traced.source) {
      // Resolve the source path relative to the sourcemap file location
      const map_dir = path.dirname (map_path_used);
      let resolved_source = path.resolve (map_dir, traced.source);
      // Normalize path separators and clean up relative paths
      resolved_source = resolved_source.replace (/\/\.\//g, '/');
      return {
        source: resolved_source,
        line: traced.line,
        column: traced.column,
        name: traced.name || undefined
      };
    }
  } catch (e) {
    // Ignore errors in source map resolution
  }
  return null;
}

// Resolve a single V8 stack-frame line to original source via sourcemap.
function resolve_single_frame (line)
{
  // Match: " at [async] funcName (url:line:col)"
  const m = line.match (/^\s*at\b([^()]*)\((.+\/.+\..+?):(\d+):(\d+)\)$/);
  if (m) {
    const funcName = m[1].trim ();
    const url = m[2];
    const frameLine = parseInt (m[3], 10);
    const frameCol = parseInt (m[4], 10);
    const resolved = resolve_source_location (url, frameLine);
    if (resolved) {
      let loc;
      if (resolved.column > 0)
        loc = `${resolved.source}:${resolved.line}:${resolved.column}`;
      else
        loc = `${resolved.source}:${resolved.line}`;
      return `    at ${funcName} (${loc})`;
    }
  }
  return null; // not a resolvable stack frame
}

// Returns the message with resolved frames
function resolve_stack_frames (message)
{
  const lines = message.split ('\n');
  const resolved = lines.map (line => resolve_single_frame (line) || line);
  return resolved.join ('\n');
}

module.exports = { resolve_source_location, resolve_stack_frames };
