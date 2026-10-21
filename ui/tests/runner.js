// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

export async function run_tests (suite, tests)
{
  if (!tests.length)
    throw new Error (`${suite}: no sub-tests registered`);
  const failures = [];
  for (const [name, fn] of tests) {
    try {
      if (await fn() === false)
        throw new Error ('returned false');
    } catch (e) {
      failures.push (`${name}: ${e?.message ?? e}`);
    }
  }
  if (failures.length)
    throw new Error (suite + ' failures:\n  ' + failures.join ('\n  '));
  return true;
}
