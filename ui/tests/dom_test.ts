// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { markdown_to_html } from '../dom';

export function test_markdown (): boolean
{
  const first = document.createElement ('div');
  const second = document.createElement ('div');
  first.className = 'existing';
  markdown_to_html (first, '[Link](https://example.org "Title") and https://example.net');
  const links = [...first.querySelectorAll ('a')];
  if (links.length !== 2 || links.some (a => a.target !== '_blank') || links[0].title !== 'Title')
    throw new Error ('markdown links lost their attributes or automatic linking');
  if (!first.classList.contains ('existing') || !first.classList.contains ('b-markdown-it-outer'))
    throw new Error ('markdown changed the container classes');
  const original = first.innerHTML;
  markdown_to_html (second, '**Bold** <script>bad()</script> [unsafe](javascript:bad)');
  if (first.innerHTML !== original || second.querySelector ('script, a') || second.querySelector ('strong')?.textContent !== 'Bold')
    throw new Error ('markdown renders interfered or allowed unsafe markup');
  markdown_to_html (first, 'replacement');
  if (first.querySelector ('a') || first.textContent?.trim() !== 'replacement')
    throw new Error ('markdown did not replace previous content');
  return true;
}
