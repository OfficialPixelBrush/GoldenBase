importScripts('build/GoldenBase.js');

let getTileFn = null;

postMessage({type: 'debug', msg: 'Worker started'});

// Ensure the worker provides the correct .wasm URL so the loader does not fetch HTML.
createModule({
  locateFile: function(path) {
    if (path.endsWith('.wasm')) {
      return 'build/GoldenBase.wasm';
    }
    return path;
  },
  onRuntimeInitialized: function() {
    getTileFn = Module.cwrap('getTile', 'number', ['number', 'number', 'number']);
    postMessage({type: 'ready'});
    postMessage({type: 'debug', msg: 'Module ready'});
  }
});

onmessage = function(event) {
  const data = event.data;
  postMessage({type: 'debug', msg: 'Received: ' + JSON.stringify(data)});
  if (!getTileFn) {
    postMessage({type: 'debug', msg: 'getTileFn not ready'});
    return;
  }
  if (data.type === 'getTile') {
    postMessage({type: 'debug', msg: 'Calling getTile for ' + data.tileKey});
    const ptr = getTileFn(data.x, data.y, data.z);
    if (!ptr) {
      postMessage({type: 'tile', tileKey: data.tileKey, buffer: new Uint8ClampedArray(128 * 128 * 4).buffer});
      return;
    }
    const size = 128 * 128 * 4;
    const raw = new Uint8ClampedArray(Module.HEAPU8.buffer, ptr, size);
    const copy = new Uint8ClampedArray(raw);
    postMessage({type: 'tile', tileKey: data.tileKey, buffer: copy.buffer}, [copy.buffer]);
  }
};