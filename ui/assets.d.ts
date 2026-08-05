// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Asset imports are bundled by esbuild; tsc only needs the module shape.
// NOTE: this file must stay a script (no imports/exports), because wildcard
// module declarations only apply from ambient (non-module) declaration files.
declare module "*.svg" {
  const src: string;
  export default src;
}
