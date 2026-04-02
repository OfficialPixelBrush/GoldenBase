let getTile;
let updateGenAndSeed;
let Module;
let pendingGenUpdate = null;  // buffer any early updateGenAndSeed message

self.onmessage = async (e) => {
    if (e.data.type === 'init') {
        importScripts(e.data.wasmJsUrl);
        Module = await createModule();
        getTile = Module.cwrap('getTile', 'number', ['number', 'number', 'number', 'number']);
        updateGenAndSeed = Module.cwrap('UpdateGenAndSeed', 'void', ['string', 'number']);

        // Drain any update that arrived before we were ready
        if (pendingGenUpdate) {
            const { seed, genId } = pendingGenUpdate;
            pendingGenUpdate = null;
            updateGenAndSeed(seed, genId);
        }

        self.postMessage({ type: 'ready' });
        return;
    }

    if (e.data.type === 'updateGenAndSeed') {
        if (!Module) { pendingGenUpdate = e.data; return; }  // not ready yet
        const { seed, genId } = e.data;
        updateGenAndSeed(seed, genId);
    }

    if (e.data.type === 'getTile') {
        const { x, y, z, id, tileSize, options } = e.data;
        const ptr = getTile(x, y, z, options);
        const bytes = new Uint8ClampedArray(
            Module.HEAPU8.buffer,
            ptr,
            tileSize * tileSize * 4
        ).slice();
        self.postMessage({ type: 'tile', id, bytes }, [bytes.buffer]);
    }
};