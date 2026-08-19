const WORKER_COUNT = navigator.hardwareConcurrency || 4;
const WASM_JS_URL = 'GoldenBase.js';
const workers = [];
const queue = [];   // pending tile requests
const free = [];    // indices of idle workers
const workerBusy = [];
const workerJobId = [];

const BIOME_INFO = [
    { name: 'None', color: '#8db360' },
    { name: 'Rainforest', color: '#537b09' },
    { name: 'Swampland', color: '#07f9b2' },
    { name: 'Seasonal Forest', color: '#2d8e49' },
    { name: 'Forest', color: '#056621' },
    { name: 'Savanna', color: '#bdb25f' },
    { name: 'Shrubland', color: '#b5db88' },
    { name: 'Taiga', color: '#0b6659' },
    { name: 'Desert', color: '#fa9418' },
    { name: 'Plains', color: '#8db360' },
    { name: 'Ice Desert', color: '#c4d339' },
    { name: 'Tundra', color: '#ffffff' },
    { name: 'Hell', color: '#ff0000' },
    { name: 'Sky', color: '#4ee031' },
];

function biomeInfo(id) {
    return BIOME_INFO[id] || { name: 'Unknown', color: '#ff00ff' };
}

function bindWorker(idx, onReady) {
    const w = new Worker('tile-worker.js');
    workers[idx] = w;
    workerBusy[idx] = false;
    workerJobId[idx] = -1;
    w.onmessage = (e) => {
        if (e.data.type === 'ready') {
            const seedEl = document.getElementById('seedValue');
            const genEl = document.getElementById('genSelection');
            if (seedEl && genEl) {
                w.postMessage({
                    type: 'updateGenAndSeed',
                    seed: seedEl.value.trim(),
                    genId: Number(genEl.value)
                });
            }
            workerBusy[idx] = false;
            if (!free.includes(idx)) free.push(idx);
            onReady?.();
            dispatch();
            return;
        }
        if (e.data.type === 'tile') {
            workerBusy[idx] = false;
            workerJobId[idx] = -1;
            const { id, bytes } = e.data;
            pendingTiles[id]?.(bytes);
            delete pendingTiles[id];
            if (!free.includes(idx)) free.push(idx);
            dispatch();
        }
    };
    w.postMessage({ type: 'init', wasmJsUrl: WASM_JS_URL });
}

function initWorkers(wasmJsUrl, onReady) {
    let readyCount = 0;
    for (let i = 0; i < WORKER_COUNT; i++) {
        bindWorker(i, () => {
            if (++readyCount === WORKER_COUNT) onReady();
        });
    }
}

function dropQueuedJobs() {
    const jobs = queue.splice(0, queue.length);
    for (const job of jobs) pendingTiles[job.id]?.(null);
}

function abortBusyWorkers() {
    for (let i = 0; i < workers.length; i++) {
        if (!workerBusy[i]) continue;
        const id = workerJobId[i];
        try { workers[i].terminate(); } catch (err) { /* already dead */ }
        workerBusy[i] = false;
        workerJobId[i] = -1;
        if (id >= 0) pendingTiles[id]?.(null);
        bindWorker(i);
    }
}

const pendingTiles = {}; // id → resolve function
let tileIdCounter = 0;
let currentGenId = 0;

function getOptions() {
    let opt_value = 0;
    const viewMode = document.getElementById('viewMode')?.value || 'topo';
    const colorMode = document.getElementById('colorMode')?.value || 'biome';
    if (viewMode === 'heightmap' && colorMode !== 'map') opt_value |= 1 << 0;
    if (document.getElementById('check_blockcolors').checked && colorMode !== 'topology' && colorMode !== 'map') opt_value |= 1 << 1;
    if (document.getElementById('check_water').checked) opt_value |= 1 << 2;
    if (document.getElementById('check_snow_mode').checked) opt_value |= 1 << 3;
    if (document.getElementById('check_snow_world').checked) opt_value |= 1 << 4;
    if (colorMode === 'biome') opt_value |= 1 << 5;
    if (viewMode === 'topo' && colorMode !== 'map') opt_value |= 1 << 6;
    if (colorMode === 'accurate') opt_value |= 1 << 7;
    if (colorMode === 'topology') opt_value |= 1 << 8;
    if (colorMode === 'map') opt_value |= 1 << 9;
    return opt_value;
}

function requestTile(x, y, z, tileSize) {
    const genId = currentGenId;
    return new Promise((resolve) => {
        if (tileFullyOutsideInt32(x, y, z, tileSize)) {
            resolve(new Uint8ClampedArray(tileSize * tileSize * 4));
            return;
        }
        const id = tileIdCounter++;
        pendingTiles[id] = (bytes) => {
            delete pendingTiles[id];
            if (bytes == null || genId !== currentGenId) resolve(null);
            else resolve(bytes);
        };
        queue.push({ x, y, z, id, tileSize, options: getOptions(), epoch: genId });
        dispatch();
    });
}

function dispatch() {
    while (free.length > 0 && queue.length > 0) {
        const job = queue.shift();
        if (job.epoch !== currentGenId) {
            pendingTiles[job.id]?.(null);
            continue;
        }
        const workerIdx = free.pop();
        workerBusy[workerIdx] = true;
        workerJobId[workerIdx] = job.id;
        workers[workerIdx].postMessage({
            type: 'getTile',
            x: job.x,
            y: job.y,
            z: job.z,
            id: job.id,
            tileSize: job.tileSize,
            options: job.options
        });
    }
}

