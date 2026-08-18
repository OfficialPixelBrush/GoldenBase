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

void Generator::SetDetailLevel(int32_t stride) {
	lowDetail = stride > 1;
}

void Generator::SampleColumns(int32_t originX, int32_t originZ, int32_t samples, int32_t stride, TileColumn *out) {
	SetDetailLevel(stride);
	for (int32_t pz = 0; pz < samples; ++pz) {
		for (int32_t px = 0; px < samples; ++px) {
			const int32_t wx = originX + px * stride;
			const int32_t wz = originZ + pz * stride;
			Int2 chunkPos{wx >> 4, wz >> 4};
			const int32_t lx = wx & 15;
			const int32_t lz = wz & 15;
			Chunk c = GenerateChunk(chunkPos);
			TileColumn &col = out[pz * samples + px];
			col.height = c.GetHeightValue(uint8_t(lx), uint8_t(lz));
			col.biome = c.GetBiome(lx, lz);
			col.temperature = c.temperature[lx * CHUNK_WIDTH + lz];
			col.humidity = c.humidity[lx * CHUNK_WIDTH + lz];
			int topY = col.height;
			if (topY < 1)
				topY = 1;
			col.surface = c.GetBlockType(Int3{lx, topY, lz});
			if (col.surface == BLOCK_AIR && topY > 0)
				col.surface = c.GetBlockType(Int3{lx, topY - 1, lz});
		}
	}
}