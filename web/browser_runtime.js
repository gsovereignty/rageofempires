Module['browserUncaughtErrors'] = [];
// The separately bundled Applesauce adapter resolves the Emscripten module
// through globalThis when its asynchronous callbacks cross into WASM.
globalThis.Module = Module;
Module['browserDiagnostics'] = [];
const recordUncaughtError = function (error) {
  Module['browserUncaughtErrors'].push(error);
  if (Module['browserUncaughtErrors'].length > 200) {
    Module['browserUncaughtErrors'].splice(
      0, Module['browserUncaughtErrors'].length - 200
    );
  }
};
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
  recordUncaughtError(error);
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
  recordUncaughtError(error);
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
  try {
    const relayConfig = JSON.parse(
      FS.readFile('/resources/nostr-relays.json', {encoding: 'utf8'})
    );
    if (!Array.isArray(relayConfig.relays) || relayConfig.relays.length !== 20 ||
        relayConfig.relays.some(function (relay) {
          return typeof relay !== 'string' || !relay.startsWith('wss://');
        }) || new Set(relayConfig.relays).size !== relayConfig.relays.length) {
      throw new Error('canonical relay pool must contain 20 unique wss URLs');
    }
    document.getElementById('relays').value = relayConfig.relays.join(',');
    Module['canonicalNostrRelays'] = relayConfig.relays;
  } catch (error) {
    Module['reportFailure']('Canonical Nostr relay configuration failed: ' + error);
    return;
  }
  document.getElementById('loading').hidden = true;
  document.getElementById('launch').hidden = false;
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

const nostrSession = document.getElementById('nostr-session');
const nostrSessionDetails = document.getElementById('nostr-session-details');
const toggleNostrSessionDetails = document.getElementById(
  'toggle-nostr-session-details'
);
const nostrPublicKey = document.getElementById('nostr-public-key');
const nostrPublicReference = document.getElementById('nostr-public-reference');
const copyMatchReference = document.getElementById('copy-match-reference');
const copyMatchStatus = document.getElementById('copy-match-status');
const relayControls = document.getElementById('relay-controls');

toggleNostrSessionDetails.addEventListener('click', function () {
  nostrSessionDetails.hidden = !nostrSessionDetails.hidden;
  const expanded = !nostrSessionDetails.hidden;
  toggleNostrSessionDetails.setAttribute('aria-expanded', String(expanded));
  toggleNostrSessionDetails.textContent = expanded
    ? 'Hide match details' : 'Show match details';
  Module['canvas'].focus({preventScroll: true});
});

relayControls.addEventListener('click', function (event) {
  const button = event.target.closest('button[data-relay]');
  if (!button || !globalThis.AoeNostrRuntime) return;
  const enabled = button.dataset.enabled !== 'true';
  globalThis.AoeNostrRuntime.setRelayEnabled(button.dataset.relay, enabled);
  Module['canvas'].focus({preventScroll: true});
});

const refreshRelayControls = function (value) {
  const relays = value?.relays || [];
  const disabled = new Set(value?.disabledRelays || []);
  const signature = JSON.stringify([relays, [...disabled]]);
  if (relayControls.dataset.signature === signature) return;
  relayControls.dataset.signature = signature;
  relayControls.replaceChildren(...relays.map(function (relay, index) {
    const enabled = !disabled.has(relay);
    const button = document.createElement('button');
    button.type = 'button';
    button.dataset.relay = relay;
    button.dataset.enabled = String(enabled);
    button.dataset.relayIndex = String(index);
    button.textContent = (enabled ? 'Disconnect ' : 'Restore ') + relay;
    return button;
  }));
};

const refreshNostrSession = function () {
  if (!Module.browserNostrMode) return;
  nostrSession.hidden = false;
  const value = Module.browserNostrDiagnostics
    ? Module.browserNostrDiagnostics() : null;
  nostrPublicKey.value = value?.publicKey || '';
  nostrPublicReference.value = value?.matchReference ||
    (Module.browserNostrMode === 'join'
      ? document.getElementById('match-reference').value.trim() : '');
  copyMatchReference.disabled = nostrPublicReference.value.length === 0;
  refreshRelayControls(value);
};
setInterval(refreshNostrSession, 500);

copyMatchReference.addEventListener('click', async function () {
  if (!nostrPublicReference.value) return;
  try {
    await navigator.clipboard.writeText(nostrPublicReference.value);
    copyMatchStatus.textContent = 'Copied';
  } catch (_) {
    nostrPublicReference.focus();
    nostrPublicReference.select();
    copyMatchStatus.textContent = 'Select and copy';
  }
});

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
    nostr: Module.browserNostrDiagnostics
      ? Module.browserNostrDiagnostics() : null,
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
  const mode = document.getElementById('launch-mode').value;
  Module.browserNostrMode =
    mode === 'host' || mode === 'join' ? mode : null;
  const query = new URLSearchParams(location.search);
  query.delete('multiplayer');
  query.delete('relays');
  query.delete('match');
  query.delete('oneRelay');
  query.delete('allied');
  if (mode === 'host' || mode === 'join') {
    query.set('multiplayer', mode);
    query.set('relays', document.getElementById('relays').value.trim());
    if (mode === 'join') {
      query.set('match', document.getElementById('match-reference').value.trim());
    }
    if (document.getElementById('one-relay').checked) {
      query.set('oneRelay', '1');
    }
    if (document.getElementById('allied').checked) {
      query.set('allied', '1');
    }
  }
  history.replaceState(null, '', location.pathname +
    (query.toString() ? '?' + query.toString() : ''));
  this.hidden = true;
  document.getElementById('launch').hidden = true;
  Module['canvas'].hidden = false;
  document.getElementById('fullscreen').hidden = false;
  Module['canvas'].focus({preventScroll: true});
  Module['callMain']([]);
  refreshNostrSession();
}, {once: true});

document.getElementById('launch-mode').addEventListener('change', function () {
  const multiplayer = this.value === 'host' || this.value === 'join';
  document.getElementById('relay-field').hidden = !multiplayer;
  document.getElementById('one-relay-field').hidden = !multiplayer;
  document.getElementById('allied-field').hidden = !multiplayer;
  document.getElementById('public-warning').hidden = !multiplayer;
  document.getElementById('reference-field').hidden = this.value !== 'join';
});

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
