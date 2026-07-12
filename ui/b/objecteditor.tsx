// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
// @ts-check

/** @class ObjectEditor
 * @description
 * The ObjectEditor component is a field-editor for object input.
 * A copy of the input value is edited; changes are applied directly through
 * each extended property's `apply_` callback (as triggered by `valuechange`
 * events from the child input widgets).
 * ### Props:
 * *value*
 * : Object with properties to be edited.
 * *readonly*
 * : Make this component non editable for the user.
 * *augment*
 * : Function to augment each property, called with each extended property.
 */

import { createSignal, createEffect, onCleanup, For } from 'solid-js';
import * as Util from '../util.js';

// <STYLE/>
Extra_css`
b-objecteditor, .b-objecteditor {
  display: grid;
  grid-gap: 0.6em 0.5em;
  .b-objecteditor-clear {
    -font-size: 1.1em; /* @include b-font-weight-bolder(); */
    color: #888; background: none; padding: 0 0 0 0.5em; margin: 0;
    outline: none; border: 1px solid rgba(0 0 0 / 0); border-radius: var(--b-button-radius);
    &:hover			{ color: #eb4; }
    &:active			{ color: #3bf; }
  }
  > * {
    /* avoid visible overflow for worst-case resizing */
    min-width: 0;
    overflow-wrap: break-word;
  }
  .b-objecteditor-group {
    align-items: center;
    margin-top: 0.5em;
    &:first-child { margin-top: 0; }
    white-space: nowrap;
    .b-objecteditor-label { text-overflow: ellipsis; overflow: hidden; @include b-font-weight-bold(); }
  }
  .b-objecteditor-flabel {
    padding: 0 0.1em 0 0;
    min-width: 3em;
    white-space: nowrap;
    text-overflow: ellipsis;
    overflow: hidden;
  }
  .b-objecteditor-field {
    justify-self: flex-end;
    justify-content: space-between;
    white-space: nowrap;
    max-width: 32em;
    width: 50vw;
  }
  .b-objecteditor-value {
    width: 100%;
  }
}`;

// <COMPONENT/>
export function ObjectEditor (props)
{
  const [gprops, set_gprops] = createSignal ([]);
  let gen = 0;
  let disconnectors = [];

  createEffect (() => {
    const val = props.value;
    const my_gen = ++gen;
    // cleanup old disconnectors
    if (disconnectors.length) {
      while (disconnectors.length)
        disconnectors.pop().call();
      disconnectors = [];
    }
    if (!val || !val.length) {
      set_gprops ([]);
      return;
    }
    (async () => {
      const { grouplist, disconnectors: new_disconnectors } = await list_fields_ (val);
      if (my_gen !== gen) {
        new_disconnectors.forEach (cb => cb());
        return;
      }
      disconnectors = new_disconnectors;
      set_gprops (grouplist);
    })();
  });

  onCleanup (() => {
    gen++; // invalidate pending async
    if (disconnectors.length) {
      while (disconnectors.length)
        disconnectors.pop().call();
      disconnectors = [];
    }
  });

  async function list_fields_ (proplist)
  {
    const new_disconnectors = [];
    const groups = {};
    const pending_xprops = [];

    for (const prop of proplist) {
      const augment = async xprop => {
        if (props.augment)
          await props.augment (xprop);
      };
      pending_xprops.push (Util.extend_property (prop, cb => new_disconnectors.push (cb), augment));
    }

    for (const pending_prop of pending_xprops) {
      const xprop = await pending_prop;
      if (!groups[xprop.group_])
        groups[xprop.group_] = [];
      groups[xprop.group_].push (xprop);
    }

    const grouplist = []; // [ { name, props: [richprop, ...] }, ... ]
    for (const k of Object.keys (groups)) {
      grouplist.push ({ name: k, props: groups[k] });
      Object.freeze (groups[k]);
    }

    return { grouplist, disconnectors: new_disconnectors };
  }

  function render_input (prop)
  {
    if (prop.hints_.search (/:range:/) >= 0) {
      return (
        <b-numberinput
          class={"b-objecteditor--" + prop.ident_}
          value={prop.value_.val}
          on:valuechange={e => prop.apply_ (e.target.value)}
          min={prop.min_}
          max={prop.max_}
          readonly={props.readonly}
        />
      );
    } else if (prop.hints_.search (/:bool:/) >= 0) {
      return (
        <b-switchinput
          class={"b-objecteditor--" + prop.ident_}
          value={prop.value_.val}
          on:valuechange={e => prop.apply_ (e.target.value)}
          readonly={props.readonly}
        />
      );
    } else if (prop.has_choices_) {
      return (
        <b-choiceinput
          class={"b-objecteditor--" + prop.ident_}
          value={prop.value_.val}
          on:valuechange={e => prop.apply_ (e.target.value)}
          title={prop.title_}
          choices={prop.value_.choices}
          prop={prop}
          disabled={props.readonly}
        />
      );
    } else {
      return (
        <b-textinput
          class={"b-objecteditor--" + prop.ident_}
          prop={prop}
          readonly={props.readonly}
        />
      );
    }
  }

  return (
    <div class="b-objecteditor">
      <For each={gprops()}>
        {(group) => (
          <>
            <div class="hflex b-objecteditor-group" style="grid-column: 1 / span 3">
              <span class="b-objecteditor-label" style="flex-grow: 0">
                {group.name}
              </span>
              <hr style="flex-grow: 1; margin-left: 0.5em; min-width: 5em" />
            </div>
            <For each={group.props}>
              {(prop) => (
                <>
                  <span class="b-objecteditor-flabel" style="grid-column: 1"
                        data-bubble={prop.descr_ || prop.blurb_}>
                    {prop.label_}
                  </span>
                  <div class="hflex b-objecteditor-field" style="grid-column: 2 / span 2">
                    <span class="b-objecteditor-value"
                          data-bubble={prop.blurb_ || prop.descr_}
                          style="text-align: right">
                      {render_input (prop)}
                    </span>
                    <span>
                      <span class="b-objecteditor-clear"
                            onClick={e => prop.reset()}
                            data-bubble={"Reset " + prop.label_}>
                        {' '}⊗{'  '}
                      </span>
                    </span>
                  </div>
                </>
              )}
            </For>
          </>
        )}
      </For>
    </div>
  );
}
