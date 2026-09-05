// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Allow JavaScript modules as Any
// declare module "*";

// SolidJS JSX conventions used across ui/b/ (contextmenu menu items carry
// uri/ic/kbd attributes; legacy custom elements are being phased out).
declare module "solid-js" {
  namespace JSX {
    interface IntrinsicElements {
      /** @deprecated use TreeBrowser component (SolidJS migration) */
      "b-treebrowser": HTMLAttributes<HTMLElement> & {
        tree?: any;
        expandall?: boolean;
      };
      button: ButtonHTMLAttributes<HTMLButtonElement> & {
        uri?: string;
        ic?: string;
        kbd?: string;
      };
    }
  }
}

// Allow certain globals
declare global {
  var App: any;
  var Ase: any;
  var Shell: any;
  var CONFIG: any;
  var __DEV__: Boolean;
  var debug: Function;
  var assert: Function;
  var _: Function;
  var Extra_css: (...args: any[]) => undefined;

  interface ViewTransition {
    readonly finished: Promise<void>;
    readonly ready: Promise<void>;
    readonly updateCallbackDone: Promise<void>;
  }
  interface Document {
    startViewTransition(setupPromise: () => Promise<void> | void): ViewTransition;
  }
}

// Custom attributes for intrinsic elements, e.g. <button uri=... ic=... kbd=.../> menu items,
// see ui/b/contextmenu.tsx: MenuRow
declare module "solid-js" {
  namespace JSX {
    interface ButtonHTMLAttributes<T> {
      uri?: string | undefined;
      ic?: string | undefined;
      kbd?: string | undefined;
    }
  }
}

export {};
