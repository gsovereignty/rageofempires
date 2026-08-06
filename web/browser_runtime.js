Module['canvas'] = document.getElementById('canvas');
Module['setStatus'] = function (message) {
  const status = document.getElementById('status');
  if (status) status.textContent = message || '';
};
Module['onRuntimeInitialized'] = function () {
  document.getElementById('loading').hidden = true;
  Module['canvas'].hidden = false;
  Module['canvas'].focus({preventScroll: true});
};
Module['onAbort'] = function (reason) {
  document.getElementById('loading').hidden = false;
  Module['setStatus']('Browser startup failed: ' + reason);
};
