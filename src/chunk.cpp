#include "chunk.h"
#include "helpers/datatypes.h"
#include <cstdint>

Biome Chunk::GetBiome(int32_t x, int32_t z) {
	return biomeArray[(x & 15) << 4 | (z & 15)];
}

Biome Chunk::SetBiome(Biome biome, int32_t x, int32_t z) {
	biomeArray[(x & 15) << 4 | (z & 15)] = biome;
	return biome;
}

void Chunk::SetBiomes(std::vector<Biome> biomes) {
	std::copy(biomes.begin(), biomes.end(), biomeArray);
}

int8_t Chunk::GetHeightValue(uint8_t x, uint8_t z) { return this->heightMap[(z & 15) << 4 | (x & 15)]; }
void Chunk::SetHeightValue(uint8_t x, uint8_t z, int8_t h) { this->heightMap[(z & 15) << 4 | (x & 15)] = this->heightMap[(z & 15) << 4 | (x & 15)] == 0 ? this->heightMap[(z & 15) << 4 | (x & 15)] : h; }

int32_t Chunk::GetHighestSolidOrLiquidBlock(Int2 pos) {
	for (int32_t y = CHUNK_HEIGHT - 1; y > 0; --y) {
		BlockType blockType = this->GetBlockType(Int3{pos.x, y, pos.y});
		if (blockType == BLOCK_AIR)
			continue;
		if (IsSolid(blockType) || IsLiquid(blockType)) {
			return y + 1;
		}
	}
	return -1;
}

void Chunk::GenerateHeightMap() {
    for (int8_t x = 0; x < CHUNK_WIDTH; ++x) {
        for (int8_t z = 0; z < CHUNK_WIDTH; ++z) {
			// Note: While doing the water fast-path is more efficient
			// It does result in us skipping any and all overhangs,
			// reducing accuracy on some seeds AND the farlands
			/*
			for (int8_t y = CHUNK_HEIGHT - 1; y >= 0; --y) {
				if (IsOpaque(blockTypeArray[PositionToBlockIndex(Int3{x,y-1,z})])) {
					heightMap[(z << 4) | x] = (int8_t)y;
					break;
				}
			}
			// Water-hit fast-path
			bool in_water = (blockTypeArray[PositionToBlockIndex(Int3{x,WATER_LEVEL-1,z})] == BLOCK_WATER_STILL);
			if (in_water) {
				for (int8_t y = WATER_LEVEL-1; y >= 0; --y) {
					if (IsOpaque(blockTypeArray[PositionToBlockIndex(Int3{x,y-1,z})])) {
						heightMap[(z << 4) | x] = (int8_t)y;
						break;
					}
				}
			} else {*/
				// Fallback to normal heightmap gen
				for (int8_t y = CHUNK_HEIGHT - 1; y >= 0; --y) {
					BlockType type = blockTypeArray[PositionToBlockIndex(Int3{x,y-1,z})];
					if (IsOpaque(type) || type == BLOCK_SNOW_LAYER) {
						heightMap[(z << 4) | x] = (int8_t)y;
						break;
					}
				}
			//}
        }
    }
}

#include "helpers/grassColorBuffer.h"
Int3 Chunk::GetGrassColor(int32_t x, int32_t z) {
	float humi = humidity[x + CHUNK_WIDTH * z];
	float temp = temperature[x + CHUNK_WIDTH * z];
	humi *= temp;
	int ti = int((1.0f - temp) * 255.0f);
	int hi = int((1.0f - humi) * 255.0f);
	int lutRes = grassColorLut[hi << 8 | ti];
	return Int3{
		(lutRes >> 16) & 0xFF,
		(lutRes >>  8) & 0xFF,
		(lutRes      ) & 0xFF,
	};
}

void Chunk::ClearChunk() {
	std::fill(std::begin(blockTypeArray), std::end(blockTypeArray), BLOCK_AIR);
}

BlockType Chunk::GetBlockType(Int3 pos) {
	if (!InChunkBounds(pos))
		return BLOCK_AIR;
	return blockTypeArray[PositionToBlockIndex(pos)];
}

void Chunk::SetBlockType(BlockType type, Int3 pos) {
	if (!InChunkBounds(pos))
		return;
	blockTypeArray[PositionToBlockIndex(pos)] = type;
}


void Chunk::SetBlockTypeAndMeta(BlockType type, int8_t meta, Int3 pos) {
	SetBlockType(type, pos);
	//SetBlockMeta(meta, pos);
}

bool Chunk::InChunkBounds(Int3 &pos) {
	pos.x &= CHUNK_WIDTH - 1;
	pos.z &= CHUNK_WIDTH - 1;
	if (pos.y < 0 || pos.y >= CHUNK_HEIGHT)
		return false;
	return true;
}