#pragma once

#include "biomes.h"
#include "blocks.h"
#include "datatypes.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// One output column of a zoomed-out WASM tile. Filled by Generator::SampleColumns.
struct TileColumn {
	int8_t height = WATER_LEVEL;
	Biome biome = BIOME_NONE;
	BlockType surface = BLOCK_GRASS;
	float temperature = 0.5f;
	float humidity = 0.5f;
};

// Full octave count only at stride 1 (1:1 block/pixel). Each doubling of
// stride drops two of the finest octaves; a coarse core is always kept so
// continents / large hills still read correctly when zoomed out.
inline int32_t OctaveBudget(int32_t fullOctaves, int32_t stride) {
	if (fullOctaves <= 0)
		return 0;
	if (stride <= 1)
		return fullOctaves;
	int32_t drops = 0;
	for (int32_t s = stride; s > 1; s >>= 1)
		drops += 2;
	const int32_t minKeep = std::max(2, fullOctaves / 4);
	return std::max(minKeep, fullOctaves - drops);
}

// Vertical density samples for the preview path. Native terrain uses 17
// samples (every 8 blocks). Zoomed-out views don't need that.
inline int32_t VerticalSamples(int32_t stride) {
	if (stride <= 2)
		return 9;
	if (stride <= 8)
		return 5;
	return 3;
}

// Native terrain noise is parameterized in 4-block units with 17 samples
// covering y = 0..16. Preview sampling uses (origin + i * stride) in blocks.
struct TerrainSampleCoords {
	double coordX;
	double coordZ;
	double scaleXZ;
	double scaleY;
};

inline TerrainSampleCoords MakeTerrainSampleCoords(int32_t originX, int32_t originZ, int32_t stride,
												   int32_t yCount) {
	const double ngStep = double(stride) / 4.0;
	TerrainSampleCoords c;
	c.coordX = double(originX) / double(stride);
	c.coordZ = double(originZ) / double(stride);
	c.scaleXZ = 684.412 * ngStep;
	c.scaleY = 684.412 * (16.0 / double(std::max(1, yCount - 1)));
	return c;
}

// Highest solid surface from a density column sampled every `yStep` blocks.
inline int8_t HeightFromDensityColumn(const double *col, int32_t yCount, double yStep) {
	int32_t lastSolid = -1;
	for (int32_t y = 0; y < yCount; ++y) {
		if (col[y] > 0.0)
			lastSolid = y;
	}
	if (lastSolid < 0)
		return 0;
	if (lastSolid >= yCount - 1) {
		int h = int((yCount - 1) * yStep);
		return int8_t(std::min(CHUNK_HEIGHT - 1, std::max(1, h)));
	}
	const double d0 = col[lastSolid];
	const double d1 = col[lastSolid + 1];
	double t = 0.0;
	if (d0 != d1)
		t = d0 / (d0 - d1);
	if (t < 0.0)
		t = 0.0;
	if (t > 1.0)
		t = 1.0;
	int h = int((double(lastSolid) + t) * yStep) + 1;
	if (h < 1)
		h = 1;
	if (h > CHUNK_HEIGHT - 1)
		h = CHUNK_HEIGHT - 1;
	return int8_t(h);
}

inline void FinishColumnSurface(TileColumn &col, bool hasBiomes, bool snowWorld) {
	if (col.height < WATER_LEVEL) {
		const bool ice = snowWorld || col.temperature < 0.5f;
		col.surface = (ice && col.height >= WATER_LEVEL - 1) ? BLOCK_ICE : BLOCK_WATER_STILL;
	} else if (hasBiomes) {
		col.surface = GetTopBlock(col.biome);
	} else {
		col.surface = BLOCK_GRASS;
	}
}
