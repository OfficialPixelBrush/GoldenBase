#include "generator.h"

// Prepare the Generator to utilize some preset numbers and functions
Generator::Generator([[maybe_unused]] int64_t pSeed, [[maybe_unused]] float multiplier) {
	seed = pSeed;
	octave_multiplier = multiplier;
}

Generator::~Generator() {}

Chunk Generator::GenerateChunk([[maybe_unused]] Int2 chunkPos) {
	return Chunk(chunkPos);
}

bool Generator::PopulateChunk([[maybe_unused]] Int2 chunkPos) { return true; }