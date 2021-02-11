// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
'use strict';

process.once ('loaded', () => {
  // window.addEventListener ('message', e => console.log ("Message:", JSON.stringify (e.data)));
  const { ipcRenderer } = require ('electron');
  // R2M: postMessage ({ cmd: "func", args: [], ElectronIpcID: <int> });
  window.addEventListener ('message', event => {
    if (event.data?.ElectronIpcID)
      ipcRenderer.send ('ElectronIpcR2M', event.data);
  });
  // M2R: postMessage ({ ElectronIpcID: <int>, … });
  ipcRenderer.on ('ElectronIpcM2R', (event, data) => {
    if (data?.ElectronIpcID)
      window.postMessage (data);
  });
});
