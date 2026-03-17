// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** Test utility function for string manipulation
 */
export function test_string_ops(): boolean
{
  const str = "hello world";
  return str.length === 11;
}

/** Test number utilities
 */
export const test_number_ops = () =>
{
  const nums = [1, 2, 3, 4, 5];
  return nums.reduce ((a, b) => a + b, 0) === 15;
};

/** Test array utilities
 */
export async function test_array_ops(): Promise<boolean>
{
  const arr = [1, 2, 3];
  return arr.map (x => x * 2).reduce ((a, b) => a + b, 0) === 12;
}
