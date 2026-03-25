#include <stdint.h>
#include <math.h>
#include <iostream>
#include <emscripten.h>
#include "./generators/beta/b173/generatorBeta173.h"
#include "biomeColors.h"

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

GeneratorBeta173 generator(3257840388504953787, nullptr);
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    uint8_t* getTile(int x, int y, int z) {
        std::cout << "getTile called with x=" << x << ", y=" << y << ", z=" << z << std::endl;

        Chunk* chunk = new Chunk(generator.GenerateChunk(Int2{x, y}));

        if (chunk->state == ChunkState::Generated) {
            std::cout << "Chunk generated successfully for x=" << x << ", y=" << y << std::endl;
        } else {
            std::cout << "Chunk generation failed for x=" << x << ", y=" << y << std::endl;
        }

        for (int px = 0; px < CHUNK_WIDTH_X; px++) {
            for (int pz = 0; pz < CHUNK_WIDTH_Z; pz++) {
                //int topY = CHUNK_HEIGHT - 1;
                // find top non-air block in this column (example)
                //while (topY >= 0 && chunk->GetBlockType(Int3{px, topY, pz}) == 0) {
                //    topY--;
                //}
                int idx = (pz * CHUNK_WIDTH_X + px) * 4;
                int height = chunk->GetHeightValue(px, pz);
                Int3 biomeColor = GetBiomeColor(chunk->GetBiome(px, pz));
                if (height <= WATER_LEVEL) {
                    buffer[idx + 0] = 0;
                    buffer[idx + 1] = 0;
                    buffer[idx + 2] = (height) * 3;
                } else {
                    buffer[idx + 0] = int((float((height-WATER_LEVEL) * 3)/255.0f * float(biomeColor.x)/255.0f) * 255.0f);
                    buffer[idx + 1] = int(((float((height-WATER_LEVEL) * 3)/255.0f * float(biomeColor.y)/255.0f) * 255.0f));
                    buffer[idx + 2] = int(((float((height-WATER_LEVEL) * 3)/255.0f * float(biomeColor.z)/255.0f) * 255.0f));
                }
                buffer[idx + 3] = 255;
            }
        }
        delete chunk;
        return buffer;
    }
}