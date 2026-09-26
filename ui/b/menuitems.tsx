// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

import { createContext, createEffect, For, onCleanup, onMount, Show, untrack, useContext, type JSX } from 'solid-js';
import * as Kbd from '../kbd';
import * as Util from '../util';
import { Icon } from './icon';
import { MenuRow } from './menurow';
import { MenuSeparator } from './menuseparator';
import { MenuTitle } from './menutitle';

/** @class MenuItems
 * @description
 * Menu contents for a [ContextMenu](#ContextMenu), described as a list of entries:
 * actions (`uri`, `label`, `icon`, `kbd`), separators, titles, rows and submenus.
 * A [MenuItem](#MenuItem) renders one action; it can also be used directly with custom children.
 */

export type MenuAction = {
  type?: 'item';
  uri: string;
  label: string;
  icon?: string;
  kbd?: string;
  disabled?: boolean;
  class?: string;
  children?: JSX.Element;
};

export type MenuEntry = MenuAction
  | { type: 'separator' }
  | { type: 'title'; label: string }
  | { type: 'row'; items: MenuEntry[]; noturn?: boolean }
  | { type: 'submenu'; label: string; items: MenuEntry[]; open?: boolean };

export const MenuContext = createContext<{
  mapname: string;
  showicons: boolean;
  enabled: (uri: string) => boolean;
  check: (uri: string) => void;
  add_hotkey: (entry: Util.KeymapEntry) => () => void;
}>();

Extra_css`
.b-menu-submenu {
  display: flex; flex-direction: column;
  margin-left: 1.5rem;
  > summary {
    position: relative; list-style: none; font-weight: bold;
    &::before { position: absolute; left: -1.3rem; content: '►'; }
  }
  &[open] > summary::before { content: '▼'; }
}`;

export function MenuItem (props: MenuAction)
{
  const menu = useContext (MenuContext);
  let button: HTMLButtonElement;
  const shortcut = () => props.kbd ? Kbd.shortcut_lookup (menu?.mapname ?? '', props.label, props.kbd) : '';

  createEffect (() => {
    const key = shortcut();
    if (key && menu)
      onCleanup (menu.add_hotkey (new Util.KeymapEntry (key, button.click.bind (button), button)));
  });
  createEffect (() => {
    const uri = props.uri;
    untrack (() => menu?.check (uri));
  });
  onMount (() => {
    button.toggleAttribute ('turn', !!button.closest ('.b-menurow:not(.noturn)'));
    button.toggleAttribute ('noturn', !!button.closest ('.b-menurow.noturn'));
  });

  return <button ref={element => button = element} uri={props.uri} ic={props.icon} kbd={props.kbd} aria-label={props.label}
    disabled={props.disabled || (menu && !menu.enabled (props.uri))} class={props.class}
    onMouseEnter={event => event.currentTarget.focus()}>
    <Show when={props.icon && menu?.showicons !== false}>
      <Icon ic={props.icon} class="pointer-events-none" />
    </Show>
    {props.children ?? props.label}
    <Show when={props.kbd}><kbd class="pointer-events-none">{Util.display_keyname (shortcut())}</kbd></Show>
  </button>;
}

export function MenuItems (props: { items: MenuEntry[] })
{
  return <For each={props.items}>{entry => {
    switch (entry.type) {
      case 'separator':
        return <MenuSeparator />;
      case 'title':
        return <MenuTitle>{entry.label}</MenuTitle>;
      case 'row':
        return <MenuRow noturn={entry.noturn}><MenuItems items={entry.items} /></MenuRow>;
      case 'submenu':
        return <details class="b-menu-submenu" open={entry.open}>
          <summary onMouseEnter={event => event.currentTarget.focus()} onKeyDown={event => {
            if (event.key === 'ArrowRight' || event.key === 'ArrowLeft') {
              (event.currentTarget.parentElement as HTMLDetailsElement).open = event.key === 'ArrowRight';
              event.preventDefault();
              event.stopPropagation();
            }
          }}>{entry.label}</summary>
          <MenuItems items={entry.items} />
        </details>;
      default:
        return <MenuItem {...entry} />;
    }
  }}</For>;
}
