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

function requestTile(x, y, z, tileSize) {
    const genId = currentGenId; // capture current generation
    return new Promise((resolve) => {
        const id = tileIdCounter++;
        pendingTiles[id] = (bytes) => {
            if (genId === currentGenId) resolve(bytes); // only accept if generation matches
        };
        queue.push({ x, y, z, id, tileSize });
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
                maxZoom: 3,
                noWrap: true,
                keepBuffer: 5   // default is 2
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

            L.polyline([[0,-map.getSize().y],[0, map.getSize().y]], {color: 'red'}).addTo(map);
            L.polyline([[-map.getSize().x,0],[map.getSize().x, 0]], {color: 'blue'}).addTo(map);

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
                        Number(document.getElementById('zPos').value),
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