const GridOverlay = L.GridLayer.extend({
    initialize: function (options) {
        L.GridLayer.prototype.initialize.call(this, options);

        this.chunkSize = 16;
        this.regionSize = 512;
    },

    createTile: function (coords) {
        const tile = document.createElement('canvas');
        const size = this.getTileSize();

        tile.width = size.x;
        tile.height = size.y;

        const ctx = tile.getContext('2d');

        const zoomScale = this._map.getZoomScale(coords.z, 0);

        const worldX = coords.x * size.x;
        const worldY = coords.y * size.y;

        // pixel-space grid spacing
        const chunkPx = this.chunkSize * zoomScale;
        const regionPx = this.regionSize * zoomScale;

        const showChunkGrid =
            document.getElementById('check_chunk_grid')?.checked;

        const showRegionGrid =
            document.getElementById('check_region_grid')?.checked;

        // chunk grid
        if (showChunkGrid && coords.z > -2) {
            ctx.strokeStyle = "#ffffff44";
            ctx.lineWidth = 1;

            ctx.beginPath();

            for (let x = -(worldX % chunkPx); x < size.x; x += chunkPx) {
                ctx.moveTo(x, 0);
                ctx.lineTo(x, size.y);
            }

            for (let y = -(worldY % chunkPx); y < size.y; y += chunkPx) {
                ctx.moveTo(0, y);
                ctx.lineTo(size.x, y);
            }

            ctx.stroke();
        }

        // region grid (skip when a region is smaller than 2px — would paint solid)
        if (showRegionGrid && regionPx >= 32) {
            ctx.strokeStyle = "#ffffff99";
            ctx.lineWidth = 2;

            ctx.beginPath();

            for (let x = -(worldX % regionPx); x < size.x; x += regionPx) {
                ctx.moveTo(x, 0);
                ctx.lineTo(x, size.y);
            }

            for (let y = -(worldY % regionPx); y < size.y; y += regionPx) {
                ctx.moveTo(0, y);
                ctx.lineTo(size.x, y);
            }

            ctx.stroke();
        }

        return tile;
    }
});


const MASK48 = (1n << 48n) - 1n;
const JAVA_MULT = 0x5DEECE66Dn;
const JAVA_ADD  = 0xBn;

function javaNextInt(seedRef, bound) {
    // seedRef is an object { v: BigInt } so caller sees the update
    seedRef.v = (seedRef.v * JAVA_MULT + JAVA_ADD) & MASK48;
    const bits = Number(seedRef.v >> 17n);      // top 31 bits
    return bits % bound;
}

function s32(n) {
    // Force JavaScript Number into signed 32-bit range (matches Java int cast)
    return (n | 0);
}

function isSlimeChunk(worldSeed, chunkX, chunkZ) {
    // Replicate Java's seed expression exactly.
    // Each multiplication/addition is cast to int (32-bit signed) as in Java.
    const wx = s32(chunkX);
    const wz = s32(chunkZ);

    const a = s32(s32(wx * wx) * 0x4c1906);
    const b = s32(wx * 0x5ac0db);
    const c = s32(s32(wz * wz) * 0x4307a7);
    const d = s32(wz * 0x5f24f);

    // worldSeed is a string from the input; parse as BigInt.
    // Only the lowest 48 bits matter per the Java spec.
    const ws = BigInt.asIntN(64, BigInt(worldSeed));

    const combined = ws + BigInt(a) + BigInt(b) + BigInt(c) + BigInt(d);
    const xored = combined ^ 0x3ad8025fn;

    // Initial seed for java.util.Random: mask to 48 bits, then XOR with multiplier
    const initSeed = (xored ^ JAVA_MULT) & MASK48;
    const seedRef = { v: initSeed };

    return javaNextInt(seedRef, 10) === 0;
}

const SlimeOverlay = L.GridLayer.extend({
    createTile: function(coords) {
        const tile = document.createElement('canvas');
        const size = this.getTileSize();
        tile.width  = size.x;
        tile.height = size.y;

        const ctx = tile.getContext('2d');
        const seed = parseSeed(document.getElementById('seedValue').value);

        const tileZoom = 0;
        const zoomDiff = tileZoom - coords.z;
        const chunksPerTile = Math.pow(2, zoomDiff) * (size.x / 16);
        // At deep zoom-out a tile covers millions of chunks; skip rather than hang.
        if (chunksPerTile > 128)
            return tile;

        const baseChunkX = coords.x * chunksPerTile;
        const baseChunkZ = coords.y * chunksPerTile;

        const chunkPx = size.x / chunksPerTile;

        ctx.save();
        ctx.globalAlpha = 0.4;
        ctx.fillStyle = '#00ff00';

        for (let cx = 0; cx < chunksPerTile; cx++) {
            for (let cz = 0; cz < chunksPerTile; cz++) {
                if (isSlimeChunk(seed, baseChunkX + cx, baseChunkZ + cz)) {
                    ctx.fillRect(
                        cx * chunkPx,
                        cz * chunkPx,
                        chunkPx,
                        chunkPx
                    );
                }
            }
        }

        ctx.restore();
        return tile;
    }
});

// Classic Far Lands (inf-20100327+): finest Perlin octave samples
// (world/4)*684.412, overflowing a Java int at ±12,550,821.
// Farther Lands: selector noise at that scale / 80, overflowing at ±1,004,065,811.
// Infdev 20100227–20100325: solid stone wall at ±33,554,432 (2^25).
const INT32_MIN = -2147483648;
const INT32_MAX = 2147483647;
const FARLANDS_EXTENT = INT32_MAX;

function tileFullyOutsideInt32(tx, tz, zoom, tilePx) {
    const bpp = Math.pow(2, -zoom);
    const w = tilePx * bpp;
    const x0 = tx * w;
    const z0 = tz * w;
    const x1 = x0 + w;
    const z1 = z0 + w;
    const hi = INT32_MAX + 1;
    return x1 <= INT32_MIN || x0 >= hi || z1 <= INT32_MIN || z0 >= hi;
}

function clampBlockCoord(n) {
    if (!Number.isFinite(n))
        return 0;
    if (n < INT32_MIN)
        return INT32_MIN;
    if (n > INT32_MAX)
        return INT32_MAX;
    return n;
}

function blockToLatLng(x, z) {
    return L.latLng(-z, x);
}

function farlandsThreshold(genId) {
    if (!genId)
        return 0;
    if (genId === 1)
        return 33554432;
    return 12550821;
}

