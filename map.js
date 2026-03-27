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

function StringToHash(value) {
    let h = 0;
    if (value.length > 0) {
        for (let i = 0; i < value.length; i++) {
            h = (31 * h + value.charCodeAt(i)) | 0;
        }
    }
    return h;
}

function updateSeedJs() {
    seed = 0
    tmpSeed = document.getElementById('seedValue').value
    console.log(tmpSeed);
    if (!isNaN(tmpSeed) && tmpSeed.trim() !== "") {
        seed = Number(tmpSeed)
    } else {
        seed = StringToHash(tmpSeed);
    }
    
    for (const k in pendingTiles) delete pendingTiles[k];
    queue.length = 0;

    // notify workers
    workers.forEach(w => {
        w.postMessage({ type: 'updateSeed', seed });
    });
}

// X,Y -> Chunk X,Z
// Z = Zoom Level
function requestTile(x, y, z, tileSize) {
    return new Promise((resolve) => {
        const id = tileIdCounter++;
        pendingTiles[id] = resolve;
        queue.push({ x, y, z, id, tileSize, heightShade: true, heightMap: true});
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
            const scale = 16*8; // pixels per block
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

            map.on('mousemove', function(e) {
                document.getElementById('coords').textContent = `Center: ${(mapCenter.x*16).toFixed(2)}, ${(mapCenter.y*16).toFixed(2)}`;
            });

            function updateCenter() {
                const center = map.getCenter();

                // convert lat/lng → pixel → tile coords
                const point = map.project(center, tileZoom);

                mapCenter.x = point.x / scale;
                mapCenter.y = point.y / scale;
            }

            window.setPosition = function() {
                map.setView(
                    [
                        Number(document.getElementById('xPos').value),
                        Number(document.getElementById('zPos').value)
                    ]
                );
            };
            
            map.on('move', updateCenter);

            const DynamicLayer = L.GridLayer.extend({
                createTile: function(coords, done) {
                    const tile = document.createElement('canvas');
                    tile.width = scale;
                    tile.height = scale;
                    const ctx = tile.getContext('2d');

                    requestTile(coords.x, coords.y, coords.z, scale).then((bytes) => {
                        const imageData = ctx.createImageData(scale, scale);
                        imageData.data.set(bytes);
                        ctx.putImageData(imageData, 0, 0);
                        done(null, tile);
                    });

                    return tile;
                }
            });

            initWorkers('build/GoldenBase.js', () => {
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