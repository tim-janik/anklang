// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import * as fs from 'fs';
import * as path from 'path';

const istty = process.stderr.isTTY;
const oprint = (...args) => process.stdout.write (`${args.join (' ')}\n`);
const eprint = (...args) => process.stderr.write (`${args.join (' ')}\n`);

// Configure checks
interface Checks { [key: string]: boolean; }
const checks: Checks = {
  'ban-printf':                    true,
  'separate-body':                 true,
  'whitespace-before-parenthesis': true,
  'whitespace-after-parenthesis':  true,
  'ban-fixme':                     true,
  'ban-todo':                      false,
};

// Detect various code smells via regexp
function lineMatcher (code: string,
		      text: string,
		      has_comment: boolean,
		      orig: string,
		      filename: string)
{
  let error = '', warning = '', offset = 0;
  let m: RegExpMatchArray | null;

  // ban-printf
  if (checks['ban-printf'] && (m = code.match (/\bd?printf\s*\(/))) {
    offset = m.index;
    warning = 'invalid call to printf, use printout()';
  }
  // whitespace-before-parenthesis
  else if (checks['whitespace-before-parenthesis'] && code.indexOf ('#') < 0 &&
	   (m = code.match (/\s\w+[a-z0-9]\([^)]/i))) {
    if (!/\s\.|:\s|@import/.test (code) &&			// ignore funcs in CSS rules
	!filename.endsWith ('css')) {
      offset = m.index + m[0].length - 2;
      warning = 'missing whitespace before parenthesis';
    }
  }
  // whitespace-after-parenthesis
  else if (checks['whitespace-after-parenthesis'] && (m = code.match (/\)[a-z0-9]/i))) {
    if (!any_in (['(/', ',/', '/,', '/)'], code)) {		// ignore JS regexp
      offset = m.index + m[0].length - 2;
      warning = 'missing whitespace after parenthesis';
    }
  }
  // ban-fixme
  else if (checks['ban-fixme'] && has_comment && (m = text.match (/\bFI[X]ME\b/i))) {
    offset = m.index;
    warning = 'comment indicates unfinished code';
  }
  // ban-todo
  else if (checks['ban-todo'] && has_comment && (m = text.match (/\bTO[D]O\b/i))) {
    offset = m.index;
    error = 'comment indicates open issues';
  }
  // separate-body (newline before function body)
  else if (checks['separate-body'] && (m = code.match (/^\s*[\w <:,>]+\s*\((.+\s+.+)?\)[\s\w]*{\s*(\/[/*].*)?$/))) {
    offset = m.index + m[0].indexOf ('{');
    let ignore : boolean;
    ignore   = /\]\s*\(/.test (code);				// ignore lambda
    ignore ||= /\balignas\s*\(/.test (code);			// ignore alignas()
    ignore ||= /do|switch|while|for|if|namespace/.test (code);	// ignore blocks
    if (!ignore)
      warning = 'missing newline before function body';
  }
  // print diagnostics
  if (error || warning) {
    const lc = count (orig, '\n'), lcs = lc.toString().padStart (5);
    const B = istty ? '\x1b[1m'  : ''; // bold
    const f = istty ? '\x1b[22m' : ''; // faint/dim
    const R = istty ? '\x1b[31m' : ''; // Red
    const G = istty ? '\x1b[32m' : ''; // Green
    const M = istty ? '\x1b[35m' : ''; // Magenta
    const Z = istty ? '\x1b[0m'  : ''; // Reset
    let msg = `${B}${filename}:${lc}: `;
    msg += error ? `${R}error:${Z}` : `${M}warning:${Z}`;
    msg += ` ${error || warning}${f}`;
    eprint (msg);
    text = text.trimRight().replace (/.*\n/, '');
    eprint (`${lcs} | ${text}`);
    const indent = text.slice (0, offset).replace (/[^ \t]/g, ' ');
    eprint (`${lcs} | ${indent}${B}${G}^${Z}`);
  }
}

// Count chars in a string
function count (str: string, w: string): number
{
  let c = 0;
  for (let i = 0; i < str.length; i++)
    if (str[i] === w)
      c++;
  return c;
}

// True iff `haystack` includes any element of `needles`
function any_in (needles: string[], haystack: string): boolean {
  return needles.some (needle => haystack.includes (needle));
}

// First disguise C strings and comments, then invoke matcher predicate per line
class CLexer {
  filename: string;
  sq = false;
  dq = false;
  bs = false;
  slc = false;
  mlc = false;
  orig = '';
  out = '';
  last = '';
  commentline = false;
  linematcher: typeof lineMatcher;
  constructor (fname: string, linematcher: typeof lineMatcher)
  {
    this.filename = fname;
    this.linematcher = linematcher;
  }
  feed_char (c: string)
  {
    this.orig += c;
    if (this.last === '\\' && (this.sq || this.dq)) {	// backslash inside string
      this.out += c; this.last = ' '; return;
    }
    if (this.sq) {					// single quote string
      if (c === "'") {
        this.sq = false;
        return this.append (c, c);
      }
      return this.append ('_', c);
    }
    if (this.dq) {					// double quote string
      if (c === '"') {
        this.dq = false;
        return this.append (c, c);
      }
      return this.append ('_', c);
    }
    if (this.mlc) {					// multi line comment
      if (c === '/' && this.last === '*') {
        this.mlc = false;
        this.out = this.out.slice (0, -1) + '*';
        return this.append (c, c);
      }
      if (c === '\n')
        return this.append (c, c);
      return this.append ('_', c);
    }
    if (this.slc) {					// single line comment
      if (c === '\n') {
        this.slc = false;
        return this.append (c, c);
      }
      return this.append ('_', c);
    }
    if (this.last === '/' && c === '/') {		// start single line comment
      this.slc = true;
      return this.append (c, c);
    }
    if (this.last === '/' && c === '*') {		// start multi line comment
      this.mlc = true;
      return this.append (c, ' ');
    }
    if (c === '"') {					// start double quote string
      this.dq = true;
      return this.append (c, c);
    }
    if (c === "'") {					// start single quote string
      this.sq = true;
      return this.append (c, c);
    }
    if (/\s/.test (c))					// whitespace
      return this.append (c, c);
    return this.append (c, c);
  }
  feed (txt: string)
  {
    for (const c of txt)
      this.feed_char (c);
  }
  append (char: string, last: string, is_commentline = false)
  {
    this.commentline = this.commentline || this.mlc || this.slc;
    if (char === '\n') {
      const was_commentline = this.commentline;
      this.commentline = this.mlc || this.slc;
      const p1 = this.out.lastIndexOf ('\n') + 1;	// BOL
      this.linematcher (this.out.slice (p1), this.orig.slice (p1), was_commentline, this.orig, this.filename);
    }
    this.out += char;
    this.last = last;
  }
}

// CLI help
function print_help()
{
  const prog = path.basename (process.argv[0]);
  oprint (`Usage: ${prog} [Options] {ccfile}`);
  oprint ('Check syntax for code smells, based on simple regular expression.');
  oprint ('Options:');
  oprint ('  --help, -h                Print this help message');
  oprint ('  --all                     Enable all checks');
  oprint ('  --none                    Disable all checks');
  oprint ('  --list                    Show check configuration');
  oprint ('  --<check-name>=1          Enable check');
  oprint ('  --<check-name>=0          Disable check');
}

// main program
function main(): number
{
  const checks_arg = (arg: string): [string, number] | null => {
    if (!arg.startsWith ('--')) return null;
    const eq = arg.indexOf ('=');
    if (eq < 3) return null;
    const check_name = arg.slice (2, eq);
    if (!(check_name in checks)) return null;
    return [check_name, parseInt (arg[eq + 1])];
  };
  for (const arg of process.argv.slice (2)) {
    let kv;
    if (arg === '--help' || arg === '-h') {
      print_help();
      return 0;
    } else if (kv = checks_arg (arg)) {
      checks[kv[0]] = Boolean (kv[1]);
    } else if (arg === '--all') {
      for (const k in checks) checks[k] = true;
    } else if (arg === '--none') {
      for (const k in checks) checks[k] = false;
    } else if (arg === '--list') {
      for (const k in checks)
        oprint (`  --${k}=${Number (checks[k])}`);
    } else {
      const clex = new CLexer (arg, lineMatcher);
      clex.feed (fs.readFileSync (arg, 'utf-8'));
    }
  }
  if (process.argv.length <= 2)
    print_help();
  return 0;
}
process.exit (main());