// Selector noise uses scale 684.412/80, so it overflows ~80× farther out.
function fartherlandsThreshold(genId) {
    if (!genId || genId === 1)
        return 0;
    return 1004065811;
}

// Overlays follow the generator last applied with Update Gen, not the dropdown.
let appliedGenId = 9;

function fillWorldRect(ctx, tileX0, tileZ0, bpp, tilePx, wx0, wz0, wx1, wz1) {
    const tileW = tilePx * bpp;
    const ix0 = Math.max(wx0, tileX0);
    const iz0 = Math.max(wz0, tileZ0);
    const ix1 = Math.min(wx1, tileX0 + tileW);
    const iz1 = Math.min(wz1, tileZ0 + tileW);
    if (ix1 <= ix0 || iz1 <= iz0)
        return;
    ctx.fillRect(
        (ix0 - tileX0) / bpp,
        (iz0 - tileZ0) / bpp,
        (ix1 - ix0) / bpp,
        (iz1 - iz0) / bpp
    );
}

const FarlandsOverlay = L.GridLayer.extend({
    createTile: function(coords) {
        const tile = document.createElement('canvas');
        const size = this.getTileSize();
        tile.width = size.x;
        tile.height = size.y;

        const F = farlandsThreshold(appliedGenId);
        if (!F)
            return tile;

        const FF = fartherlandsThreshold(appliedGenId);
        const farOuter = FF > 0 ? Math.min(FF, FARLANDS_EXTENT) : FARLANDS_EXTENT;

        const bpp = Math.pow(2, -coords.z);
        const tileX0 = coords.x * size.x * bpp;
        const tileZ0 = coords.y * size.y * bpp;
        const ctx = tile.getContext('2d');
        ctx.globalAlpha = 0.38;

        // Edge Far Lands (one axis overflowed): red
        ctx.fillStyle = '#ff2200';
        fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, F, -F, farOuter, F);
        fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -farOuter, -F, -F, F);
        fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -F, F, F, farOuter);
        fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -F, -farOuter, F, -F);

        // Corner Far Lands (both axes overflowed): orange
        ctx.fillStyle = '#ff9900';
        fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, F, F, farOuter, farOuter);
        fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, F, -farOuter, farOuter, -F);
        fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -farOuter, F, -F, farOuter);
        fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -farOuter, -farOuter, -F, -F);

        if (FF > 0) {
            // Edge Farther Lands (selector overflow on one axis): lighter orange
            ctx.fillStyle = '#ffb35c';
            fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, FF, -FF, FARLANDS_EXTENT, FF);
            fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -FARLANDS_EXTENT, -FF, -FF, FF);
            fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -FF, FF, FF, FARLANDS_EXTENT);
            fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -FF, -FARLANDS_EXTENT, FF, -FF);

            // Corner Farther Lands (selector overflow on both axes): orange-yellow
            ctx.fillStyle = '#ffdc4a';
            fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, FF, FF, FARLANDS_EXTENT, FARLANDS_EXTENT);
            fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, FF, -FARLANDS_EXTENT, FARLANDS_EXTENT, -FF);
            fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -FARLANDS_EXTENT, FF, -FF, FARLANDS_EXTENT);
            fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -FARLANDS_EXTENT, -FARLANDS_EXTENT, -FF, -FF);
        }

        return tile;
    }
});

// Hard world boundary at ±32,000,000, added after Infdev 20100313.
// Terrain past this point is non-solid fake chunks (air with leftover textures).
function worldBoundaryThreshold(genId) {
    if (genId >= 2)
        return 32000000;
    return 0;
}

const WorldBoundaryOverlay = L.GridLayer.extend({
    createTile: function(coords) {
        const tile = document.createElement('canvas');
        const size = this.getTileSize();
        tile.width = size.x;
        tile.height = size.y;

        const B = worldBoundaryThreshold(appliedGenId);
        if (!B)
            return tile;

        const bpp = Math.pow(2, -coords.z);
        const tileX0 = coords.x * size.x * bpp;
        const tileZ0 = coords.y * size.y * bpp;
        const ctx = tile.getContext('2d');
        ctx.globalAlpha = 0.55;
        ctx.fillStyle = '#000000';

        // Everything outside the ±32M square
        fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, B, -FARLANDS_EXTENT, FARLANDS_EXTENT, FARLANDS_EXTENT);
        fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -FARLANDS_EXTENT, -FARLANDS_EXTENT, -B, FARLANDS_EXTENT);
        fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -B, B, B, FARLANDS_EXTENT);
        fillWorldRect(ctx, tileX0, tileZ0, bpp, size.x, -B, -FARLANDS_EXTENT, B, -B);

        return tile;
    }
});

// Matches src/java/javaMath.h HashCode() / Java String.hashCode()
function javaStringHashCode(str) {
    let h = 0;
    for (let i = 0; i < str.length; i++) {
        h = (Math.imul(31, h) + str.charCodeAt(i)) | 0;
    }
    return h;
}

function parseSeed(raw) {
    const trimmed = raw.trim();
    // Same numeric check as UpdateGenAndSeed in main.cpp
    if (/^[+-]?\d+$/.test(trimmed)) {
        return trimmed;
    }
    return String(javaStringHashCode(trimmed));
}

function syncBlockColorsForColorMode() {
    const colorMode = document.getElementById('colorMode')?.value;
    const topology = colorMode === 'topology';
    const mapMode = colorMode === 'map';
    const cb = document.getElementById('check_blockcolors');
    const label = document.getElementById('check_blockcolors_label');
    if (cb) {
        cb.disabled = topology || mapMode;
        if (label)
            label.style.opacity = cb.disabled ? '0.4' : '';
    }
    const shade = document.getElementById('viewMode');
    if (shade) {
        shade.disabled = mapMode;
        shade.style.opacity = mapMode ? '0.4' : '';
    }
}

function generatorHasBiomes(genId) {
    return genId >= 8;
}

