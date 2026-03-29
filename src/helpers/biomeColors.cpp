#include "biomeColors.h"

/*
Int3 GetBiomeColor(Biome biome) {
    switch (biome) {
        // Fallback for Alpha-grass, as biomes didn't exist or have biome colors in Alpha
        case BIOME_NONE:
            return Int3{200, 255, 128};
        case BIOME_RAINFOREST: // 1FF458
            return Int3{0x1F, 0xF4, 0x58};
        case BIOME_SWAMPLAND: // 8BAF48
            return Int3{0x8B, 0xAF, 0x48};
        case BIOME_SEASONALFOREST: // 4EE031
            return Int3{0x4E, 0xE0, 0x31};
        case BIOME_FOREST: // 4EBA31
        case BIOME_SAVANNA: // 4EE031
        case BIOME_SHRUBLAND: // 4EE031
        case BIOME_DESERT: // 4EE031
        case BIOME_PLAINS: // 4EE031
        case BIOME_HELL: // 4EE031
        case BIOME_SKY: // 4EE031
            return Int3{0x4E, 0xE0, 0x31};
        case BIOME_TAIGA: // 7BB731
            return Int3{0x7B, 0xB7, 0x31};
        case BIOME_ICEDESERT: // C4D339
            return Int3{0xC4, 0xD3, 0x39};
        case BIOME_TUNDRA: // C4D339
            return Int3{0xC4, 0xD3, 0x39};
        default:
            return Int3{0xFF, 0xFF, 0xFF};
    }
    return Int3{0xFF, 0xFF, 0xFF};
}
*/

Int3 GetBiomeColor(Biome biome) {
    switch (biome) {
        // Fallback for pre-biome gen, as biomes didn't exist or have biome colors in Alpha
        case BIOME_NONE:
        case BIOME_PLAINS:
            return Int3{0x8d, 0xb3, 0x60};
        case BIOME_RAINFOREST:
            return Int3{0x53, 0x7b, 0x09};
        case BIOME_SWAMPLAND:
            return Int3{0x07, 0xf9, 0xb2};
        case BIOME_SEASONALFOREST: 
            return Int3{0x2d, 0x8e, 0x49};
        case BIOME_FOREST:
            return Int3{0x05, 0x66, 0x21};
        case BIOME_SAVANNA:
            return Int3{0xbd, 0xb2, 0x5f};
        case BIOME_SHRUBLAND:
            return Int3{0xb5, 0xdb, 0x88};
        case BIOME_DESERT:
            return Int3{0xfa, 0x94, 0x18};
        case BIOME_HELL:
            return Int3{0xff, 0x00, 0x00};
        case BIOME_SKY: 
            return Int3{0x4E, 0xE0, 0x31};
        case BIOME_TAIGA: 
            return Int3{0x0b, 0x66, 0x59};
        case BIOME_ICEDESERT: 
            return Int3{0xC4, 0xD3, 0x39};
        case BIOME_TUNDRA:
            return Int3{0xFF, 0xFF, 0xFF};
        default:
            return Int3{0xFF, 0x00, 0xFF};
    }
    return Int3{0xFF, 0x00, 0xFF};
}