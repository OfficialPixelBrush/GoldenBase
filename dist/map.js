const WORKER_COUNT = navigator.hardwareConcurrency || 4;
const workers = [];
const queue = [];   // pending tile requests
const free = [];    // indices of idle workers

const BIOME_SWATCH = {
    //None:           '#8db360',
    Plains:         '#8db360',
    Rainforest:     '#537b09',
    Swampland:      '#07f9b2',
    SeasonalForest: '#2d8e49',
    Forest:         '#056621',
    Savanna:        '#bdb25f',
    Shrubland:      '#b5db88',
    Desert:         '#fa9418',
    //Hell:           '#ff0000',
    //Sky:            '#4ee031',
    Taiga:          '#0b6659',
    //IceDesert:      '#c4d339',
    Tundra:         '#ffffff',
    //Invalid:              '#ff00ff'
};

function createBiomeSwatch(containerId) {
    const container = document.getElementById(containerId);

    Object.entries(BIOME_SWATCH).forEach(([name, color]) => {
        const row = document.createElement('div');
        row.style.display = 'flex';
        row.style.alignItems = 'center';
        row.style.marginBottom = '4px';

        const swatch = document.createElement('div');
        swatch.style.width = '16px';
        swatch.style.height = '16px';
        swatch.style.background = color;
        swatch.style.border = '1px solid #444';
        swatch.style.marginRight = '8px';

        const label = document.createElement('code');
        label.textContent = `${name}`;

        row.appendChild(swatch);
        row.appendChild(label);

        container.appendChild(row);
    });
}

function initWorkers(wasmJsUrl, onReady) {
    let readyCount = 0;
    for (let i = 0; i < WORKER_COUNT; i++) {
        const w = new Worker('tile-worker.js');
        workers.push(w);

        w.onmessage = (e) => {
            if (e.data.type === 'ready') {
                free.push(i);
                if (++readyCount === WORKER_COUNT) onReady();
                return;
            }
            if (e.data.type === 'tile') {
                // Resolve the promise for this tile
                const { id, bytes } = e.data;
                pendingTiles[id]?.(bytes);
                delete pendingTiles[id];

                // Mark worker free, drain queue
                free.push(i);
                dispatch();
            }
        };

        w.postMessage({ type: 'init', wasmJsUrl });
    }
}

const pendingTiles = {}; // id → resolve function
let tileIdCounter = 0;
let currentGenId = 0;

function getOptions() {
    let opt_value = 0;
    if (document.getElementById('check_heightmap').checked) opt_value |= 1 << 0;
    if (document.getElementById('check_blockcolors').checked) opt_value |= 1 << 1;
    if (document.getElementById('check_water').checked) opt_value |= 1 << 2;
    if (document.getElementById('check_snow_mode').checked) opt_value |= 1 << 3;
    if (document.getElementById('check_snow_world').checked) opt_value |= 1 << 4;
    return opt_value;
}

function requestTile(x, y, z, tileSize) {
    const genId = currentGenId; // capture current generation
    return new Promise((resolve) => {
        const id = tileIdCounter++;
        pendingTiles[id] = (bytes) => {
            if (genId === currentGenId) resolve(bytes);
            delete pendingTiles[id];
        };
        queue.push({ x, y, z, id, tileSize, options: getOptions() });
        dispatch();
    });
}

