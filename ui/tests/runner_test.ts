// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { run_tests } from './runner.js';

export async function test_runner (): Promise<boolean>
{
  const order: string[] = [];
  const result = await run_tests ('success', [
    ['first', async () => { await Promise.resolve(); order.push ('first'); }],
    ['second', () => { order.push ('second'); }],
    ['true', () => true],
  ]);
  if (result !== true || order.join (',') !== 'first,second')
    throw new Error ('runner did not await tests in order');

  let message = '';
  try {
    await run_tests ('failure', [
      ['error', async () => { throw new Error ('broken'); }],
      ['string', () => { throw 'rejected'; }],
      ['null', () => { throw null; }],
      ['false', () => false],
      ['async_false', async () => false],
      ['last', () => { order.push ('last'); }],
    ]);
  } catch (e) {
    message = (e as Error).message;
  }
  const expected = 'failure failures:\n  error: broken\n  string: rejected\n  null: null' +
    '\n  false: returned false\n  async_false: returned false';
  if (message !== expected || order.at (-1) !== 'last')
    throw new Error ('runner did not collect failures and continue');

  message = '';
  try {
    await run_tests ('empty', []);
  } catch (e) {
    message = (e as Error).message;
  }
  if (message !== 'empty: no sub-tests registered')
    throw new Error ('runner accepted an empty suite');
  return true;
}