function applyShareParams() {
    const p = new URLSearchParams(window.location.search);
    const setVal = (id, key) => {
        if (p.has(key))
            document.getElementById(id).value = p.get(key);
    };
    const setCheck = (id, key) => {
        if (p.has(key))
            document.getElementById(id).checked = p.get(key) === '1';
    };
    setVal('seedValue', 'seed');
    if (p.has('gen')) {
        const gen = p.get('gen');
        const genEl = document.getElementById('genSelection');
        if (genEl && [...genEl.options].some((o) => o.value === gen))
            genEl.value = gen;
    }
    if (p.has('shade')) {
        const v = p.get('shade');
        if (['none', 'heightmap', 'topo'].includes(v))
            document.getElementById('viewMode').value = v;
    }
    if (p.has('color')) {
        const v = p.get('color');
        if (['none', 'biome', 'accurate', 'topology', 'map'].includes(v))
            document.getElementById('colorMode').value = v;
    }
    setCheck('check_blockcolors', 'blocks');
    setCheck('check_water', 'water');
    setCheck('check_snow_mode', 'snow');
    setCheck('check_snow_world', 'snowWorld');
    setCheck('check_slime_chunks', 'slime');
    setCheck('check_farlands', 'farlands');
    setCheck('check_world_boundary', 'boundary');
    setCheck('check_region_labels', 'labels');
    setCheck('check_chunk_grid', 'chunks');
    setCheck('check_region_grid', 'regions');
    setCheck('check_crosshair', 'cursor');
    const x = p.has('x') ? Number(p.get('x')) : 0;
    const z = p.has('z') ? Number(p.get('z')) : 0;
    const zoom = p.has('zoom') ? Number(p.get('zoom')) : 0;
    const xPos = document.getElementById('xPos');
    const zPos = document.getElementById('zPos');
    if (xPos)
        xPos.value = String(x);
    if (zPos)
        zPos.value = String(z);
    return { x, z, zoom, fromUrl: p.has('x') || p.has('z') || p.has('zoom') || p.has('seed') || p.has('gen') };
}

let mapCenter = { x: 0, y: 0 };

