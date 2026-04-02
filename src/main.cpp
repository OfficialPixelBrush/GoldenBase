#include <stdint.h>
#include <math.h>
#include <iostream>
#include <emscripten.h>
#include <string>
#include "./generators/beta/b173/generatorBeta173.h"
#include "./generators/alpha/a112_01/generatorAlpha112_01.h"
#include "./generators/infdev/inf20100227/generatorInfdev20100227.h"
#include "./generators/infdev/inf20100327/generatorInfdev20100327.h"
#include "biomeColors.h"
#include "blocks.h"

#define MAX_BATCH_SIZE 4  // supports zoomLevel 0..3 (2^3 = 8 chunks)

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
        case BLOCK_GRASS:           return biomeColor;
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
    GEN_INVALID = 0,
    GEN_BETA_BETA173 = 3,
    GEN_ALPHA_ALPHA112_01 = 4,
    GEN_INFDEV_INFDEV20100327 = 2,
    GEN_INFDEV_INFDEV20100227 = 1,
};

extern "C" {
    int64_t currentSeed = 3257840388504953787;
    genSelect activeGenId = GEN_BETA_BETA173;
    Generator* generatorPtr = nullptr;

    EMSCRIPTEN_KEEPALIVE
    void UpdateGenAndSeed(const char* seed_cstr, genSelect genId = GEN_BETA_BETA173) {
        std::string seedString = std::string(seed_cstr);
        currentSeed = 0;
        
        bool isNumber = !seedString.empty() && 
            (seedString[0] == '-' || seedString[0] == '+' || std::isdigit(seedString[0])) &&
            std::all_of(seedString.begin() + 1, seedString.end(), ::isdigit);

        if (isNumber) {
            currentSeed = std::strtoll(seedString.c_str(), nullptr, 10);
        } else {
            //std::cout << "Non-numeric seed!" << std::endl;
            currentSeed = int64_t(hashCode(seedString));
        }

        //std::cout << seed_cstr << " -> " << currentSeed << std::endl;
        
        activeGenId = genId;
        if (generatorPtr) {
            delete generatorPtr;
            generatorPtr = nullptr;
        }
        switch(activeGenId) {
            default:
            case GEN_BETA_BETA173:
                generatorPtr = new GeneratorBeta173(currentSeed);
                break;
            case GEN_INFDEV_INFDEV20100227:
                generatorPtr = new GeneratorInfdev20100227(currentSeed);
                break;
            case GEN_INFDEV_INFDEV20100327:
                generatorPtr = new GeneratorInfdev20100327(currentSeed);
                break;
            case GEN_ALPHA_ALPHA112_01:
                generatorPtr = new GeneratorAlpha112_01(currentSeed);
                break;
        }
    }
    
    /*
        OPTIONS BITMASK
        1   -> Heightmap 
        2   -> Block Colors
        4   -> x
        8   -> x
        16  -> x
        32  -> x
        64  -> x
        128 -> x
        256 -> x
        ...
    */

    EMSCRIPTEN_KEEPALIVE
    uint8_t* getTile(int x, int z, int zoomLevel, int32_t options) {
        //std::cout << x << ", " << z << ": " << zoomLevel << std::endl;
        bool heightmap      = (options & 1) > 0;
        bool blockColors    = (options & 2) > 0;
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
                //Chunk copy = chunk;

                for (int px = 0; px < CHUNK_WIDTH_X; px++) {
                    for (int pz = 0; pz < CHUNK_WIDTH_Z; pz++) {
                        float fr,fg,fb;
                        uint8_t r, g, b;
                        float shadedHeight = 1.0f;
                        int topY = chunk.GetHeightValue(px, pz);
                        int surface_block_id = chunk.GetBlockType(Int3{px, topY, pz});
                        if (surface_block_id == BLOCK_WATER_STILL) {
                            fr = 0.0f; fg = 0.0f; fb = 1.0f;
                        } else if (surface_block_id == BLOCK_ICE) {
                            fr = 0.5f; fg = 0.8f; fb = 1.0f;
                        } else {
                            Int3 biomeColor = GetBiomeColor(chunk.GetBiome(pz, px));
                            if (blockColors) {
                                surface_block_id = chunk.GetBlockType(Int3{px, topY-1, pz});
                                Int3 blockColor = GetBlockColor(surface_block_id, biomeColor);
                                fr = Int8ToFloat(blockColor.x);
                                fg = Int8ToFloat(blockColor.y);
                                fb = Int8ToFloat(blockColor.z);
                            } else {
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
    UpdateGenAndSeed(
        std::to_string(currentSeed).c_str(),
        GEN_BETA_BETA173
    );
    GenerateBiomeLookup();
    return 0;
}