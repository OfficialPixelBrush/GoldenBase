#include "generator.h"

// Prepare the Generator to utilize some preset numbers and functions
Generator::Generator([[maybe_unused]] int64_t pSeed) {}

Generator::~Generator() {}

Chunk Generator::GenerateChunk([[maybe_unused]] Int2 chunkPos) {
	return Chunk(chunkPos);
}

bool Generator::PopulateChunk([[maybe_unused]] Int2 chunkPos) { return true; }