#include <stdint.h>
#include <math.h>
#include <iostream>
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

#define MAX_BATCH_SIZE 4  // supports zoomLevel 0..3 (2^3 = 8 chunks per tile side)
// Negative zoomLevel zooms out: each step doubles the number of chunks per tile side.
// MAX_ZOOM_OUT controls how far out we allow (e.g. 3 → 32 chunks per side at level -3).
#define MAX_ZOOM_OUT 4
// Low-detail multiplier used when zoomed out beyond 1:1 pixel scale (zoomLevel < 0).
#define LOW_DETAIL_MULTIPLIER 0.01f

static uint8_t buffer[(CHUNK_WIDTH * MAX_BATCH_SIZE) * (CHUNK_WIDTH * MAX_BATCH_SIZE) * 4];
// The output tile is always MAX_BATCH_SIZE*CHUNK_WIDTH pixels square, regardless of zoom level.

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
    // Low-detail generator used when zoomLevel < 0 (zoomed out beyond 1:1).
    // Created with LOW_DETAIL_MULTIPLIER to reduce noise octave count for speed.
    Generator* lowDetailGeneratorPtr = nullptr;

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
            currentSeed = int64_t(hashCode(seedString));
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

        // Replace normal-detail generator
        if (generatorPtr) { delete generatorPtr; generatorPtr = nullptr; }
        generatorPtr = makeGenerator(1.0);

        // Replace low-detail generator (used when zoomed out beyond 1:1)
        if (lowDetailGeneratorPtr) { delete lowDetailGeneratorPtr; lowDetailGeneratorPtr = nullptr; }
        lowDetailGeneratorPtr = makeGenerator(LOW_DETAIL_MULTIPLIER);
        lowDetailGeneratorPtr->lowDetail = true;
    }
    
    /*
        OPTIONS BITMASK
        1   -> Heightmap, if the color should be multiplied by the height (darker at low y, brighter at high y)
        2   -> Block Colors, if not set, biome colors are used for everything
        4   -> Show Water, if water should be rendered
        8   -> Snow Mode, snow should be rendered
        16  -> Snow World, if world is snow world
        32  -> Accurate Grass Colors, that match Beta Minecraft
        64  -> x
        128 -> x
        256 -> x
        ...
    */

    EMSCRIPTEN_KEEPALIVE
    uint8_t* getTile(int x, int z, int zoomLevel, int32_t options) {
        bool heightmap      = (options &  1) > 0;
        bool blockColors    = (options &  2) > 0;
        bool showWater      = (options &  4) > 0;
        bool snowMode       = (options &  8) > 0;
        bool snowWorld      = (options & 16) > 0;
        bool tempHumiColor  = (options & 32) > 0;

        // ── Zoom-level semantics ──────────────────────────────────────────────
        // zoomLevel > 0  : zoomed IN  — fewer chunks, each block drawn at scale×scale pixels
        //   batchSize = MAX_BATCH_SIZE >> zoomLevel  (min 1)
        //   scale     = 1 << zoomLevel
        //   stride    = 1  (every block is sampled)
        //
        // zoomLevel = 0  : base — MAX_BATCH_SIZE chunks per side, 1px per block
        //
        // zoomLevel < 0  : zoomed OUT — more chunks per tile, 1px per sampled block
        //   batchSize = MAX_BATCH_SIZE << (-zoomLevel)  (more chunks)
        //   scale     = 1
        //   stride    = 1 << (-zoomLevel)  (sample every stride-th block per chunk)
        //   Uses lowDetailGeneratorPtr for faster (lower-octave) generation.
        //
        // The output tile is always (MAX_BATCH_SIZE*CHUNK_WIDTH)² pixels.
        // ─────────────────────────────────────────────────────────────────────

        int tileWidth = MAX_BATCH_SIZE * CHUNK_WIDTH;   // constant: 64

        int batchSize, scale, stride;
        Generator* gen;

        if (zoomLevel >= 0) {
            // Zoomed in (or base)
            batchSize = MAX_BATCH_SIZE >> zoomLevel;
            scale     = 1 << zoomLevel;
            stride    = 1;
            if (batchSize < 1)          { batchSize = 1; scale = MAX_BATCH_SIZE; }
            if (scale > MAX_BATCH_SIZE) { scale = MAX_BATCH_SIZE; batchSize = 1; }
            gen = generatorPtr;
        } else {
            // Zoomed out — clamp to MAX_ZOOM_OUT
            int zoomOut = -zoomLevel;
            if (zoomOut > MAX_ZOOM_OUT) zoomOut = MAX_ZOOM_OUT;
            batchSize = MAX_BATCH_SIZE << zoomOut;   // more chunks to cover
            scale     = 1;
            stride    = 1 << zoomOut;                // sample 1 in every stride blocks
            if (zoomLevel < -1) {
                gen = lowDetailGeneratorPtr ? lowDetailGeneratorPtr : generatorPtr;
                blockColors = false;
                heightmap = false;
            } else {
                gen = generatorPtr;
            }
        }
        gen->snowMode = snowMode;
        // Only set snow world for specific generator
        if (auto* alphaGen = dynamic_cast<GeneratorAlpha112_01*>(gen))
            alphaGen->snowCovered = snowWorld;
        // Is beta generator, and thus can have darker grass
        bool darkerGrass = dynamic_cast<GeneratorBeta173*>(gen) != nullptr;

        for (int bx = 0; bx < batchSize; bx++) {
            for (int bz = 0; bz < batchSize; bz++) {
                Chunk chunk = gen->GenerateChunk(Int2{
                    x * batchSize + bx,
                    z * batchSize + bz
                });

                // Which output pixel column/row does this chunk contribute to?
                // At stride>1 we only write one pixel per stride blocks.
                for (int px = 0; px < CHUNK_WIDTH; px += stride) {
                    for (int pz = 0; pz < CHUNK_WIDTH; pz += stride) {
                        float fr,fg,fb;
                        uint8_t r, g, b;
                        float shadedHeight = 1.0f;
                        int topY = chunk.GetHeightValue(px, pz);
                        int surface_block_id = chunk.GetBlockType(Int3{px, topY, pz});
                        if (showWater && surface_block_id == BLOCK_WATER_STILL) {
                            fr = 0.0f; fg = 0.0f; fb = 1.0f;
                        } else if (showWater && surface_block_id == BLOCK_ICE) {
                            fr = 0.5f; fg = 0.8f; fb = 1.0f;
                        } else {
                            Int3 biomeColor;
                            bool renderDarkerGrass = tempHumiColor && darkerGrass;
                            if (renderDarkerGrass)
                                biomeColor = chunk.GetGrassColor(px,pz);
                            else
                                biomeColor = GetBiomeColor(chunk.GetBiome(px, pz));
                            if (blockColors) {
                                surface_block_id = chunk.GetBlockType(Int3{px, topY-1, pz});
                                Int3 blockColor = GetBlockColor(surface_block_id, biomeColor, renderDarkerGrass);
                                fr = Int8ToFloat(blockColor.x);
                                fg = Int8ToFloat(blockColor.y);
                                fb = Int8ToFloat(blockColor.z);
                            } else {
                                biomeColor = renderDarkerGrass ? MultiplyColor(biomeColor, HexToInt3(0x929292)) : biomeColor;
                                fr = Int8ToFloat(biomeColor.x);
                                fg = Int8ToFloat(biomeColor.y);
                                fb = Int8ToFloat(biomeColor.z);
                            }
                        }
                        if (heightmap) {
                            float heightFloat = (HeightToFloat(topY) * 1.5f);
                            float gamma = 0.9f;
                            shadedHeight = powf(heightFloat, gamma);
                            fr *= shadedHeight;
                            fg *= shadedHeight;
                            fb *= shadedHeight;
                        }
                        r = FloatToInt8(fr);
                        g = FloatToInt8(fg);
                        b = FloatToInt8(fb);

                        // Output pixel position: each (chunk, sampled-block) maps to one pixel.
                        // For zoom-out: pixel (outX, outZ) = (bx*(CHUNK_WIDTH/stride) + px/stride, ...)
                        // For zoom-in:  pixel origin (bx*CHUNK_WIDTH + px)*scale, drawn scale×scale.
                        if (scale > 1) {
                            int originX = (bx * CHUNK_WIDTH + px) * scale;
                            int originZ = (bz * CHUNK_WIDTH + pz) * scale;
                            for (int sy = 0; sy < scale; sy++) {
                                for (int sx = 0; sx < scale; sx++) {
                                    int idx = ((originZ + sy) * tileWidth + (originX + sx)) * 4;
                                    buffer[idx + 0] = r;
                                    buffer[idx + 1] = g;
                                    buffer[idx + 2] = b;
                                    buffer[idx + 3] = 255;
                                }
                            }
                        } else {
                            // stride >= 1: one output pixel per sampled block
                            int samplesPerChunk = CHUNK_WIDTH / stride;   // e.g. 16, 8, 4...
                            int outX = bx * samplesPerChunk + px / stride;
                            int outZ = bz * samplesPerChunk + pz / stride;
                            int idx = (outZ * tileWidth + outX) * 4;
                            buffer[idx + 0] = r;
                            buffer[idx + 1] = g;
                            buffer[idx + 2] = b;
                            buffer[idx + 3] = 255;
                        }
                    }
                }
            }
        }
        return buffer;
    }
}

int main() {
    UpdateGenAndSeed(
        std::to_string(currentSeed).c_str(),
        GEN_BETA_BETA173
    );
    GenerateBiomeLookup();
    return 0;
}