// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
'use strict';

process.once ('loaded', () => {
  const { contextBridge, ipcRenderer } = require ('electron');
  const Electron = {
    async call (func, ...args) { return await ipcRenderer.invoke (func, args); },
  };
  contextBridge.exposeInMainWorld ('__Electron__', Electron);
});
