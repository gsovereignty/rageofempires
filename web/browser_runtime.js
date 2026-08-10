Module['browserUncaughtErrors'] = [];
window.addEventListener('error', function (event) {
  Module['browserUncaughtErrors'].push({
    message: event.message || String(event.error || 'unknown error'),
    source: event.filename || '',
    line: event.lineno || 0,
    column: event.colno || 0,
    stack: event.error && event.error.stack || ''
  });
});
window.addEventListener('unhandledrejection', function (event) {
  Module['browserUncaughtErrors'].push({
    message: String(event.reason || 'unhandled rejection'),
    source: '',
    line: 0,
    column: 0,
    stack: event.reason && event.reason.stack || ''
  });
});
Module['canvas'] = document.getElementById('canvas');
Module['browserDisplayMetrics'] = function () {
  const rect = Module['canvas'].getBoundingClientRect();
  return {
    cssWidth: rect.width,
    cssHeight: rect.height,
    backingWidth: Module['canvas'].width,
    backingHeight: Module['canvas'].height,
    devicePixelRatio: window.devicePixelRatio,
    fullscreen: document.fullscreenElement !== null
  };
};
Module['canvas'].addEventListener('contextmenu', function (event) {
  event.preventDefault();
});
Module['preRun'] ??= [];
Module['preRun'].push(function () {
  Module['addRunDependency']('browser-storage');
  const ensureDirectory = function (path) {
    const existing = FS.analyzePath(path);
    if (!existing.exists) {
      FS.mkdir(path);
      return;
    }
    if (!FS.isDir(existing.object.mode)) {
      throw new Error('Browser storage path is not a directory: ' + path);
    }
  };
  ensureDirectory('/user');
  FS.mount(IDBFS, {}, '/user');
  FS.syncfs(true, function (error) {
    if (error) {
      Module['reportFailure']('Initial IndexedDB sync failed: ' + error);
      return;
    }
    for (const path of ['/user/settings', '/user/autosave']) {
      ensureDirectory(path);
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
  document.getElementById('fullscreen').hidden = false;
  Module['canvas'].focus({preventScroll: true});
  Module['callMain']([]);
}, {once: true});

document.getElementById('fullscreen').addEventListener(
  'pointerup',
  async function () {
    try {
      if (document.fullscreenElement) {
        await document.exitFullscreen();
      } else {
        await document.getElementById('browser-app').requestFullscreen();
      }
      Module['canvas'].focus({preventScroll: true});
    } catch (error) {
      Module['reportFailure']('Fullscreen transition failed: ' + error);
    }
  }
);
