// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

/** @class B-PROPGROUP
 * A property group contains a group title, several rows and each row contains a number of properties.
 * @property {string} name - Group name.
 * @property {Array<any>} props - List of properties with cached information and layout rows.
 * @property {boolean} readonly - Make this component non editable for the user.
 */

import { createMemo, For } from 'solid-js';
import { Toggle } from './toggle.tsx';
import { TextInput } from './textinput.tsx';
import { Knob } from './knob.tsx';
import { ChoiceInput } from './choiceinput.tsx';

// == STYLE ==
Extra_css`
@reference "../tailwind.css";
b-propgroup, .b-propgroup {
  @apply vflex;
  padding: 5px;
  justify-content: space-evenly;
  border-radius: var(--b-button-radius);
  background: var(--b-device-area1);
  &:nth-child(2n) {
    background: var(--b-device-area2);
  }
  .b-propgroup-title {
    @apply flex grow justify-center text-center;
  }
  --b-prop-width: 3rem;
  --b-prop-height: 2.5rem;
  --b-prop-gap: calc(2 * 0.125rem);
  .b-propgroup-row > * { @apply justify-center; }
  .b-propgroup-row > * + * { margin-left: var(--b-prop-gap); }
  .b-propgroup-row:not(:last-child) { margin-bottom: var(--b-prop-gap); }
}
.b-propgroup-row b-textinput, .b-propgroup-row .b-textinput {
  width: calc(var(--b-prop-gap) * 4 + 5 * var(--b-prop-width));
}
.b-propgroup-row {
  > * { width: var(--b-prop-width); }
  > span { margin-top: calc(2 * 0.125rem); }
}`;

function prop_case (prop)
{
  const hints = ':' + prop.hints_ + ':';
  if (hints.search (/:choice:/) >= 0)
    return 'C';		// choice
  if (hints.search (/:toggle:/) >= 0)
    return 'B';		// toggle
  if (hints.search (/:text:/) >= 0)
    return 'T';		// text
  if (prop.is_numeric_)
    return 'K';		// knob
  return '?';
}

function assign_layout_rows (props_array)
{
  // split properties into rows, according to lrow_
  const rows = [];
  for (const prop of props_array) {
    console.assert ('number' == typeof prop.lrow_);
    if (!rows[prop.lrow_])
      rows[prop.lrow_] = [];
    rows[prop.lrow_].push (prop);
  }
  // freezing avoids watchers
  return Object.freeze (rows);
}

function PropHtml (prop, readonly)
{
  switch (prop_case (prop)) {
    case 'B':
      return <Toggle disabled={prop.readonly || readonly} value={!!prop.value_.num}
        label={''}
        onValueChange={val => prop.set_normalized (!!val)} />;
    case 'C':
      return <ChoiceInput small={true} disabled={prop.readonly || readonly}
        label={prop.label_} title={prop.title_}
        value={prop.value_.val} prop={prop}
        onValueChange={uri => prop.apply_ (uri)} />;
    case 'K':
      return <Knob disabled={prop.readonly || readonly} prop={prop} />;
    case 'T':
      return <TextInput disabled={prop.readonly || readonly}
        prop={prop}
        label={prop.label_} title={prop.title_} />;
    default:
      return <span>{prop.nick_}</span>;
  }
}

// == COMPONENT ==
export function PropGroup (props)
{
  const prop_rows = createMemo (() => assign_layout_rows (props.props ?? []));
  return (
    <div class="b-propgroup" style={props.style}>
      <span class="b-propgroup-title"> {props.name} </span>
      <For each={prop_rows()}>
        {(row_props) => (
          <div class="b-propgroup-row grid justify-evenly" style="grid-auto-flow: column dense">
            <For each={row_props}>
              {(prop) => (
                <>
                  {PropHtml (prop, props.readonly)}
                  <span class="text-center text-[90%]" style="grid-row: 2/3"> {prop.nick_} </span>
                </>
              )}
            </For>
          </div>
        )}
      </For>
    </div>
  );
}
