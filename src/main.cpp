#include <stdint.h>
#include <math.h>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <emscripten.h>
#include <string>
#include "./generators/beta/b173/generatorBeta173.h"
#include "./generators/alpha/a112_01/generatorAlpha112_01.h"
#include "./generators/infdev/inf20100227/generatorInfdev20100227.h"
#include "./generators/infdev/inf20100327/generatorInfdev20100327.h"
#include "./generators/infdev/inf20100420/generatorInfdev20100420.h"
#include "./generators/infdev/inf20100611/generatorInfdev20100611.h"
#include "biomeColors.h"
#include "blocks.h"
#include "noiseLod.h"
#include "javaMath.h"

#define TILE_PIXELS 256
// Negative zoomLevel zooms out: each step doubles the number of chunks per tile side.
// MAX_ZOOM_OUT 18 → 256 * 2^18 = 67,108,864 blocks per tile, enough to see
// the inf-20100227 stone-wall Far Lands at ±33,554,432 from spawn.
#define MAX_ZOOM_OUT 18

static uint8_t buffer[TILE_PIXELS * TILE_PIXELS * 4];
static TileColumn columns[TILE_PIXELS * TILE_PIXELS];
static int8_t tileHeights[TILE_PIXELS * TILE_PIXELS];
static uint8_t tileLiquid[TILE_PIXELS * TILE_PIXELS];
// The output tile is always TILE_PIXELS square, regardless of zoom level.

