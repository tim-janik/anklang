import { For, type JSX } from 'solid-js';
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
  return <button uri={props.uri} ic={props.icon} kbd={props.kbd}
    disabled={props.disabled} class={props.class}>
    {props.children ?? props.label}
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
          <summary onKeyDown={event => {
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
