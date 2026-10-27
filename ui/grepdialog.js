// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

const HTML = () => `
Grep Pattern: <br>
<input id="g-grepspattern" type="text" size="50"/> <br>
Grep Result: <br>
<hr>
<pre id="g-grepresult" style="min-height:12rem">

</pre>
`;

let grep_dialog;
export function hotkey_toggle()
{
  if (grep_dialog?.open) {
    grep_dialog.close();
    grep_dialog = null;
    return;
  }
  if (!grep_dialog) {
    grep_dialog = document.createElement ('DIALOG');
    grep_dialog.className = "p-4";
    grep_dialog.innerHTML = HTML ();
    document.body.append (grep_dialog);
    const pinput = /**@type{HTMLInputElement}*/ (grep_dialog.querySelector ('#g-grepspattern'));
    pinput.onchange = devgreps_pattern_change;
  }
  grep_dialog.showModal();
}

async function devgreps_pattern_change()
{
  const pinput = grep_dialog.querySelector ('#g-grepspattern');
  const resultelement = grep_dialog.querySelector ('#g-grepresult');
  const pattern = pinput.value;
  resultelement.innerText = '\n';
  if (pattern && App?.project) {
    let r = App.project.grep_serialized (pattern, -1);
    r = await r;
    resultelement.innerText = r;
  }
}
