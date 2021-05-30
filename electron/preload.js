// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
'use strict';

process.once ('loaded', () => {
  const { contextBridge, ipcRenderer } = require ('electron');
  const Electron = {
    async call (func, ...args) { return await ipcRenderer.invoke (func, args); },
  };
  contextBridge.exposeInMainWorld ('__Electron__', Electron);
});