static uint8_t clamp(double v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

EM_JS(void, emscripten_expose_heap, (), {
    if (typeof Module !== 'undefined') {
        Module.HEAP8 = HEAP8;
        Module.HEAP16 = HEAP16;
        Module.HEAPU8 = HEAPU8;
        Module.HEAPU16 = HEAPU16;
        Module.HEAP32 = HEAP32;
        Module.HEAPU32 = HEAPU32;
        Module.HEAPF32 = HEAPF32;
        Module.HEAPF64 = HEAPF64;
        Module.HEAP64 = HEAP64;
        Module.HEAPU64 = HEAPU64;
    }
});

float HeightToFloat(int height) {
    return float(height) / float(CHUNK_HEIGHT);
}

float Int8ToFloat(int range) {
    return float(range) / 255.0f;
}

uint8_t FloatToInt8(float range) {
    return clamp(range * 255.0f);
}

Int3 MultiplyColor(Int3 a, Int3 b) {
    return Int3{
        FloatToInt8(Int8ToFloat(a.x) * Int8ToFloat(b.x)),
        FloatToInt8(Int8ToFloat(a.y) * Int8ToFloat(b.y)),
        FloatToInt8(Int8ToFloat(a.z) * Int8ToFloat(b.z))
    };
}

Int3 HexToInt3(int32_t value) {
    return Int3{
        (value >> 16) & 0xFF,
        (value >> 8) & 0xFF,
        value & 0xFF
    };
}

Int3 LerpInt3(Int3 a, Int3 b, float t) {
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;
    return Int3{
        FloatToInt8(Int8ToFloat(a.x) + (Int8ToFloat(b.x) - Int8ToFloat(a.x)) * t),
        FloatToInt8(Int8ToFloat(a.y) + (Int8ToFloat(b.y) - Int8ToFloat(a.y)) * t),
        FloatToInt8(Int8ToFloat(a.z) + (Int8ToFloat(b.z) - Int8ToFloat(a.z)) * t)
    };
}

// Hypsometric (elevation) tint for the topographic map mode.
Int3 TopographicColor(int height, bool water, bool ice) {
    if (ice)
        return HexToInt3(0xc5e8f7);
    if (water)
        return HexToInt3(0x3d7ea6);

    struct Stop {
        int y;
        int32_t hex;
    };
    static const Stop stops[] = {
        {0,   0x0a2850},
        {32,  0x1d5a78},
        {62,  0xd4c882},
        {64,  0x3d8c37},
        {72,  0x6bb34a},
        {84,  0xb8c74a},
        {96,  0xc89646},
        {108, 0xa07850},
        {116, 0x8a7a70},
        {127, 0xf2f0ec},
    };
    const int n = int(sizeof(stops) / sizeof(stops[0]));
    if (height <= stops[0].y)
        return HexToInt3(stops[0].hex);
    if (height >= stops[n - 1].y)
        return HexToInt3(stops[n - 1].hex);
    for (int i = 1; i < n; ++i) {
        if (height <= stops[i].y) {
            const float t = float(height - stops[i - 1].y) / float(stops[i].y - stops[i - 1].y);
            return LerpInt3(HexToInt3(stops[i - 1].hex), HexToInt3(stops[i].hex), t);
        }
    }
    return HexToInt3(stops[n - 1].hex);
}

void ApplyTopoHillshade() {
    const float lx = -0.57735027f;
    const float ly = 0.57735027f;
    const float lz = -0.57735027f;
    for (int z = 0; z < TILE_PIXELS; ++z) {
        const int z0 = z > 0 ? z - 1 : z;
        const int z1 = z < TILE_PIXELS - 1 ? z + 1 : z;
        for (int x = 0; x < TILE_PIXELS; ++x) {
            const int i = z * TILE_PIXELS + x;
            const int x0 = x > 0 ? x - 1 : x;
            const int x1 = x < TILE_PIXELS - 1 ? x + 1 : x;
            float dzdx = float(tileHeights[z * TILE_PIXELS + x1] - tileHeights[z * TILE_PIXELS + x0]);
            float dzdz = float(tileHeights[z1 * TILE_PIXELS + x] - tileHeights[z0 * TILE_PIXELS + x]);
            if (x1 != x0)
                dzdx /= float(x1 - x0);
            if (z1 != z0)
                dzdz /= float(z1 - z0);
            const float nx = -dzdx;
            const float ny = 1.8f;
            const float nz = -dzdz;
            const float invLen = 1.0f / sqrtf(nx * nx + ny * ny + nz * nz);
            float shade = (nx * lx + ny * ly + nz * lz) * invLen;
            shade = 0.42f + 0.58f * shade;
            if (shade < 0.22f)
                shade = 0.22f;
            if (tileLiquid[i])
                shade = 0.82f + 0.18f * shade;
            const int idx = i * 4;
            buffer[idx + 0] = clamp(float(buffer[idx + 0]) * shade);
            buffer[idx + 1] = clamp(float(buffer[idx + 1]) * shade);
            buffer[idx + 2] = clamp(float(buffer[idx + 2]) * shade);
        }
    }
}

Int3 ColorModeTint(int colorMode, Biome biome, float temperature, float humidity) {
    if (colorMode == 2)
        return GetGrassColor(temperature, humidity);
    if (colorMode == 1)
        return GetBiomeColor(biome);
    return GetBiomeColor(BIOME_NONE);
}

bool IsWaterOrIce(int block_id) {
    return block_id == BLOCK_WATER_STILL || block_id == BLOCK_WATER_FLOWING || block_id == BLOCK_ICE;
}

Int3 GetBlockColor(int block_id, Int3 biomeColor, bool darkerGrass) {
    switch(block_id) {
        default:
            return Int3{255, 0, 255};
        case BLOCK_ORE_GOLD:
        case BLOCK_ORE_IRON:
        case BLOCK_ORE_DIAMOND:
        case BLOCK_ORE_REDSTONE_ON:
        case BLOCK_ORE_REDSTONE_OFF:
        case BLOCK_ORE_COAL:
        case BLOCK_ORE_LAPIS_LAZULI:
        case BLOCK_STONE:           return HexToInt3(0x7f7f7f);
        case BLOCK_GRASS:           return darkerGrass ? MultiplyColor(biomeColor, HexToInt3(0x929292)) : biomeColor;
        case BLOCK_DIRT:            return HexToInt3(0x79553a);
        case BLOCK_COBBLESTONE:     return HexToInt3(0x898989);
        case BLOCK_PLANKS:          return HexToInt3(0xbc9862);
        case BLOCK_BEDROCK:         return HexToInt3(0x575757);
        case BLOCK_WATER_FLOWING:
        case BLOCK_WATER_STILL:     return HexToInt3(0x1f55ff);
        case BLOCK_LAVA_FLOWING:
        case BLOCK_LAVA_STILL:      return HexToInt3(0xfc5700);
        case BLOCK_SAND:            return HexToInt3(0xded7a1);
        case BLOCK_GRAVEL:          return HexToInt3(0x8f7875);
        case BLOCK_LOG:             return HexToInt3(0x8f7875);
        case BLOCK_SPONGE:          return HexToInt3(0xc7c73f);
        case BLOCK_GLASS:           return HexToInt3(0xfefefe);
        case BLOCK_SANDSTONE:       return HexToInt3(0xded5a6);
        case BLOCK_BRICKS:          return HexToInt3(0x7c4536);
        case BLOCK_TNT:             return HexToInt3(0xdb441a);
        case BLOCK_OBSIDIAN:        return HexToInt3(0x0e0e16);
        case BLOCK_IRON:            return HexToInt3(0xd1d1d1);
        case BLOCK_GOLD:            return HexToInt3(0xe7c845);
        case BLOCK_DIAMOND:         return HexToInt3(0x00bdb3);
        case BLOCK_ICE:             return HexToInt3(0x77a9ff);
        case BLOCK_CLAY:            return HexToInt3(0xa0a7b2);
        case BLOCK_SNOW:            return HexToInt3(0xffffff);
        case BLOCK_SNOW_LAYER:      return HexToInt3(0xffffff);
        case BLOCK_DANDELION:       return HexToInt3(0xdee602);
    }
}

enum genSelect {
    GEN_BETA_BETA173            = 9, // Beta 1.3.0   - Beta 1.7.3
    GEN_ALPHA_ALPHA120          = 8, // Alpha 1.2.0  - Beta 1.2.0_02
    GEN_ALPHA_ALPHA112_01       = 7, // Inf-20100624 - Alpha 1.1.2_01
    GEN_INFDEV_INFDEV20100616   = 6, // Inf-20100616 - Inf-20100618
    GEN_INFDEV_INFDEV20100611   = 5, // Inf-20100611 - Inf-20100615
    GEN_INFDEV_INFDEV20100420   = 4, // Inf-20100420 - Inf-20100608
    GEN_INFDEV_INFDEV20100413   = 3, // Inf-20100413 - Inf-20100415
    GEN_INFDEV_INFDEV20100327   = 2, // Inf-20100327 - Inf-20100330
    GEN_INFDEV_INFDEV20100227   = 1, // Inf-20100227 - Inf-20100325
    GEN_INVALID                 = 0,
};

extern "C" {
    int64_t currentSeed = 3257840388504953787;
    genSelect activeGenId = GEN_BETA_BETA173;
    Generator* generatorPtr = nullptr;

    EMSCRIPTEN_KEEPALIVE
    int getTileSize() {
        return TILE_PIXELS;
    }

    EMSCRIPTEN_KEEPALIVE
    int getMaxZoomOut() {
        return MAX_ZOOM_OUT;
    }

    EMSCRIPTEN_KEEPALIVE
    int getBiomeAt(int32_t blockX, int32_t blockZ) {
        if (!generatorPtr)
            return int(BIOME_NONE);
        TileColumn col;
        generatorPtr->SetDetailLevel(1);
        generatorPtr->SampleColumns(blockX, blockZ, 1, 1, &col);
        return int(col.biome);
    }

    EMSCRIPTEN_KEEPALIVE
    void UpdateGenAndSeed(const char* seed_cstr, int genId = GEN_BETA_BETA173) {
        std::string seedString = std::string(seed_cstr);
        currentSeed = 0;
        
        bool isNumber = !seedString.empty() && 
            (seedString[0] == '-' || seedString[0] == '+' || std::isdigit(seedString[0])) &&
            std::all_of(seedString.begin() + 1, seedString.end(), ::isdigit);

        if (isNumber) {
            currentSeed = std::strtoll(seedString.c_str(), nullptr, 10);
        } else {
            currentSeed = int64_t(HashCode(seedString));
        }
        
        activeGenId = static_cast<genSelect>(genId);

        // Helper: allocate a generator for the given genId, seed, and multiplier
        auto makeGenerator = [&](float mult) -> Generator* {
            switch(activeGenId) {
                default:
                case GEN_BETA_BETA173:
                    return new GeneratorBeta173(currentSeed, mult);
                case GEN_ALPHA_ALPHA120: {
                    auto* g = new GeneratorBeta173(currentSeed, mult);
                    g->gravelFix = false;
                    return g;
                }
                case GEN_INFDEV_INFDEV20100227:
                    return new GeneratorInfdev20100227(currentSeed, mult);
                case GEN_INFDEV_INFDEV20100327:
                    return new GeneratorInfdev20100327(currentSeed, mult);
                case GEN_INFDEV_INFDEV20100413: {
                    auto* g = new GeneratorInfdev20100327(currentSeed, mult);
                    g->infdev20100413 = true;
                    return g;
                }
                case GEN_INFDEV_INFDEV20100420:
                    return new GeneratorInfdev20100420(currentSeed, mult);
                case GEN_INFDEV_INFDEV20100611:
                    return new GeneratorInfdev20100611(currentSeed, mult);
                case GEN_INFDEV_INFDEV20100616: {
                    auto* g = new GeneratorInfdev20100611(currentSeed, mult);
                    g->infdev20100616 = true;
                    return g;
                }
                case GEN_ALPHA_ALPHA112_01:
                    return new GeneratorAlpha112_01(currentSeed, mult);
            }
        };

        // Replace generator (octave LOD is applied per tile via SetDetailLevel)
        if (generatorPtr) { delete generatorPtr; generatorPtr = nullptr; }
        generatorPtr = makeGenerator(1.0);
    }
    
    /*
        OPTIONS BITMASK
        1   -> Heightmap, if the color should be multiplied by the height (darker at low y, brighter at high y)
        2   -> Block Colors, if not set, biome colors are used for everything
        4   -> Show Water, if water should be rendered
        8   -> Snow Mode, snow should be rendered
        16  -> Snow World, if world is snow world
        32  -> Color mode biome (simplified distinct biome colors)
        64  -> Hillshade (slope shading; does not change the color mode)
        128 -> Color mode accurate (vanilla temp/humidity grass tint)
        256 -> Color mode topology (hypsometric elevation tint)
        ...
        Color mode none: neither 32, 128, nor 256 (plain pre-biome green)
    */

    EMSCRIPTEN_KEEPALIVE
    uint8_t* getTile(int x, int z, int zoomLevel, int32_t options) {
        bool heightmap      = (options &  1) > 0;
        bool blockColors    = (options &  2) > 0;
        bool showWater      = (options &  4) > 0;
        bool snowMode       = (options &  8) > 0;
        bool snowWorld      = (options & 16) > 0;
        bool colorBiome     = (options & 32) > 0;
        bool hillshade      = (options & 64) > 0;
        bool colorAccurate  = (options & 128) > 0;
        bool colorTopology  = (options & 256) > 0;

        // ── Zoom-level semantics ──────────────────────────────────────────────
        // zoomLevel > 0  : zoomed IN  — fewer chunks, each block drawn at scale×scale pixels
        //   batchSize = (TILE_PIXELS/16) >> zoomLevel  (min 1)
        //   scale     = 1 << zoomLevel
        //   stride    = 1  (every block is sampled, full octaves)
        //
        // zoomLevel = 0  : 1:1 — TILE_PIXELS/16 chunks per side, 1px per block, full detail
        //
        // zoomLevel < 0  : zoomed OUT — more world per tile, 1px per sampled block
        //   stride    = 1 << (-zoomLevel)
        //   Fine octaves are dropped; terrain is sampled as a single noise field
        //   instead of generating full voxel chunks.
        //
        // The output tile is always TILE_PIXELS² pixels.
        // ─────────────────────────────────────────────────────────────────────

        const int chunksAtZoom0 = TILE_PIXELS / CHUNK_WIDTH;
        const int tileWidth = TILE_PIXELS;

        int batchSize, scale, stride;
        Generator* gen = generatorPtr;
        if (!gen)
            return buffer;

        if (zoomLevel >= 0) {
            batchSize = chunksAtZoom0 >> zoomLevel;
            scale     = 1 << zoomLevel;
            stride    = 1;
            if (batchSize < 1) {
                batchSize = 1;
                scale = chunksAtZoom0;
            }
        } else {
            int zoomOut = -zoomLevel;
            if (zoomOut > MAX_ZOOM_OUT)
                zoomOut = MAX_ZOOM_OUT;
            batchSize = chunksAtZoom0 << zoomOut;
            scale     = 1;
            stride    = 1 << zoomOut;
        }

        gen->SetDetailLevel(stride);
        gen->snowMode = snowMode;
        if (auto* alphaGen = dynamic_cast<GeneratorAlpha112_01*>(gen))
            alphaGen->snowCovered = snowWorld;
        bool darkerGrass = dynamic_cast<GeneratorBeta173*>(gen) != nullptr;
        const int colorMode = colorTopology ? 3 : (colorAccurate ? 2 : (colorBiome ? 1 : 0));

        auto writePixel = [&](int outX, int outZ, int topY, int surface_block_id, Biome biome, float temperature,
                              float humidity, bool forceMaterial = false) {
            if (!showWater && IsWaterOrIce(surface_block_id))
                surface_block_id = int(GetFillerBlock(biome));
            const bool isWater = surface_block_id == BLOCK_WATER_STILL || surface_block_id == BLOCK_WATER_FLOWING;
            const bool isIce = surface_block_id == BLOCK_ICE;
            float fr, fg, fb;
            const bool showLiquid = showWater && (isWater || isIce);
            const bool isSnow = surface_block_id == BLOCK_SNOW_LAYER || surface_block_id == BLOCK_SNOW;
            const bool useMaterial = blockColors || forceMaterial;
            if (showLiquid && isWater) {
                fr = 0.0f; fg = 0.0f; fb = 1.0f;
            } else if (showLiquid && isIce) {
                fr = 0.5f; fg = 0.8f; fb = 1.0f;
            } else if (snowMode && isSnow) {
                fr = 1.0f; fg = 1.0f; fb = 1.0f;
            } else if (colorMode == 3 && !useMaterial) {
                Int3 topo = TopographicColor(topY, false, false);
                fr = Int8ToFloat(topo.x);
                fg = Int8ToFloat(topo.y);
                fb = Int8ToFloat(topo.z);
            } else {
                Int3 tint = ColorModeTint(colorMode, biome, temperature, humidity);
                const bool accurateBeta = colorMode == 2 && darkerGrass;
                if (useMaterial) {
                    Int3 blockColor = GetBlockColor(surface_block_id, tint, accurateBeta);
                    fr = Int8ToFloat(blockColor.x);
                    fg = Int8ToFloat(blockColor.y);
                    fb = Int8ToFloat(blockColor.z);
                } else {
                    if (accurateBeta)
                        tint = MultiplyColor(tint, HexToInt3(0x929292));
                    fr = Int8ToFloat(tint.x);
                    fg = Int8ToFloat(tint.y);
                    fb = Int8ToFloat(tint.z);
                }
            }
            if (heightmap && !hillshade) {
                float heightFloat = (HeightToFloat(topY) * 1.5f);
                float gamma = 0.9f;
                float shadedHeight = powf(heightFloat, gamma);
                fr *= shadedHeight;
                fg *= shadedHeight;
                fb *= shadedHeight;
            }
            uint8_t r = FloatToInt8(fr);
            uint8_t g = FloatToInt8(fg);
            uint8_t b = FloatToInt8(fb);
            const uint8_t liquid = showLiquid ? 1 : 0;
            const int8_t storedHeight = int8_t(std::max(0, std::min(127, topY)));
            if (scale > 1) {
                int originX = outX * scale;
                int originZ = outZ * scale;
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        const int pix = (originZ + sy) * tileWidth + (originX + sx);
                        int idx = pix * 4;
                        buffer[idx + 0] = r;
                        buffer[idx + 1] = g;
                        buffer[idx + 2] = b;
                        buffer[idx + 3] = 255;
                        tileHeights[pix] = storedHeight;
                        tileLiquid[pix] = liquid;
                    }
                }
            } else {
                const int pix = outZ * tileWidth + outX;
                int idx = pix * 4;
                buffer[idx + 0] = r;
                buffer[idx + 1] = g;
                buffer[idx + 2] = b;
                buffer[idx + 3] = 255;
                tileHeights[pix] = storedHeight;
                tileLiquid[pix] = liquid;
            }
        };

        if (stride > 1) {
            const int32_t originX = int32_t(int64_t(x) * int64_t(batchSize) * CHUNK_WIDTH);
            const int32_t originZ = int32_t(int64_t(z) * int64_t(batchSize) * CHUNK_WIDTH);
            gen->SampleColumns(originX, originZ, TILE_PIXELS, stride, columns);
            const int32_t farlandsAt = (activeGenId == GEN_INFDEV_INFDEV20100227) ? 33554432
                                     : (activeGenId != GEN_INVALID) ? 12550821 : 0;
            const bool stoneWallFarlands = activeGenId == GEN_INFDEV_INFDEV20100227;
            for (int pz = 0; pz < TILE_PIXELS; ++pz) {
                for (int px = 0; px < TILE_PIXELS; ++px) {
                    TileColumn col = columns[pz * TILE_PIXELS + px];
                    bool forceMaterial = false;
                    if (farlandsAt > 0) {
                        const int64_t wx = int64_t(originX) + int64_t(px) * stride;
                        const int64_t wz = int64_t(originZ) + int64_t(pz) * stride;
                        const int64_t ax = wx < 0 ? -wx : wx;
                        const int64_t az = wz < 0 ? -wz : wz;
                        if (ax >= farlandsAt || az >= farlandsAt) {
                            col.height = CHUNK_HEIGHT - 1;
                            if (stoneWallFarlands) {
                                col.surface = BLOCK_STONE;
                                col.biome = BIOME_NONE;
                                forceMaterial = true;
                            } else {
                                col.surface = GetTopBlock(col.biome);
                            }
                        }
                    }
                    int surface = col.surface;
                    if (!showWater && IsWaterOrIce(surface))
                        surface = GetFillerBlock(col.biome);
                    if (blockColors && surface == BLOCK_AIR)
                        surface = BLOCK_STONE;
                    writePixel(px, pz, col.height, surface, col.biome, col.temperature, col.humidity,
                               forceMaterial);
                }
            }
            if (hillshade)
                ApplyTopoHillshade();
            return buffer;
        }

        for (int bx = 0; bx < batchSize; bx++) {
            for (int bz = 0; bz < batchSize; bz++) {
                Chunk chunk = gen->GenerateChunk(Int2{
                    x * batchSize + bx,
                    z * batchSize + bz
                });

                for (int px = 0; px < CHUNK_WIDTH; px++) {
                    for (int pz = 0; pz < CHUNK_WIDTH; pz++) {
                        int topY = chunk.GetHeightValue(px, pz);
                        int cover = chunk.GetBlockType(Int3{px, topY, pz});
                        int below = topY > 0 ? chunk.GetBlockType(Int3{px, topY - 1, pz}) : BLOCK_AIR;
                        int surface_block_id = cover;
                        const bool coverLiquid = IsWaterOrIce(cover);
                        const bool belowLiquid = IsWaterOrIce(below);
                        if (showWater && (coverLiquid || belowLiquid)) {
                            surface_block_id = coverLiquid ? cover : below;
                        } else {
                            surface_block_id = BLOCK_AIR;
                            for (int y = topY; y >= 0; --y) {
                                const int b = chunk.GetBlockType(Int3{px, y, pz});
                                if (b == BLOCK_AIR || IsWaterOrIce(b))
                                    continue;
                                if (b == BLOCK_SNOW_LAYER && !snowMode)
                                    continue;
                                surface_block_id = b;
                                topY = y;
                                break;
                            }
                            if (surface_block_id == BLOCK_AIR)
                                surface_block_id = int(GetFillerBlock(chunk.GetBiome(px, pz)));
                        }
                        int outX = bx * CHUNK_WIDTH + px;
                        int outZ = bz * CHUNK_WIDTH + pz;
                        writePixel(outX, outZ, topY, surface_block_id, chunk.GetBiome(px, pz),
                                   chunk.temperature[px * CHUNK_WIDTH + pz],
                                   chunk.humidity[px * CHUNK_WIDTH + pz]);
                    }
                }
            }
        }
        if (hillshade)
            ApplyTopoHillshade();
        return buffer;
    }
}

int main() {
	MathHelper::InitSinTable();
    UpdateGenAndSeed(
        std::to_string(currentSeed).c_str(),
        GEN_BETA_BETA173
    );
    GenerateBiomeLookup();
    return 0;
}