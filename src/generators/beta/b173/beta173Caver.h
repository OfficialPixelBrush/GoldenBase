#pragma once

#include "javaRandom.h"
#include "datatypes.h"
#include "chunk.h"
#include <cstdint>
#include <memory>

/**
 * @brief Used to carve caves into the world
 * 
 */
class Beta173Caver {
  private:
	int32_t carveExtentLimit = 8;
	JavaRandom rand = JavaRandom();

  public:
	Beta173Caver();
	void CarveCavesForChunk(int64_t seed, Int2 chunkPos, Chunk &c);
	void CarveCaves(Int2 chunkOffset, Int2 chunkPos, Chunk &c);
	void CarveCave(Int2 chunkPos, Chunk &c, Vec3 offset);
	void CarveCave(Int2 chunkPos, Chunk &c, Vec3 offset,
				   float tunnelRadius, float carveYaw, float carvePitch, int32_t tunnelStep, int32_t tunnelLength,
				   double verticalScale);
};