#include "mapColors.h"
#include "blocks.h"

// MapColor.colorValue from beta 1.7.3 MapColor.java
static const int32_t kMapGrass = 8368696;
static const int32_t kMapSand = 16247203;
static const int32_t kMapCloth = 10987431;
static const int32_t kMapTnt = 16711680;
static const int32_t kMapIce = 10526975;
static const int32_t kMapIron = 10987431;
static const int32_t kMapFoliage = 31744;
static const int32_t kMapSnow = 16777215;
static const int32_t kMapClay = 10791096;
static const int32_t kMapDirt = 12020271;
static const int32_t kMapStone = 7368816;
static const int32_t kMapWater = 4210943;
static const int32_t kMapWood = 6837042;

static Int3 Hex(int32_t value) {
	return Int3{(value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF};
}

bool IsMapColorAir(int block_id) {
	switch (block_id) {
	case BLOCK_AIR:
	case BLOCK_GLASS:
	case BLOCK_FIRE:
	case BLOCK_TORCH:
	case BLOCK_REDSTONE:
	case BLOCK_REDSTONE_TORCH_OFF:
	case BLOCK_REDSTONE_TORCH_ON:
	case BLOCK_REDSTONE_REPEATER_OFF:
	case BLOCK_REDSTONE_REPEATER_ON:
	case BLOCK_RAIL:
	case BLOCK_RAIL_POWERED:
	case BLOCK_RAIL_DETECTOR:
	case BLOCK_LADDER:
	case BLOCK_LEVER:
	case BLOCK_BUTTON_STONE:
	case BLOCK_PRESSURE_PLATE_STONE:
	case BLOCK_PRESSURE_PLATE_WOOD:
	case BLOCK_NETHER_PORTAL:
	case BLOCK_CAKE:
	case BLOCK_SIGN:
	case BLOCK_SIGN_WALL:
		return true;
	default:
		return false;
	}
}

bool IsMapWaterColor(int block_id) {
	return block_id == BLOCK_WATER_STILL || block_id == BLOCK_WATER_FLOWING;
}

Int3 GetBlockMapColor(int block_id) {
	switch (block_id) {
	case BLOCK_GRASS:
		return Hex(kMapGrass);
	case BLOCK_SAND:
	case BLOCK_GRAVEL:
	case BLOCK_SOULSAND:
		return Hex(kMapSand);
	case BLOCK_WOOL:
	case BLOCK_SPONGE:
	case BLOCK_BED:
	case BLOCK_COBWEB:
		return Hex(kMapCloth);
	case BLOCK_LAVA_FLOWING:
	case BLOCK_LAVA_STILL:
	case BLOCK_TNT:
		return Hex(kMapTnt);
	case BLOCK_ICE:
		return Hex(kMapIce);
	case BLOCK_GOLD:
	case BLOCK_IRON:
	case BLOCK_DIAMOND:
	case BLOCK_DOOR_IRON:
		return Hex(kMapIron);
	case BLOCK_LEAVES:
	case BLOCK_SAPLING:
	case BLOCK_TALLGRASS:
	case BLOCK_DEADBUSH:
	case BLOCK_DANDELION:
	case BLOCK_ROSE:
	case BLOCK_MUSHROOM_BROWN:
	case BLOCK_MUSHROOM_RED:
	case BLOCK_CACTUS:
	case BLOCK_SUGARCANE:
	case BLOCK_CROP_WHEAT:
	case BLOCK_PUMPKIN:
	case BLOCK_PUMPKIN_LIT:
		return Hex(kMapFoliage);
	case BLOCK_SNOW:
	case BLOCK_SNOW_LAYER:
		return Hex(kMapSnow);
	case BLOCK_CLAY:
		return Hex(kMapClay);
	case BLOCK_DIRT:
	case BLOCK_FARMLAND:
		return Hex(kMapDirt);
	case BLOCK_WATER_FLOWING:
	case BLOCK_WATER_STILL:
		return Hex(kMapWater);
	case BLOCK_LOG:
	case BLOCK_PLANKS:
	case BLOCK_CHEST:
	case BLOCK_CHEST_LOCKED:
	case BLOCK_CRAFTING_TABLE:
	case BLOCK_FENCE:
	case BLOCK_DOOR_WOOD:
	case BLOCK_TRAPDOOR:
	case BLOCK_JUKEBOX:
	case BLOCK_NOTEBLOCK:
	case BLOCK_BOOKSHELF:
	case BLOCK_STAIRS_WOOD:
		return Hex(kMapWood);
	default:
		if (IsMapColorAir(block_id))
			return Hex(0);
		return Hex(kMapStone);
	}
}
