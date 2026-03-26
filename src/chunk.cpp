#include "chunk.h"
#include <iostream>

Biome Chunk::GetBiome(int32_t x, int32_t z) {
	return biomeArray[(z & 15) << 4 | (x & 15)];
}

Biome Chunk::SetBiome(Biome biome, int32_t x, int32_t z) {
	biomeArray[(z & 15) << 4 | (x & 15)] = biome;
	return biome;
}

void Chunk::SetBiomes(std::vector<Biome> biomes) {
	for (size_t i = 0; i < biomes.size(); ++i) {
		biomeArray[i] = biomes[i];
	}
}

int8_t Chunk::GetHeightValue(uint8_t x, uint8_t z) { return this->heightMap[(z & 15) << 4 | (x & 15)]; }

void Chunk::GenerateHeightMap() {
	int32_t lowestBlock = CHUNK_HEIGHT - 1;
	int32_t x, z;
	for (x = 0; x < CHUNK_WIDTH_X; ++x) {
		for (z = 0; z < CHUNK_WIDTH_Z; ++z) {
			int32_t y = CHUNK_HEIGHT - 1;

			for (y = CHUNK_HEIGHT - 1; y > 0; --y) {
				if (IsOpaque(int16_t(GetBlockType(Int3{x, y-1, z}))))
					break;
			}

			this->heightMap[z << 4 | x] = int8_t(y);
			if (y < lowestBlock) {
				lowestBlock = y;
			}
		}
	}
	this->lowestBlockHeight = lowestBlock;
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
	pos.x &= CHUNK_WIDTH_X - 1;
	pos.z &= CHUNK_WIDTH_Z - 1;
	if (pos.y < 0 || pos.y >= CHUNK_HEIGHT)
		return false;
	return true;
}