function dispatch() {
    while (free.length > 0 && queue.length > 0) {
        const workerIdx = free.pop();
        const job = queue.shift();
        workers[workerIdx].postMessage({
            type: 'getTile',
            ...job
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

        // region grid
        if (showRegionGrid) {
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

let mapCenter = { x: 0, y: 0 };

window.addEventListener('load', () => {
  createModule({
      onRuntimeInitialized: function() {
            const scale = 16*4; // pixels per block
            const Module = this;
            window.Module = Module;

            const tileZoom = 0; // your tiles exist only at this zoom

            const map = L.map('map', {
                crs: L.CRS.Simple,
                minZoom: -4,  // allows zooming out 4 levels (matches MAX_ZOOM_OUT in main.cpp)
                maxZoom: 2,
                noWrap: true,
                keepBuffer: 10   // default is 2
            });
            map.createPane('gridPane');
            map.getPane('gridPane').style.zIndex = 700;

            map.createPane('tilePane');
            map.getPane('tilePane').style.zIndex = 400;
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
                            <p style="margin:0;">Made by <a style="color: lightblue" href="https://pixelbrush.dev/about">Pixel Brush</a></p>
                        </td>
                        <td style="text-align: center;">
                            <p style="margin:0;"><a style="color: lightblue" href="https://github.com/OfficialPixelBrush/GoldenBase">Github Repository</a></p>
                        </td>
                    </tr>
                    <tr>
                        <td>
                            <code id="coords"></code>
                            </br>
                            <code id="bigCoords"></code>
                        </td>
                        <td>
                            <input type="number" id="xPos" placeholder="x" value="0" style="width: 20%">
                            <input type="number" id="zPos" placeholder="z" value="0" style="width: 20%">
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
                            <label for="check_snow_world">Snow World<br>
                        <td>
                    </tr>
                    <tr>
                        <td>
                            <label>Seed</label>
                        </td>
                        <td>
                            <input id="seedValue" value="3257840388504953787">
                        </td>
                    </tr>
                    <tr>
                        <td colspan="2">
                            <button id="updateGen" style="width:100%">Update Gen</button>
                        </td>
                    </tr>
                    <tr>
                        <td style="vertical-align: top;">
                            <details>
                                <summary>Visualizer Settings</summary>
                                <input type="checkbox" id="check_heightmap" name="check_heightmap" checked="true">
                                <label for="check_heightmap">Heightmap</label><br>
                                <input type="checkbox" id="check_blockcolors" name="check_blockcolors" checked="true">
                                <label for="check_blockcolors">Block colors</label><br>
                                <input type="checkbox" id="check_water" name="check_water" checked="true">
                                <label for="check_water">Show Water</label><br>
                                <input type="checkbox" id="check_snow_mode" name="check_snow_mode" checked="true">
                                <label for="check_snow_mode">Show surface snow<br>
                                <input type="checkbox" id="check_chunk_grid" name="check_chunk_grid" checked="true">
                                <label for="check_chunk_grid">Show Chunk Grid</label><br>
                                <input type="checkbox" id="check_region_grid" name="check_region_grid" checked="true">
                                <label for="check_region_grid">Show Region Grid</label><br>
                            </details>
                        </td>
                        <td style="vertical-align: top;">
                            <details>
                                <summary>Biome Colors (a1.2.0+)</summary>
                                <div id="biomeSwatches"></div>
                            </details>
                        </td>
                    </tr>
                    </table>
                `;
                return div;
            };
            infoControl.addTo(map);
            createBiomeSwatch('biomeSwatches');

            function checkIfSnowWorld(genId) {
                if (genId == 7) {
                    snowWorldRow.style.display = "";
                    return;
                }
                snowWorldRow.style.display = "none";
            }
            checkIfSnowWorld(9);
            document
                .getElementById('genSelection')
                .addEventListener('change', (e) => {
                    checkIfSnowWorld(Number(e.target.value));
                });

            // Prevent clicks from propagating to the map
            L.DomEvent.disableClickPropagation(infoControl.getContainer());

            function cleanZero(n) {
                return Math.abs(n) < 1e-9 ? 0 : n;
            }

            function updateCenter() {
                const center = map.getCenter();
                const point = map.project(center, tileZoom);

                mapCenter.x = point.x / scale;
                
                mapCenter.y = point.y / scale;

                const blockPosX = mapCenter.x*16*4;
                const blockPosZ = mapCenter.y*16*4;
                document.getElementById('coords').textContent = `Center: ${(blockPosX).toFixed(2)}, ${(blockPosZ).toFixed(2)}`;
                document.getElementById('bigCoords').textContent = `Cnk: ${((blockPosX/16)-0.5).toFixed(0)}, ${((blockPosZ/16)-0.5).toFixed(0)} / Rgn: ${((blockPosX/512)-0.5).toFixed(0)}, ${((blockPosZ/512)-0.5).toFixed(0)}`;
            }

            window.setPosition = function() {
                map.setView(
                    [
                        Number(document.getElementById('zPos').value)*-1,
                        Number(document.getElementById('xPos').value)
                    ]
                );

                clearOffscreenTiles();
            };

            function cancelAllTiles() {
                currentGenId++;                            // invalidates all in-flight promises
                queue.length = 0;                          // discard queued jobs
                for (const k in pendingTiles) delete pendingTiles[k]; // discard callbacks
            }

            function clearOffscreenTiles() {
                const bounds = map.getBounds();
                const currentZoom = map.getZoom();

                // Filter the queue: keep only tiles inside current bounds
                for (let i = queue.length - 1; i >= 0; i--) {
                    const { x, y, z, id } = queue[i];
                    const latlng = map.unproject([x * scale, y * scale], currentZoom);
                    if (!bounds.contains(latlng)) {
                        queue.splice(i, 1);      // remove off-screen tile
                        delete pendingTiles[id]; // cancel its promise
                    }
                }
            }
            
            function regenTiles() {
                // regenerate currently visible tiles
                map.eachLayer(layer => {
                    if (
                        layer instanceof L.GridLayer &&
                        layer._tiles &&
                        layer.options.pane === "tilePane"
                    ) {
                        Object.values(layer._tiles).forEach(tileObj => {
                            const coords = tileObj.coords;
                            // remove old tile promise to force regeneration
                            const tile = tileObj.el;
                            requestTile(coords.x, coords.y, coords.z, tile.width).then((bytes) => {
                                const ctx = tile.getContext('2d');
                                const imageData = ctx.createImageData(tile.width, tile.height);
                                imageData.data.set(bytes);
                                ctx.putImageData(imageData, 0, 0);
                            });
                        });
                    }
                });
            }

            // When updating generator/seed:
            window.updateGenJs = function() {
                cancelAllTiles();
                const genId = Number(document.getElementById('genSelection').value);
                const seed = document.getElementById('seedValue').value.trim();

                checkIfSnowWorld(genId);

                // notify workers
                workers.forEach(w => {
                    w.postMessage({ type: 'updateGenAndSeed', seed, genId });
                });

                regenTiles(); // regenerate visible tiles
            }
            
            document.getElementById('updateGen').addEventListener('click', updateGenJs);
            document.getElementById('xPos').addEventListener('change', setPosition);
            document.getElementById('zPos').addEventListener('change', setPosition);
            
            map.on('move',      updateCenter);
            map.on('zoomstart', cancelAllTiles);
            map.on('zoomend',   regenTiles);

            const DynamicLayer = L.GridLayer.extend({
                createTile: function(coords, done) {
                    const tile = document.createElement('canvas');
                    tile.width = scale;
                    tile.height = scale;
                    const ctx = tile.getContext('2d');

                    const tileKey = `${coords.x},${coords.y},${coords.z}`; // unique key

                    requestTile(coords.x, coords.y, coords.z, scale).then((bytes) => {
                        // Only draw if tile still matches the coords (prevents old tiles overwriting)
                        if (tileKey === `${coords.x},${coords.y},${coords.z}`) {
                            const imageData = ctx.createImageData(scale, scale);
                            imageData.data.set(bytes);
                            ctx.putImageData(imageData, 0, 0);
                            done(null, tile);
                        }
                    });

                    return tile;
                }
            });

            // Add the graticule to your map
            const gridOverlay = new GridOverlay({
                pane: 'gridPane',
                tileSize: scale,
                scale: 1,
                minZoom: -4,
                maxZoom: 3,
                noWrap: true
            }).addTo(map);
            const crossH = L.DomUtil.create('div', '', document.body);
            const crossV = L.DomUtil.create('div', '', document.body);
            
            
            function refreshGridOverlay() {
                gridOverlay.redraw();
            }
            
            document
                .getElementById('check_chunk_grid')
                .addEventListener('change', refreshGridOverlay);

            document
                .getElementById('check_region_grid')
                .addEventListener('change', refreshGridOverlay);

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

            initWorkers('GoldenBase.js', () => {
                console.log('All workers ready');
                new DynamicLayer({
                    pane: 'tilePane',
                    tileSize: scale,
                    minZoom: -4,  // matches map minZoom and MAX_ZOOM_OUT in main.cpp
                    maxZoom: 3,
                    noWrap: true,
                }).addTo(map);

                // Put desired position here
                map.setView([0, 0], tileZoom);
            });
      }
  });
});