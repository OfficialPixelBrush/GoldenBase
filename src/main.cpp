#include <stdint.h>
#include <math.h>
#include <iostream>
#include <emscripten.h>
#include "./generators/beta/b173/generatorBeta173.h"
#include "./generators/infdev/inf20100227/generatorInfdev20100227.h"
#include "./generators/infdev/inf20100327/generatorInfdev20100327.h"
#include "biomeColors.h"
#include "blocks.h"

static uint8_t buffer[CHUNK_WIDTH_X * CHUNK_WIDTH_Z * 4]; // static = lives for the program's lifetime

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

Int3 GetBlockColor(int block_id, Int3 biomeColor) {
    switch(block_id) {
        default:
            return Int3{255, 0, 255}; // magenta for unknown blocks
        case BLOCK_STONE:
            return Int3{128, 128, 128};
        case BLOCK_GRASS:
            return MultiplyColor(Int3{200, 200, 200}, biomeColor); // Biome tint
        case BLOCK_DIRT:
            return Int3{155, 118, 83};
        case BLOCK_COBBLESTONE:
            return Int3{120, 120, 120};
        case BLOCK_PLANKS:
            return Int3{160, 82, 45};
        case BLOCK_SAPLING:
            return Int3{34, 177, 76}; // Biome tint
        case BLOCK_BEDROCK:
            return Int3{54, 54, 54};
        case BLOCK_WATER_FLOWING:
        case BLOCK_WATER_STILL:
            return Int3{64, 164, 223};
        case BLOCK_LAVA_FLOWING:
        case BLOCK_LAVA_STILL:
            return Int3{255, 69, 0};
        case BLOCK_SAND:
            return Int3{194, 178, 128};
        case BLOCK_GRAVEL:
            return Int3{136, 126, 126};
        case BLOCK_ORE_GOLD:
            return Int3{255, 215, 0};
        case BLOCK_ORE_IRON:
            return Int3{216, 216, 216};
        case BLOCK_ORE_COAL:
            return Int3{54, 54, 54};
        case BLOCK_LOG:
            return Int3{102, 51, 0};
        case BLOCK_LEAVES:
            return Int3{34, 177, 76}; // Biome tint
        case BLOCK_SPONGE:
            return Int3{255, 255, 102};
        case BLOCK_GLASS:
            return Int3{255, 255, 255};
        case BLOCK_ORE_LAPIS_LAZULI:
            return Int3{38, 97, 156};
        case BLOCK_DISPENSER:
            return Int3{128, 128, 128};
        case BLOCK_SANDSTONE:
            return Int3{194, 178, 128};
        case BLOCK_NOTEBLOCK:
            return Int3{160, 82, 45};
        case BLOCK_BED:
            return Int3{255, 0, 255}; // magenta for unknown blocks
        case BLOCK_RAIL_POWERED:
            return Int3{128, 128, 128};
        case BLOCK_RAIL_DETECTOR:
            return Int3{128, 128, 128};
        case BLOCK_PISTON_STICKY:
            return Int3{0, 255, 0};
        case BLOCK_BRICKS:
            return Int3{150, 50, 50};
        case BLOCK_TNT:
            return Int3{255, 0, 0};
        case BLOCK_OBSIDIAN:
            return Int3{50, 0, 100};
    }
}

//GeneratorBeta173 generator(3257840388504953787, nullptr);
//GeneratorBeta173 generator(3257840388504953787, nullptr);
extern "C" {
    int64_t currentSeed = 3257840388504953787;
    Generator* generatorPtr;

    EMSCRIPTEN_KEEPALIVE
    void UpdateGenerator(int generatorId = 0) {
        if (generatorPtr) {
            delete generatorPtr;
        }
        switch(generatorId) {
            case 0:
                generatorPtr = new GeneratorBeta173(currentSeed, nullptr);
                break;
            case 1:
                generatorPtr = new GeneratorInfdev20100227(currentSeed, nullptr);
                break;
            case 2:
                generatorPtr = new GeneratorInfdev20100327(currentSeed, nullptr);
                break;
            default:
                generatorPtr = new GeneratorBeta173(currentSeed, nullptr);
        }
    }

    EMSCRIPTEN_KEEPALIVE
    void UpdateSeed(int64_t seed) {
        if (!generatorPtr) {
            UpdateGenerator(0);
        }
        currentSeed = seed;
        generatorPtr->seed = seed;
    }

    EMSCRIPTEN_KEEPALIVE
    uint8_t* getTile(int x, int y, int z) {
        std::cout << "getTile called with x=" << x << ", y=" << y << ", z=" << z << std::endl;

        Chunk* chunk = new Chunk(generatorPtr->GenerateChunk(Int2{x, y}));

        if (chunk->state == ChunkState::Generated) {
            std::cout << "Chunk generated successfully for x=" << x << ", y=" << y << std::endl;
        } else {
            std::cout << "Chunk generation failed for x=" << x << ", y=" << y << std::endl;
        }

        for (int px = 0; px < CHUNK_WIDTH_X; px++) {
            for (int pz = 0; pz < CHUNK_WIDTH_Z; pz++) {
                int topY = chunk->GetHeightValue(px, pz);
                int surface_block_id = chunk->GetBlockType(Int3{px, topY-1, pz});
                Int3 biomeColor = GetBiomeColor(chunk->GetBiome(pz, px));
                Int3 blockColor = GetBlockColor(surface_block_id, biomeColor);
                int idx = (pz * CHUNK_WIDTH_X + px) * 4;
                float heightFloat = HeightToFloat(topY);
                if (topY <= WATER_LEVEL) {
                    buffer[idx + 0] = 0;
                    buffer[idx + 1] = 0;
                    buffer[idx + 2] = FloatToInt8(heightFloat);
                } else {
                    buffer[idx + 0] = (FloatToInt8(heightFloat * Int8ToFloat(blockColor.x)));
                    buffer[idx + 1] = (FloatToInt8(heightFloat * Int8ToFloat(blockColor.y)));
                    buffer[idx + 2] = (FloatToInt8(heightFloat * Int8ToFloat(blockColor.z)));
                }
                buffer[idx + 3] = 255;
            }
        }
        delete chunk;
        return buffer;
    }
}

int main() {
    UpdateGenerator(0);
    GenerateBiomeLookup();
    return 0;
}