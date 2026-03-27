#include "biomeColors.h"

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