// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { run_tests } from './runner.js';
// == Test registry ==
const sub_tests: [string, () => Promise<any>][] = [];

/** Test utility function for string manipulation
 */
async function test_string_ops (): Promise<boolean>
{
  const str = "hello world";
  return str.length === 11;
}
sub_tests.push (['string_ops', test_string_ops]);

/** Test number utilities
 */
async function test_number_ops (): Promise<boolean>
{
  const nums = [1, 2, 3, 4, 5];
  return nums.reduce ((a, b) => a + b, 0) === 15;
}
sub_tests.push (['number_ops', test_number_ops]);

/** Test array utilities
 */
async function test_array_ops (): Promise<boolean>
{
  const arr = [1, 2, 3];
  return arr.map (x => x * 2).reduce ((a, b) => a + b, 0) === 12;
}
sub_tests.push (['array_ops', test_array_ops]);

export async function test_util (): Promise<boolean>
{
  return run_tests ('util', sub_tests);
}