window.addEventListener('load', () => {
  createModule({
      onRuntimeInitialized: function() {
            const getTileSize = this.cwrap('getTileSize', 'number', []);
            const scale = getTileSize(); // pixels per tile; 1:1 is 1px per block
            const minZoom = -23;
            const Module = this;
            window.Module = Module;
            const updateGenAndSeedMain = this.cwrap('UpdateGenAndSeed', 'void', ['string', 'number']);
            const getBiomeAt = (typeof this._getBiomeAt === 'function')
                ? this.cwrap('getBiomeAt', 'number', ['number', 'number', 'number'])
                : null;

            const tileZoom = 0; // your tiles exist only at this zoom

            const map = L.map('map', {
                crs: L.CRS.Simple,
                minZoom: minZoom,
                maxZoom: 2,
                noWrap: true,
                maxBoundsViscosity: 1.0,
                maxBounds: [
                    [-INT32_MAX, INT32_MIN],
                    [-INT32_MIN, INT32_MAX],
                ],
                keepBuffer: 10   // default is 2
            });
            map.createPane('gridPane');
            map.getPane('gridPane').style.zIndex = 700;
            map.createPane('labelPane');
            map.getPane('labelPane').style.zIndex = 750;
            map.getPane('labelPane').style.pointerEvents = 'none';

            map.createPane('tilePane');
            map.getPane('tilePane').style.zIndex = 400;

            const crossH = L.DomUtil.create('div', '', map.getContainer());
            const crossV = L.DomUtil.create('div', '', map.getContainer());
            Object.assign(crossH.style, {
                position: 'absolute',
                top: '50%',
                left: 0,
                width: '100%',
                height: '1px',
                background: '#ffffff55',
                pointerEvents: 'none',
                zIndex: 9999
            });
            Object.assign(crossV.style, {
                position: 'absolute',
                left: '50%',
                top: 0,
                width: '1px',
                height: '100%',
                background: '#ffffff55',
                pointerEvents: 'none',
                zIndex: 9999
            });
            const Grid = L.GridLayer.extend({
                createTile: function(coords) {
                    const tile = L.DomUtil.create('canvas', 'leaflet-tile');
                    const size = this.getTileSize();

                    tile.width = size.x;
                    tile.height = size.y;

                    const ctx = tile.getContext('2d');

                    ctx.strokeStyle = '#888';
                    ctx.lineWidth = 1;

                    ctx.beginPath();
                    ctx.moveTo(0, 0);
                    ctx.lineTo(size.x, 0);
                    ctx.lineTo(size.x, size.y);
                    ctx.lineTo(0, size.y);
                    ctx.closePath();
                    ctx.stroke();

                    return tile;
                }
            });

            (new Grid()).addTo(map);

            L.polyline([[0,-10],[0, 10]], {color: 'white'}).addTo(map);
            L.polyline([[-10,0],[10, 0]], {color: 'white'}).addTo(map);
            
            // Create a custom control
            const infoControl = L.control({ position: 'topright' });

            infoControl.onAdd = function (map) {
            const div = L.DomUtil.create('div', 'custom-control');

            div.innerHTML = `
                <table style="width: 300pt; table-layout: fixed;">
                    <tr>
                        <td style="text-align: center;" colspan="2">
                            <p style="margin:0;"><b>GoldenBase</b></p>
                            <p style="margin:0;">Pre-release world explorer</p>
                        </td>
                    </tr>
                    <tr>
                        <td style="text-align: center;">
                            <p style="margin:0;">
                                Made by <a style="color: lightblue" href="https://pixelbrush.dev/about">Pixel Brush</a>
                            </p>
                        </td>
                        <td style="text-align: center;">
                            <p style="margin:0;">
                                <a style="color: lightblue" href="https://github.com/OfficialPixelBrush/GoldenBase">
                                    Github Repository
                                </a>
                            </p>
                        </td>
                    </tr>
                </table>
                <details open="true">
                    <summary style="cursor:pointer;">Controls / Configuration</summary>

                    <table style="width: 300pt; table-layout: fixed;">

                        <tr>
                            <td>
                                <code id="coords"></code><br/>
                                <code id="bigCoords"></code><br>
                                <code id="biomeCoords"></code>
                            </td>
                            <td>
                                <input type="number" id="xPos" placeholder="x" value="0" style="width: 100%"><br>
                                <input type="number" id="zPos" placeholder="z" value="0" style="width: 100%"><br>
                                <button onclick="setPosition()">Go</button>
                            </td>
                        </tr>

                        <tr>
                            <td>
                                <label>Generator</label>
                            </td>
                            <td>
                                <select id="genSelection">
                                    <option value="9">b1.3.0 - b1.7.3</option>
                                    <option value="8">a1.2.0 - b1.2.0_02</option>
                                    <option value="7">inf-20100624 - a1.1.2_01</option>
                                    <option value="6">inf-20100616 - inf-20100618</option>
                                    <option value="5">inf-20100611 - inf-20100615</option>
                                    <option value="4">inf-20100420 - inf-20100608</option>
                                    <option value="3">inf-20100413 - inf-20100415</option>
                                    <option value="2">inf-20100327 - inf-20100330</option>
                                    <option value="1">inf-20100227 - inf-20100325</option>
                                </select>
                            </td>
                        </tr>

                        <tr id="snowWorldRow">
                            <td>
                                <input type="checkbox" id="check_snow_world" name="check_snow_world">
                                <label for="check_snow_world">Snow World</label>
                            </td>
                            <td></td>
                        </tr>

                        <tr>
                            <td>
                                <label>Seed</label>
                            </td>
                            <td style="display:flex; align-items:center; gap:4px;">
                                <input id="seedValue" value="3257840388504953787" style="flex:1; min-width:0;">
                                <button id="randomSeed" title="Random seed" style="flex-shrink:0; font-size:18px; padding:2px 6px; cursor:pointer;">🎲</button>
                            </td>
                        </tr>

                        <tr>
                            <td colspan="2">
                                <button id="updateGen" style="width:100%">Update Gen</button>
                            </td>
                        </tr>

                        <tr>
                            <td style="vertical-align: top;" colspan="2">
                                <details>
                                    <summary>Visualizer Settings</summary>
                                    <label for="viewMode">Shading</label>
                                    <select id="viewMode">
                                        <option value="none">None</option>
                                        <option value="heightmap">Heightmap</option>
                                        <option value="topo" selected>Hillshade</option>
                                    </select><br>

                                    <label for="colorMode">Coloration</label>
                                    <select id="colorMode">
                                        <option value="none">None</option>
                                        <option value="biome" selected>Biome</option>
                                        <option value="accurate">Accurate</option>
                                        <option value="topology">Topology</option>
                                        <option value="map">Map Item</option>
                                    </select><br>

                                    <input type="checkbox" id="check_blockcolors" checked>
                                    <label for="check_blockcolors" id="check_blockcolors_label">Block colors</label><br>

                                    <input type="checkbox" id="check_water" checked>
                                    <label for="check_water">Show Water/Ice</label><br>

                                    <input type="checkbox" id="check_snow_mode" checked>
                                    <label for="check_snow_mode">Show surface snow</label><br>

                                    <label for="viewMode">Overlays</label><br>
                                    <input type="checkbox" id="check_slime_chunks">
                                    <label for="check_slime_chunks" id="check_slime_chunks_checkmark">Slime Chunks</label><br>

                                    <input type="checkbox" id="check_farlands" checked>
                                    <label for="check_farlands" id="check_farlands_label">Far Lands</label><br>

                                    <input type="checkbox" id="check_world_boundary" checked>
                                    <label for="check_world_boundary" id="check_world_boundary_label">Non-solid</label><br>

                                    <input type="checkbox" id="check_region_labels" checked>
                                    <label for="check_region_labels">Region labels</label><br>

                                    <input type="checkbox" id="check_chunk_grid">
                                    <label for="check_chunk_grid">Chunk Grid</label><br>

                                    <input type="checkbox" id="check_region_grid" checked>
                                    <label for="check_region_grid">Region Grid</label><br>

                                    <input type="checkbox" id="check_crosshair" checked>
                                    <label for="check_crosshair">Show Cursor</label><br>
                                </details>
                            </td>
                        </tr>

                    </table>
                </details>
            `;

            return div;
        };
            infoControl.addTo(map);
            const share = applyShareParams();
            appliedGenId = Number(document.getElementById('genSelection').value) || 9;
            updateGenAndSeedMain(
                document.getElementById('seedValue').value.trim(),
                appliedGenId
            );
            syncBlockColorsForColorMode();

            function checkIfSnowWorld(genId) {
                if (genId == 7) {
                    snowWorldRow.style.display = "";
                    return;
                }
                snowWorldRow.style.display = "none";
            }

            function checkIfSlimeChunks(genId) {
                const checkbox = document.getElementById('check_slime_chunks');
                const label = document.getElementById('check_slime_chunks_checkmark');
                const supported = genId > 6 && genId != 8;

                checkbox.disabled = !supported;
                label.style.opacity = supported ? '' : '0.4';

                if (!supported && checkbox.checked) {
                    checkbox.checked = false;
                    updateSlimeLayer();
                }
            }
            checkIfSnowWorld(appliedGenId);
            checkIfSlimeChunks(appliedGenId);
            document
                .getElementById('genSelection')
                .addEventListener('change', (e) => {
                    checkIfSnowWorld(Number(e.target.value));
                    checkIfSlimeChunks(Number(e.target.value));
                });

            // Prevent clicks from propagating to the map
            L.DomEvent.disableClickPropagation(infoControl.getContainer());

            function cleanZero(n) {
                return Math.abs(n) < 1e-9 ? 0 : n;
            }

            function cursorBlockPos() {
                const point = map.project(map.getCenter(), tileZoom);
                return {
                    x: clampBlockCoord(point.x),
                    z: clampBlockCoord(point.y),
                };
            }

            function applyInt32CenterBounds() {
                const size = map.getSize();
                let padLng = 0;
                let padLat = 0;
                if (size.x >= 1 && size.y >= 1) {
                    const c00 = map.containerPointToLatLng([0, 0]);
                    const c11 = map.containerPointToLatLng([size.x, size.y]);
                    padLng = Math.abs(c11.lng - c00.lng) / 2;
                    padLat = Math.abs(c11.lat - c00.lat) / 2;
                }
                map.setMaxBounds([
                    [-INT32_MAX - padLat, INT32_MIN - padLng],
                    [-INT32_MIN + padLat, INT32_MAX + padLng],
                ]);
                map.options.maxBoundsViscosity = 1.0;
            }

            function clampViewToInt32() {
                const c = map.getCenter();
                const x = clampBlockCoord(c.lng);
                const z = clampBlockCoord(-c.lat);
                if (x !== c.lng || z !== -c.lat)
                    map.setView(L.latLng(-z, x), map.getZoom(), { animate: false });
            }

            function updateCenter() {
                const cursor = cursorBlockPos();
                const blockPosX = cursor.x;
                const blockPosZ = cursor.z;
                mapCenter.x = blockPosX / scale;
                mapCenter.y = blockPosZ / scale;
                document.getElementById('coords').textContent = `Center: ${blockPosX.toFixed(2)}, ${blockPosZ.toFixed(2)}`;
                document.getElementById('bigCoords').textContent = `Cnk: ${((blockPosX/16)-0.5).toFixed(0)}, ${((blockPosZ/16)-0.5).toFixed(0)} / Rgn: ${((blockPosX/512)-0.5).toFixed(0)}, ${((blockPosZ/512)-0.5).toFixed(0)}`;
                const biomeEl = document.getElementById('biomeCoords');
                if (biomeEl) {
                    if (!generatorHasBiomes(appliedGenId)) {
                        biomeEl.style.display = 'none';
                        biomeEl.textContent = '';
                    } else {
                        biomeEl.style.display = '';
                        if (getBiomeAt) {
                            const bx = Math.floor(blockPosX);
                            const bz = Math.floor(blockPosZ);
                            const info = biomeInfo(getBiomeAt(bx, bz, map.getZoom()));
                            biomeEl.innerHTML = `Biome: <span style="display:inline-block;width:10px;height:10px;background:${info.color};border:1px solid #888;vertical-align:middle;margin-right:4px;"></span>${info.name}`;
                        } else {
                            biomeEl.textContent = 'Biome: —';
                        }
                    }
                }
            }

            function writeShareParams() {
                const p = new URLSearchParams();
                p.set('seed', document.getElementById('seedValue')?.value.trim() ?? '');
                p.set('gen', document.getElementById('genSelection')?.value ?? '9');
                const center = map.getCenter();
                const point = map.project(center, tileZoom);
                p.set('x', String(Math.round(point.x)));
                p.set('z', String(Math.round(point.y)));
                p.set('zoom', String(map.getZoom()));
                p.set('shade', document.getElementById('viewMode')?.value ?? 'topo');
                p.set('color', document.getElementById('colorMode')?.value ?? 'biome');
                const flag = (id) => document.getElementById(id)?.checked ? '1' : '0';
                p.set('blocks', flag('check_blockcolors'));
                p.set('water', flag('check_water'));
                p.set('snow', flag('check_snow_mode'));
                p.set('snowWorld', flag('check_snow_world'));
                p.set('slime', flag('check_slime_chunks'));
                p.set('farlands', flag('check_farlands'));
                p.set('boundary', flag('check_world_boundary'));
                p.set('labels', flag('check_region_labels'));
                p.set('chunks', flag('check_chunk_grid'));
                p.set('regions', flag('check_region_grid'));
                p.set('cursor', flag('check_crosshair'));
                const qs = p.toString();
                const url = `${location.pathname}${qs ? '?' + qs : ''}${location.hash}`;
                if (`${location.pathname}${location.search}${location.hash}` !== url)
                    history.replaceState(null, '', url);
            }

            let shareTimer = 0;
            function scheduleShareParams() {
                clearTimeout(shareTimer);
                shareTimer = setTimeout(writeShareParams, 200);
            }

            window.setPosition = function() {
                const x = clampBlockCoord(Number(document.getElementById('xPos').value));
                const z = clampBlockCoord(Number(document.getElementById('zPos').value));
                document.getElementById('xPos').value = String(x);
                document.getElementById('zPos').value = String(z);
                map.setView([z * -1, x]);

                clearOffscreenTiles();
            };

            function cancelAllTiles() {
                currentGenId++;
                dropQueuedJobs();
                abortBusyWorkers();
                for (const k of Object.keys(pendingTiles)) {
                    const cb = pendingTiles[k];
                    delete pendingTiles[k];
                    cb(null);
                }
            }

            function clearOffscreenTiles() {
                const bounds = map.getBounds();
                const currentZoom = map.getZoom();

                for (let i = queue.length - 1; i >= 0; i--) {
                    const { x, y, z, id } = queue[i];
                    const latlng = map.unproject([x * scale, y * scale], currentZoom);
                    if (!bounds.contains(latlng)) {
                        queue.splice(i, 1);
                        const cb = pendingTiles[id];
                        delete pendingTiles[id];
                        cb?.(null);
                    }
                }
            }
            
            function regenTiles() {
                map.eachLayer(layer => {
                    if (
                        layer instanceof L.GridLayer &&
                        layer._tiles &&
                        layer.options.pane === "tilePane"
                    ) {
                        Object.values(layer._tiles).forEach(tileObj => {
                            const coords = tileObj.coords;
                            const tile = tileObj.el;
                            requestTile(coords.x, coords.y, coords.z, tile.width).then((bytes) => {
                                if (!bytes) return;
                                const ctx = tile.getContext('2d');
                                const imageData = ctx.createImageData(tile.width, tile.height);
                                imageData.data.set(bytes);
                                ctx.putImageData(imageData, 0, 0);
                            });
                        });
                    }
                });
            }

            // Slime overlay — created once, shown/hidden by checkbox
            const slimeLayer = new SlimeOverlay({
                pane: 'gridPane',           // sits above tiles, below UI
                tileSize: scale,
                minZoom: minZoom,
                maxZoom: 2,
                noWrap: true,
                opacity: 1,
            });

            function updateSlimeLayer() {
                const show = document.getElementById('check_slime_chunks')?.checked;
                if (show) {
                    if (!map.hasLayer(slimeLayer)) slimeLayer.addTo(map);
                    else slimeLayer.redraw();
                } else {
                    if (map.hasLayer(slimeLayer)) map.removeLayer(slimeLayer);
                }
            }

            document
                .getElementById('check_slime_chunks')
                .addEventListener('change', () => {
                    updateSlimeLayer();
                    writeShareParams();
                });
            updateSlimeLayer();

            const farlandsLayer = new FarlandsOverlay({
                pane: 'gridPane',
                tileSize: scale,
                minZoom: minZoom,
                maxZoom: 2,
                noWrap: true,
                opacity: 1,
            });

            function updateFarlandsLayer() {
                const show = document.getElementById('check_farlands')?.checked;
                if (show) {
                    if (!map.hasLayer(farlandsLayer)) farlandsLayer.addTo(map);
                    else farlandsLayer.redraw();
                } else {
                    if (map.hasLayer(farlandsLayer)) map.removeLayer(farlandsLayer);
                }
            }

            function checkIfFarlands(genId) {
                const checkbox = document.getElementById('check_farlands');
                const label = document.getElementById('check_farlands_label');
                const supported = farlandsThreshold(genId) > 0;

                checkbox.disabled = !supported;
                label.style.opacity = supported ? '' : '0.4';

                if (!supported && checkbox.checked) {
                    checkbox.checked = false;
                }
                updateFarlandsLayer();
            }

            checkIfFarlands(appliedGenId);
            document
                .getElementById('check_farlands')
                .addEventListener('change', () => {
                    updateFarlandsLayer();
                    writeShareParams();
                });

            const worldBoundaryLayer = new WorldBoundaryOverlay({
                pane: 'gridPane',
                tileSize: scale,
                minZoom: minZoom,
                maxZoom: 2,
                noWrap: true,
                opacity: 1,
            });

            function updateWorldBoundaryLayer() {
                const show = document.getElementById('check_world_boundary')?.checked;
                if (show) {
                    if (!map.hasLayer(worldBoundaryLayer)) worldBoundaryLayer.addTo(map);
                    else worldBoundaryLayer.redraw();
                } else {
                    if (map.hasLayer(worldBoundaryLayer)) map.removeLayer(worldBoundaryLayer);
                }
            }

            function checkIfWorldBoundary(genId) {
                const checkbox = document.getElementById('check_world_boundary');
                const label = document.getElementById('check_world_boundary_label');
                const supported = worldBoundaryThreshold(genId) > 0;

                checkbox.disabled = !supported;
                label.style.opacity = supported ? '' : '0.4';

                if (!supported && checkbox.checked) {
                    checkbox.checked = false;
                }
                updateWorldBoundaryLayer();
            }

            checkIfWorldBoundary(appliedGenId);
            document
                .getElementById('check_world_boundary')
                .addEventListener('change', () => {
                    updateWorldBoundaryLayer();
                    writeShareParams();
                });

            const regionLabelLayer = L.layerGroup([], { pane: 'labelPane' });

            function addRegionLabel(text, x, z, color, viewBounds) {
                const latlng = blockToLatLng(x, z);
                if (viewBounds && !viewBounds.contains(latlng))
                    return;
                const icon = L.divIcon({
                    className: 'world-region-label',
                    html: `<span style="color:${color}">${text}</span>`,
                    iconSize: [0, 0],
                    iconAnchor: [0, 0],
                });
                L.marker(latlng, {
                    icon,
                    pane: 'labelPane',
                    interactive: false,
                    keyboard: false,
                    zIndexOffset: 500,
                }).addTo(regionLabelLayer);
            }

            function labelRing(text, inner, outer, color, corners, viewBounds) {
                const mid = (inner + outer) / 2;
                if (corners) {
                    addRegionLabel(text, mid, mid, color, viewBounds);
                    addRegionLabel(text, mid, -mid, color, viewBounds);
                    addRegionLabel(text, -mid, mid, color, viewBounds);
                    addRegionLabel(text, -mid, -mid, color, viewBounds);
                } else {
                    addRegionLabel(text, mid, 0, color, viewBounds);
                    addRegionLabel(text, -mid, 0, color, viewBounds);
                    addRegionLabel(text, 0, mid, color, viewBounds);
                    addRegionLabel(text, 0, -mid, color, viewBounds);
                }
            }

            function rebuildRegionLabels() {
                regionLabelLayer.clearLayers();
                if (!document.getElementById('check_region_labels')?.checked)
                    return;
                if (!map._loaded)
                    return;
                const F = farlandsThreshold(appliedGenId);
                const FF = fartherlandsThreshold(appliedGenId);
                const farOuter = FF > 0 ? Math.min(FF, FARLANDS_EXTENT) : FARLANDS_EXTENT;
                // Off-screen markers at ±10^9 sit at billion-pixel CSS translates
                // and stall the browser; only mount labels in the current view.
                const bounds = map.getBounds();
                if (!bounds || !bounds.isValid())
                    return;
                const viewBounds = bounds.pad(1);

                if (F > 0) {
                    labelRing('Far Lands', F, farOuter, '#ff6a4a', false, viewBounds);
                    labelRing('Corner Far Lands', F, farOuter, '#ffb040', true, viewBounds);
                }
                if (FF > 0) {
                    labelRing('Farther Lands', FF, FARLANDS_EXTENT, '#ffc266', false, viewBounds);
                    labelRing('Corner Farther Lands', FF, FARLANDS_EXTENT, '#ffe14a', true, viewBounds);
                }
            }

            function updateRegionLabels() {
                rebuildRegionLabels();
                const show = document.getElementById('check_region_labels')?.checked;
                if (show) {
                    if (!map.hasLayer(regionLabelLayer))
                        regionLabelLayer.addTo(map);
                } else if (map.hasLayer(regionLabelLayer)) {
                    map.removeLayer(regionLabelLayer);
                }
            }

            document
                .getElementById('check_region_labels')
                .addEventListener('change', () => {
                    updateRegionLabels();
                    writeShareParams();
                });
            updateRegionLabels();

            // When updating generator/seed:
            window.updateGenJs = function() {
                cancelAllTiles();
                const genId = Number(document.getElementById('genSelection').value);
                const seed = document.getElementById('seedValue').value.trim();
                appliedGenId = genId;

                checkIfSnowWorld(genId);
                checkIfSlimeChunks(genId);
                checkIfFarlands(genId);
                checkIfWorldBoundary(genId);

                // notify workers
                workers.forEach(w => {
                    w.postMessage({ type: 'updateGenAndSeed', seed, genId });
                });
                updateGenAndSeedMain(seed, genId);
                updateCenter();

                regenTiles(); // regenerate visible tiles
                updateSlimeLayer();
                updateFarlandsLayer();
                updateWorldBoundaryLayer();
                updateRegionLabels();
                writeShareParams();
            }
            
            document.getElementById('updateGen').addEventListener('click', updateGenJs);
            document.getElementById('xPos').addEventListener('change', setPosition);
            document.getElementById('zPos').addEventListener('change', setPosition);
            
            map.on('move',      updateCenter);
            map.on('moveend',   () => {
                clampViewToInt32();
                updateRegionLabels();
                scheduleShareParams();
            });
            map.on('zoomend',   () => {
                applyInt32CenterBounds();
                updateRegionLabels();
                scheduleShareParams();
            });
            map.on('resize',    applyInt32CenterBounds);
            map.on('zoomstart', () => {
                currentGenId++;
                dropQueuedJobs();
                abortBusyWorkers();
            });

            const DynamicLayer = L.GridLayer.extend({
                createTile: function(coords, done) {
                    const tile = document.createElement('canvas');
                    tile.width = scale;
                    tile.height = scale;
                    const layer = this;
                    const tileKey = `${coords.x},${coords.y},${coords.z}`;
                    let attempts = 0;

                    const load = () => {
                        requestTile(coords.x, coords.y, coords.z, scale).then((bytes) => {
                            const stillHere = layer._map && layer._map.getZoom() === coords.z;
                            if (bytes && tileKey === `${coords.x},${coords.y},${coords.z}`) {
                                const ctx = tile.getContext('2d');
                                const imageData = ctx.createImageData(scale, scale);
                                imageData.data.set(bytes);
                                ctx.putImageData(imageData, 0, 0);
                                done(null, tile);
                                return;
                            }
                            // Zoomed away: finish so Leaflet can drop this tile.
                            if (!stillHere) {
                                done(null, tile);
                                return;
                            }
                            // Still the active zoom but this request was aborted
                            // (worker recycle). Retry instead of showing a blank tile.
                            if (++attempts < 8) {
                                load();
                                return;
                            }
                            done(null, tile);
                        });
                    };
                    load();
                    return tile;
                }
            });

            // Add the graticule to your map
            const gridOverlay = new GridOverlay({
                pane: 'gridPane',
                tileSize: scale,
                scale: 1,
                minZoom: minZoom,
                maxZoom: 3,
                noWrap: true
            }).addTo(map);
            
            
            function refreshGridOverlay() {
                gridOverlay.redraw();
                writeShareParams();
            }
            
            document
                .getElementById('check_chunk_grid')
                .addEventListener('change', refreshGridOverlay);

            document
                .getElementById('check_region_grid')
                .addEventListener('change', refreshGridOverlay);

            function updateCrosshairVisibility() {
                const show = document.getElementById('check_crosshair')?.checked ?? true;
                crossH.style.display = show ? '' : 'none';
                crossV.style.display = show ? '' : 'none';
            }
            document.getElementById('check_crosshair')?.addEventListener('change', () => {
                updateCrosshairVisibility();
                writeShareParams();
            });
            updateCrosshairVisibility();

            [
                'viewMode',
                'colorMode',
                'check_blockcolors',
                'check_water',
                'check_snow_mode',
            ].forEach((id) => {
                document.getElementById(id)?.addEventListener('change', () => {
                    if (id === 'colorMode')
                        syncBlockColorsForColorMode();
                    cancelAllTiles();
                    regenTiles();
                    writeShareParams();
                });
            });
            
            document.getElementById('randomSeed').addEventListener('click', () => {
                // Generate a random 64-bit-range signed long, same as Java's world seeds
                const lo = Math.floor(Math.random() * 0x100000000);
                const hi = Math.floor(Math.random() * 0x100000000);
                const seed = BigInt.asIntN(64, (BigInt(hi) << 32n) | BigInt(lo));
                document.getElementById('seedValue').value = seed.toString();
                updateGenJs();
            });

            initWorkers('GoldenBase.js', () => {
                console.log('All workers ready');
                new DynamicLayer({
                    pane: 'tilePane',
                    tileSize: scale,
                    minZoom: minZoom,
                    maxZoom: 3,
                    noWrap: true,
                    keepBuffer: 8,
                    updateWhenZooming: false,
                }).addTo(map);

                let startZoom = Number.isFinite(share.zoom) ? share.zoom : tileZoom;
                if (startZoom < minZoom) startZoom = minZoom;
                if (startZoom > 2) startZoom = 2;
                map.setView([clampBlockCoord(share.z) * -1, clampBlockCoord(share.x)], startZoom);
                applyInt32CenterBounds();
                updateRegionLabels();
                updateCenter();
                writeShareParams();
            });
      }
  });
});