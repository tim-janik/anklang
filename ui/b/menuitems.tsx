import { createContext, createEffect, createSignal, For, onCleanup, onMount, Show, useContext, type JSX } from 'solid-js';
import * as Kbd from '../kbd';
import * as Util from '../util';
import { Icon } from './icon';
import { MenuRow } from './menurow';
import { MenuSeparator } from './menuseparator';
import { MenuTitle } from './menutitle';

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
  isactive: (uri: string) => boolean | Promise<boolean>;
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
  const [active, set_active] = createSignal (true);
  let button: HTMLButtonElement;
  const shortcut = () => props.kbd ? Kbd.shortcut_lookup (menu?.mapname ?? '', props.label, props.kbd) : '';
  const check_isactive = async (apply = true) => {
    const enabled = !props.disabled && (!menu || await menu.isactive (props.uri));
    if (apply)
      set_active (enabled);
    return enabled;
  };

  createEffect (() => {
    const key = shortcut();
    if (key && menu)
      onCleanup (menu.add_hotkey (new Util.KeymapEntry (key, button.click.bind (button), button)));
  });
  onMount (() => {
    button.toggleAttribute ('turn', !!button.closest ('.b-menurow:not(.noturn)'));
    button.toggleAttribute ('noturn', !!button.closest ('.b-menurow.noturn'));
  });

  return <button ref={element => {
    button = element;
    (element as any).check_isactive = check_isactive;
    (element as any).set_menu_active = set_active;
  }} uri={props.uri} ic={props.icon} kbd={props.kbd} aria-label={props.label}
    disabled={props.disabled || !active()} class={props.class}
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
