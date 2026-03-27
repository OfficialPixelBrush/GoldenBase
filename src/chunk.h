#pragma once
#include "blocks.h"
#include "blockHelper.h"
#include "datatypes.h"
#include "generators/beta/b173/beta173Biome.h"
#include "helper.h"
#include <cstdint>

enum ChunkState : int8_t {
	Invalid,
	Generating,
	Generated,
	Populating,
	Populated
};

/**
 * @brief Responsible for reading, writing and holding onto its block data
 * 
 */
class Chunk {
  private:
	int8_t heightMap[CHUNK_WIDTH_X * CHUNK_WIDTH_Z];
	uint8_t lowestBlockHeight = CHUNK_HEIGHT - 1;
	int32_t xPos, zPos;
	Biome biomeArray[CHUNK_WIDTH_X * CHUNK_WIDTH_Z] = {BIOME_NONE};
	BlockType blockTypeArray[(CHUNK_WIDTH_X * CHUNK_WIDTH_Z * CHUNK_HEIGHT)];

  public:
	int8_t state = ChunkState::Invalid;

	Chunk(Int2 pos) : xPos(pos.x), zPos(pos.y) {}
	int8_t GetHeightValue(uint8_t x, uint8_t z);
	void GenerateHeightMap();
	void ClearChunk();

	Biome GetBiome(int32_t x, int32_t z);
	Biome SetBiome(Biome biome, int32_t x, int32_t z);
	void SetBiomes(std::vector<Biome> biomes);

	void SetBlockType(BlockType type, Int3 pos);
	BlockType GetBlockType(Int3 pos);
	void SetBlockTypeAndMeta(BlockType type, int8_t meta, Int3 pos);

	bool InChunkBounds(Int3 &pos);
};