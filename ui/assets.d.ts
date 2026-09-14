// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Keep this file a script because wildcard module declarations only apply from ambient declaration files.
declare module "*.svg" {
  const src: string;
  export default src;
}
