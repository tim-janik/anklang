// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Allow JavaScript modules as Any
// declare module "*";

// Allow certain globals
declare global {
  var App: any;
  var Ase: any;
  var Data: any;
  var Shell: any;
  var __DEV__: bool;
}

export {};
