// Each worker loads its own WASM module — completely isolated memory
let getTile;
let updateSeed;
let updateGenerator;
let Module;

self.onmessage = async (e) => {
    if (e.data.type === 'init') {
        // Import the Emscripten-generated JS glue
        importScripts(e.data.wasmJsUrl); // e.g. 'build/GoldenBase.js'

        Module = await createModule();
        getTile = Module.cwrap('getTile', 'number', ['number', 'number', 'boolean', 'boolean']);
        updateSeed = Module.cwrap('UpdateSeed', 'void', ['number']);
        updateGenerator = Module.cwrap('UpdateGenerator', 'void', ['number']);

        self.postMessage({ type: 'ready' });
        return;
    }

    if (e.data.type === 'updateSeed') {
        const { seed } = e.data;
        updateSeed(seed);
    }

    if (e.data.type === 'updateGenerator') {
        const { genId } = e.data;
        updateGenerator(genId);
    }

    if (e.data.type === 'getTile') {
        const { x, y, z, id, tileSize, heightShade, heightMap } = e.data;

        const ptr = getTile(x, y, z, heightShade, heightMap);

        // Copy the data OUT of WASM heap before posting — the buffer will be
        // overwritten on the next call (it's static in C++)
        const bytes = new Uint8ClampedArray(
            Module.HEAPU8.buffer,
            ptr,
            tileSize * tileSize * 4
        ).slice(); // <-- critical: .slice() copies, doesn't reference

        // Transfer the underlying ArrayBuffer for zero-copy postMessage
        self.postMessage({ type: 'tile', id, bytes }, [bytes.buffer]);
    }
};