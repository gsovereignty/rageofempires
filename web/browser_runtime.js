Module['browserUncaughtErrors'] = [];
Module['browserDiagnostics'] = [];
const diagnosticValue = function (value) {
  if (value instanceof Error) {
    return {name: value.name, message: value.message, stack: value.stack || ''};
  }
  if (typeof value === 'string' || typeof value === 'number' ||
      typeof value === 'boolean' || value === null || value === undefined) {
    return value === undefined ? 'undefined' : value;
  }
  try {
    return JSON.parse(JSON.stringify(value));
  } catch (_) {
    return String(value);
  }
};
const recordDiagnostic = function (level, values) {
  Module['browserDiagnostics'].push({
    elapsedMilliseconds: Math.round(performance.now()),
    level,
    values: Array.from(values, diagnosticValue)
  });
  if (Module['browserDiagnostics'].length > 1000) {
    Module['browserDiagnostics'].splice(0, 100);
  }
};
for (const level of ['error', 'warn']) {
  const original = console[level].bind(console);
  console[level] = function (...values) {
    recordDiagnostic(level, values);
    original(...values);
  };
}
window.addEventListener('error', function (event) {
  const error = {
    message: event.message || String(event.error || 'unknown error'),
    source: event.filename || '',
    line: event.lineno || 0,
    column: event.colno || 0,
    stack: event.error && event.error.stack || ''
  };
  Module['browserUncaughtErrors'].push(error);
  recordDiagnostic('uncaught-error', [error]);
}, true);
window.addEventListener('unhandledrejection', function (event) {
  const error = {
    message: String(event.reason || 'unhandled rejection'),
    source: '',
    line: 0,
    column: 0,
    stack: event.reason && event.reason.stack || ''
  };
  Module['browserUncaughtErrors'].push(error);
  recordDiagnostic('unhandled-rejection', [error]);
});
Module['canvas'] = document.getElementById('canvas');
Module['browserDisplayMetrics'] = function () {
  const rect = Module['canvas'].getBoundingClientRect();
  const computed = getComputedStyle(Module['canvas']);
  return {
    cssWidth: rect.width,
    cssHeight: rect.height,
    backingWidth: Module['canvas'].width,
    backingHeight: Module['canvas'].height,
    devicePixelRatio: window.devicePixelRatio,
    fullscreen: document.fullscreenElement !== null,
    fullscreenElement: document.fullscreenElement?.id || null,
    windowInnerWidth: window.innerWidth,
    windowInnerHeight: window.innerHeight,
    windowOuterWidth: window.outerWidth,
    windowOuterHeight: window.outerHeight,
    visualViewport: window.visualViewport ? {
      width: window.visualViewport.width,
      height: window.visualViewport.height,
      scale: window.visualViewport.scale
    } : null,
    canvasInlineStyle: Module['canvas'].getAttribute('style') || '',
    canvasComputedStyle: {
      display: computed.display,
      width: computed.width,
      height: computed.height
    },
    documentHidden: document.hidden
  };
};
Module['browserDisplayHistory'] = [];
const recordDisplay = function (event) {
  Module['browserDisplayHistory'].push({
    elapsedMilliseconds: Math.round(performance.now()),
    event,
    display: Module['browserDisplayMetrics']()
  });
  if (Module['browserDisplayHistory'].length > 200) {
    Module['browserDisplayHistory'].shift();
  }
};
window.addEventListener('resize', function () { recordDisplay('resize'); });
window.addEventListener('fullscreenchange', function () {
  recordDisplay('fullscreenchange');
});
document.addEventListener('visibilitychange', function () {
  recordDisplay('visibilitychange');
});
new ResizeObserver(function () { recordDisplay('canvas-resize'); })
  .observe(Module['canvas']);
recordDisplay('diagnostics-installed');
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
  recordDiagnostic('reported-failure', [reason]);
  document.getElementById('loading').hidden = false;
  document.getElementById('start').hidden = true;
  Module['canvas'].hidden = true;
  Module['setStatus']('Browser startup failed: ' + reason);
};
Module['onAbort'] = Module['reportFailure'];

document.getElementById('diagnostics').addEventListener('click', function () {
  const canvas = Module['canvas'];
  const report = {
    capturedAt: new Date().toISOString(),
    page: location.href,
    userAgent: navigator.userAgent,
    display: Module['browserDisplayMetrics'](),
    displayHistory: Module['browserDisplayHistory'],
    bodyText: document.body.innerText,
    canvasHidden: canvas.hidden,
    runtimeCalled: Boolean(Module.calledRun),
    storageReady: Boolean(Module.storageReady),
    persistence: {
      status: Module.persistenceSyncStatus || null,
      error: Module.persistenceSyncError || null
    },
    uncaughtErrors: Module['browserUncaughtErrors'],
    diagnostics: Module['browserDiagnostics'],
    lifecycle: Module.browserLifecycle || null,
    telemetry: Module.browserTelemetry || null,
    audioTelemetry: Module.browserAudioTelemetry || null,
    resources: performance.getEntriesByType('resource').map(function (entry) {
      return {
        name: entry.name,
        duration: entry.duration,
        transferSize: entry.transferSize,
        decodedBodySize: entry.decodedBodySize
      };
    })
  };
  const blob = new Blob([JSON.stringify(report, null, 2) + '\n'], {
    type: 'application/json'
  });
  const link = document.createElement('a');
  link.href = URL.createObjectURL(blob);
  link.download = 'aoe-browser-diagnostics-' +
    new Date().toISOString().replaceAll(':', '-') + '.json';
  link.click();
  setTimeout(function () { URL.revokeObjectURL(link.href); }, 0);
});

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
