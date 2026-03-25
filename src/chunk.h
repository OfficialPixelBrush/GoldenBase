#pragma once
#include "blocks.h"
#include "blockHelper.h"
#include "datatypes.h"
#include "helper.h"
#include "world.h"
#include <cstdint>

class World;

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
	int8_t heightMap[256];
	uint8_t lowestBlockHeight = CHUNK_HEIGHT - 1;
	World *world;
	int32_t xPos, zPos;

	void RelightBlock(int32_t x, int32_t y, int32_t z);
	void UpdateSkylight_do(int32_t x, int32_t z);
	void CheckSkylightNeighborHeight(int32_t x, int32_t z, int32_t height);
	// Block* GetBlock(Int3 pos);
	// Block* GetBlock(int32_t x, int8_t y, int32_t z);
	BlockType blockTypeArray[(CHUNK_WIDTH_X * CHUNK_WIDTH_Z * CHUNK_HEIGHT)];
	uint8_t blockMetaArray[(CHUNK_WIDTH_X * CHUNK_WIDTH_Z * CHUNK_HEIGHT) / 2];
	uint8_t blockLightArray[(CHUNK_WIDTH_X * CHUNK_WIDTH_Z * CHUNK_HEIGHT)];

  public:
	int8_t state = ChunkState::Invalid;
	bool modified = false;

	Chunk(Int2 pos) : xPos(pos.x), zPos(pos.y) {}
	int8_t GetHeightValue(uint8_t x, uint8_t z);
	void GenerateHeightMap();
	void PrintHeightmap();
	bool CanBlockSeeTheSky(Int3 pos);
	void SetLight(bool skyLight, Int3 pos, int8_t newLight);
	int8_t GetLight(bool skyLight, Int3 pos);
	int8_t GetTotalLight(Int3 pos);
	void ClearChunk();

	void SetBlockType(BlockType type, Int3 pos);
	BlockType GetBlockType(Int3 pos);
	void SetBlockMeta(int8_t meta, Int3 pos);
	int8_t GetBlockMeta(Int3 pos);
	void SetBlockTypeAndMeta(BlockType type, int8_t meta, Int3 pos);

	void SetBlockLight(int8_t value, Int3 pos);
	int8_t GetBlockLight(Int3 pos);
	void SetSkyLight(int8_t value, Int3 pos);
	int8_t GetSkyLight(Int3 pos);

	bool InChunkBounds(Int3 &pos);
    
	std::array<uint8_t, CHUNK_WIDTH_X * CHUNK_HEIGHT * CHUNK_WIDTH_Z> GetBlockTypes();
	std::array<uint8_t, CHUNK_WIDTH_X *(CHUNK_HEIGHT / 2) * CHUNK_WIDTH_Z> GetBlockMetas();
	std::array<uint8_t, CHUNK_WIDTH_X *(CHUNK_HEIGHT / 2) * CHUNK_WIDTH_Z> GetBlockLights();
	std::array<uint8_t, CHUNK_WIDTH_X *(CHUNK_HEIGHT / 2) * CHUNK_WIDTH_Z> GetSkyLights();
};