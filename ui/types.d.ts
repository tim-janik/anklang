// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Allow JavaScript modules as Any
// declare module "*";

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

export {};
