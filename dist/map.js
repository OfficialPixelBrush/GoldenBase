const WORKER_COUNT = navigator.hardwareConcurrency || 4;
const workers = [];
const queue = [];   // pending tile requests
const free = [];    // indices of idle workers

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
                minZoom: 0,
                maxZoom: 2,
                noWrap: true,
                keepBuffer: 10   // default is 2
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
                            <p style="margin:0;">Made by <a style="color: lightblue" href="https://pixelbrush.dev/about">Pixel Brush</a></p>
                        </td>
                        <td style="text-align: center;">
                            <p style="margin:0;"><a style="color: lightblue" href="https://github.com/OfficialPixelBrush/GoldenBase">Github Repository</a></p>
                        </td>
                    </tr>
                    <tr>
                        <td>
                            <code id="coords"></code>
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
                                <option value="3">a1.2.0 - b1.7.3</option>
                                <option disabled="true" value="4">inf-20100611 - a1.1.2_01</option>
                                <option value="2">inf-20100327</option>
                                <option value="1">inf-20100227</option>
                            </select>
                        </td>
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
                        <td>
                            <details>
                                <summary>Visualizer Settings</summary>
                                <input type="checkbox" id="check_heightmap" name="check_heightmap" value="Heightmap">
                                <label for="check_heightmap">Heightmap</label><br>
                                <input type="checkbox" id="check_blockcolors" name="check_blockcolors" value="Block colors">
                                <label for="check_blockcolors">Block colors</label><br>
                            </details>
                        </td>
                        <td>
                            <button id="updateGen">Update Gen</button>
                        </td>
                    </tr>
                    </table>
                `;
                return div;
            };

            infoControl.addTo(map);

            // Prevent clicks from propagating to the map
            L.DomEvent.disableClickPropagation(infoControl.getContainer());

            function updateCenter() {
                const center = map.getCenter();
                const point = map.project(center, tileZoom);

                mapCenter.x = point.x / scale;
                
                mapCenter.y = point.y / scale;

                document.getElementById('coords').textContent = `Center: ${(mapCenter.x*16*4).toFixed(2)}, ${(mapCenter.y*16*4).toFixed(2)}`;
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

            function clearOffscreenTiles() {
                const bounds = map.getBounds();
                const tileZoom = 0;

                // Filter the queue: keep only tiles inside current bounds
                for (let i = queue.length - 1; i >= 0; i--) {
                    const { x, y, z, id } = queue[i];
                    const latlng = map.unproject([x * scale, y * scale], tileZoom);
                    if (!bounds.contains(latlng)) {
                        queue.splice(i, 1);      // remove off-screen tile
                        delete pendingTiles[id]; // cancel its promise
                    }
                }
            }
            
            function regenTiles() {
                // regenerate currently visible tiles
                map.eachLayer(layer => {
                    if (layer instanceof L.GridLayer && layer._tiles) {
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
                currentGenId++; // increment generation
                const genId = Number(document.getElementById('genSelection').value);
                const seed = document.getElementById('seedValue').value.trim();

                // clear pending tiles and queue
                for (const k in pendingTiles) delete pendingTiles[k];
                queue.length = 0;

                // notify workers
                workers.forEach(w => {
                    w.postMessage({ type: 'updateGenAndSeed', seed, genId });
                });

                regenTiles(); // regenerate visible tiles
            }
            
            document.getElementById('updateGen').addEventListener('click', updateGenJs);
            document.getElementById('xPos').addEventListener('change', setPosition);
            document.getElementById('zPos').addEventListener('change', setPosition);
            
            map.on('move', updateCenter);

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

            initWorkers('GoldenBase.js', () => {
                console.log('All workers ready');
                new DynamicLayer({
                    tileSize: scale,
                    minZoom: 0,
                    maxZoom: 3,
                    noWrap: true,
                }).addTo(map);

                // Put desired position here
                map.setView([0, 0], tileZoom);
            });
      }
  });
});