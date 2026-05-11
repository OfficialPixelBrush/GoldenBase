#include "chunk.h"
#include "helpers/datatypes.h"
#include <iostream>

Biome Chunk::GetBiome(int32_t x, int32_t z) {
	return biomeArray[(x & 15) << 4 | (z & 15)];
}

Biome Chunk::SetBiome(Biome biome, int32_t x, int32_t z) {
	biomeArray[(x & 15) << 4 | (z & 15)] = biome;
	return biome;
}

void Chunk::SetBiomes(std::vector<Biome> biomes) {
	for (size_t i = 0; i < biomes.size(); ++i) {
		biomeArray[i] = biomes[i];
	}
}

int8_t Chunk::GetHeightValue(uint8_t x, uint8_t z) { return this->heightMap[(z & 15) << 4 | (x & 15)]; }

void Chunk::GenerateHeightMap() {
    for (int8_t x = 0; x < CHUNK_WIDTH; ++x) {
        for (int8_t z = 0; z < CHUNK_WIDTH; ++z) {
			// Note: While doing the water fast-path is more efficient
			// It does result in us skipping any and all overhangs,
			// reducing accuracy on some seeds AND the farlands
			for (int8_t y = CHUNK_HEIGHT - 1; y >= 0; --y) {
				if (IsOpaque(blockTypeArray[PositionToBlockIndex(Int3{x,y-1,z})])) {
					heightMap[(z << 4) | x] = (int8_t)y;
					break;
				}
			}
			/*
			// Water-hit fast-path
			bool in_water = (blockTypeArray[PositionToBlockIndex(Int3{x,WATER_LEVEL-1,z})] == BLOCK_WATER_STILL);
			if (in_water) {
				for (int8_t y = WATER_LEVEL-1; y >= 0; --y) {
					if (IsOpaque(blockTypeArray[PositionToBlockIndex(Int3{x,y-1,z})])) {
						heightMap[(z << 4) | x] = (int8_t)y;
						break;
					}
				}
			} else {
				// Fallback to normal heightmap gen
				for (int8_t y = CHUNK_HEIGHT - 1; y >= 0; --y) {
					if (IsOpaque(blockTypeArray[PositionToBlockIndex(Int3{x,y-1,z})])) {
						heightMap[(z << 4) | x] = (int8_t)y;
						break;
					}
				}
			}
			*/
        }
    }
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