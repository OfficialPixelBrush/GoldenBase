#pragma once
#include <memory>
#include <string>

#include <blocks.h>
#include <chunk.h>
#include <helper.h>
#include <java_random.h>
#include <noise_octaves_perlin.h>
#include <blockHelper.h>
#include <noiseLod.h>
#include <biomes.h>

#include <cstdlib>
#include <ctime>

class Chunk;

/**
 * @brief Generic Generator object that makes an empty world
 * 
 */
class Generator {
  public:
	Generator(int64_t seed, float multiplier = 1.0);
	virtual ~Generator();
	virtual Chunk GenerateChunk(Int2 chunkPos);
	virtual bool PopulateChunk(Int2 chunkPos);
	// Drop fine octaves according to blocks-per-pixel. stride==1 keeps every octave.
	virtual void SetDetailLevel(int32_t stride);
	// Fast height/biome preview for a regular sample grid. Default falls back to GenerateChunk.
	virtual void SampleColumns(int32_t originX, int32_t originZ, int32_t samples, int32_t stride, TileColumn *out);
	int64_t seed;
	float octave_multiplier = 1.0;
	bool lowDetail = false;
	bool snowMode = false;
};