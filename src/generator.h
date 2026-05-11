#pragma once
#include <memory>
#include <string>

#include <blocks.h>
#include <chunk.h>
#include <helper.h>
#include <javaRandom.h>
#include <noiseOctaves.h>
#include <blockHelper.h>

#include <cstdlib>
#include <ctime>

class Chunk;

/**
 * @brief Generic Generator object that makes an empty world
 * 
 */
class Generator {
  public:
	Generator(int64_t seed, int divisor = 1);
	virtual ~Generator();
	virtual Chunk GenerateChunk(Int2 chunkPos);
	virtual bool PopulateChunk(Int2 chunkPos);
	int64_t seed;
	int octave_divisor = 1;
};