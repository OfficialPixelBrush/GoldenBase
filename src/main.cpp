#include <stdint.h>
#include <math.h>
#include <iostream>
#include <emscripten.h>
#include "./generators/beta/b173/generatorBeta173.h"
#include "./generators/infdev/inf20100227/generatorInfdev20100227.h"
#include "./generators/infdev/inf20100327/generatorInfdev20100327.h"
#include "biomeColors.h"
#include "blocks.h"

#define MAX_BATCH_SIZE 8  // supports zoomLevel 0..3 (2^3 = 8 chunks)

static uint8_t buffer[(CHUNK_WIDTH_X * MAX_BATCH_SIZE) * (CHUNK_WIDTH_Z * MAX_BATCH_SIZE) * 4];

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

Int3 GetBlockColor(int block_id, Int3 biomeColor) {
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
        case BLOCK_GRASS:           return MultiplyColor(HexToInt3(0x959595), biomeColor);
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
    }
}

enum genSelect {
    GEN_INVALID,
    GEN_INFDEV_INFDEV20100227,
    GEN_INFDEV_INFDEV20100327,
    GEN_BETA_BETA173,
};

extern "C" {
    int64_t currentSeed = 3257840388504953787;
    genSelect activeGeneratorId = GEN_BETA_BETA173;
    Generator* generatorPtr = nullptr;

    EMSCRIPTEN_KEEPALIVE
    void UpdateGenerator(genSelect generatorId = GEN_BETA_BETA173) {
        activeGeneratorId = generatorId;
        if (generatorPtr) {
            delete generatorPtr;
            generatorPtr = nullptr;
        }
        switch(generatorId) {
            case GEN_BETA_BETA173:
                generatorPtr = new GeneratorBeta173(currentSeed);
                break;
            case GEN_INFDEV_INFDEV20100227:
                generatorPtr = new GeneratorInfdev20100227(currentSeed);
                break;
            case GEN_INFDEV_INFDEV20100327:
                generatorPtr = new GeneratorInfdev20100327(currentSeed);
                break;
            default:
                generatorPtr = new GeneratorBeta173(currentSeed);
        }
    }

    EMSCRIPTEN_KEEPALIVE
    void UpdateSeed(int64_t seed) {
        std::cout << seed << std::endl;
        currentSeed = seed;
        UpdateGenerator(activeGeneratorId);
    }
    
    EMSCRIPTEN_KEEPALIVE
    uint8_t* getTile(int x, int z, int zoomLevel) {
        // zoomLevel < 0 : zoomed out — more chunks, 1px per block
        // zoomLevel = 0 : MAX_BATCH_SIZE chunks, 1px per block  (base)
        // zoomLevel > 0 : fewer chunks, scale px per block
        //
        // At base (zoomLevel=0): batchSize=MAX_BATCH_SIZE, scale=1
        // Each step up halves the batch and doubles the scale:
        //   batchSize = MAX_BATCH_SIZE >> zoomLevel  (min 1)
        //   scale     = 1 << zoomLevel               (max MAX_BATCH_SIZE)
        // Buffer is always exactly MAX_BATCH_SIZE*CHUNK_WIDTH pixels wide.

        int batchSize = MAX_BATCH_SIZE >> zoomLevel;
        int scale     = 1 << zoomLevel;

        // Clamp to valid range
        if (batchSize < 1)          { batchSize = 1; scale = MAX_BATCH_SIZE; }
        if (scale > MAX_BATCH_SIZE) { scale = MAX_BATCH_SIZE; batchSize = 1; }

        int tileWidth = MAX_BATCH_SIZE * CHUNK_WIDTH_X;   // always constant
        // (tileHeight = MAX_BATCH_SIZE * CHUNK_WIDTH_Z)

        for (int bx = 0; bx < batchSize; bx++) {
            for (int bz = 0; bz < batchSize; bz++) {
                Chunk chunk = generatorPtr->GenerateChunk(Int2{
                    x * batchSize + bx,
                    z * batchSize + bz
                });

                for (int px = 0; px < CHUNK_WIDTH_X; px++) {
                    for (int pz = 0; pz < CHUNK_WIDTH_Z; pz++) {
                        int topY = chunk.GetHeightValue(px, pz);
                        int surface_block_id = chunk.GetBlockType(Int3{px, topY - 1, pz});
                        Int3 biomeColor = GetBiomeColor(chunk.GetBiome(pz, px));
                        Int3 blockColor = GetBlockColor(surface_block_id, biomeColor);
                        float heightFloat = HeightToFloat(topY);
                        float gamma = 0.5f;
                        float shadedHeight = powf(heightFloat, gamma);

                        uint8_t r, g, b;
                        if (topY <= WATER_LEVEL) {
                            r = 0; g = 0; b = FloatToInt8(shadedHeight);
                        } else {
                            r = FloatToInt8(shadedHeight * Int8ToFloat(blockColor.x));
                            g = FloatToInt8(shadedHeight * Int8ToFloat(blockColor.y));
                            b = FloatToInt8(shadedHeight * Int8ToFloat(blockColor.z));
                        }

                        // Top-left pixel of this block in the output tile
                        int originX = (bx * CHUNK_WIDTH_X + px) * scale;
                        int originZ = (bz * CHUNK_WIDTH_Z + pz) * scale;

                        for (int sy = 0; sy < scale; sy++) {
                            for (int sx = 0; sx < scale; sx++) {
                                int idx = ((originZ + sy) * tileWidth + (originX + sx)) * 4;
                                buffer[idx + 0] = r;
                                buffer[idx + 1] = g;
                                buffer[idx + 2] = b;
                                buffer[idx + 3] = 255;
                            }
                        }
                    }
                }
            }
        }
        return buffer;
    }
}

int main() {
    UpdateGenerator(activeGeneratorId);
    GenerateBiomeLookup();
    return 0;
}