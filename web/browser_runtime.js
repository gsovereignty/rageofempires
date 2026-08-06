Module['canvas'] = document.getElementById('canvas');
Module['preRun'] ??= [];
Module['preRun'].push(function () {
  Module['addRunDependency']('browser-storage');
  try { FS.mkdir('/user'); } catch (error) {
    if (!String(error).includes('File exists')) throw error;
  }
  FS.mount(IDBFS, {}, '/user');
  FS.syncfs(true, function (error) {
    if (error) {
      Module['reportFailure']('Initial IndexedDB sync failed: ' + error);
      return;
    }
    for (const path of ['/user/settings', '/user/autosave']) {
      try { FS.mkdir(path); } catch (mkdirError) {
        if (!String(mkdirError).includes('File exists')) throw mkdirError;
      }
    }
    Module['storageReady'] = true;
    Module['removeRunDependency']('browser-storage');
  });
});
Module['setStatus'] = function (message) {
  const status = document.getElementById('status');
  if (status) status.textContent = message || '';
};
Module['onRuntimeInitialized'] = function () {
  document.getElementById('loading').hidden = true;
  document.getElementById('start').hidden = false;
};
Module['reportFailure'] = function (reason) {
  document.getElementById('loading').hidden = false;
  document.getElementById('start').hidden = true;
  Module['canvas'].hidden = true;
  Module['setStatus']('Browser startup failed: ' + reason);
};
Module['onAbort'] = Module['reportFailure'];

document.getElementById('start').addEventListener('pointerup', function () {
  if (!Module['storageReady']) return;
  this.hidden = true;
  Module['canvas'].hidden = false;
  Module['canvas'].focus({preventScroll: true});
  Module['callMain']([]);
}, {once: true});
