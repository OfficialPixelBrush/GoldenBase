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

// Terrain octaves: full count only at stride 1. Each doubling of stride
// drops two of the finest octaves; a coarse core is always kept.
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

// Sand/gravel beach noise is only 4 octaves. Keep it through a few zoom-out
// steps so coasts stay sandy, then drop it once pixels are much coarser
// than a beach is wide.
inline int32_t SurfaceOctaveBudget(int32_t fullOctaves, int32_t stride) {
	if (fullOctaves <= 0)
		return 0;
	if (stride <= 4)
		return fullOctaves;
	if (stride <= 8)
		return std::max(2, fullOctaves / 2);
	return 0;
}

// Biome octaves are few (2–4) and define map-scale climate boundaries, so
// they drop much more slowly: full detail through 8:1, then one octave per
// further doubling, never below 2.
inline int32_t BiomeOctaveBudget(int32_t fullOctaves, int32_t stride) {
	if (fullOctaves <= 0)
		return 0;
	if (stride <= 8)
		return fullOctaves;
	int32_t drops = 0;
	for (int32_t s = stride >> 3; s > 1; s >>= 1)
		++drops;
	return std::max(std::min(2, fullOctaves), fullOctaves - drops);
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

inline bool IsSnowyBiome(Biome biome) {
	return biome == BIOME_TAIGA || biome == BIOME_TUNDRA || biome == BIOME_ICEDESERT;
}

// Preview surface after height is known. Ice is the *ocean surface* (any
// seafloor depth) when the column is cold — matching Beta's
// temp < 0.5 && y >= 63 water freeze. Snow uses the same height-adjusted
// temperature as PseudoPopulateChunk.
inline void FinishColumnSurface(TileColumn &col, bool hasBiomes, bool snowWorld, bool snowMode = false) {
	const float heightAdjTemp = col.temperature - float(col.height - WATER_LEVEL) / 64.0f * 0.3f;
	const bool frozenWater = snowWorld || col.temperature < 0.5f || IsSnowyBiome(col.biome);
	const bool snowCover = snowMode && (snowWorld || heightAdjTemp < 0.5f || IsSnowyBiome(col.biome));

	if (col.height < WATER_LEVEL) {
		col.surface = frozenWater ? BLOCK_ICE : BLOCK_WATER_STILL;
	} else if (snowCover) {
		col.surface = BLOCK_SNOW_LAYER;
	} else if (hasBiomes) {
		col.surface = GetTopBlock(col.biome);
	} else {
		col.surface = BLOCK_GRASS;
	}
}

// Beta/Alpha ReplaceBlocksForBiome: sand/gravel only near sea level.
inline void ApplyCoastalBeach(TileColumn &col, double sandNoise, double gravelNoise) {
	if (col.height < WATER_LEVEL || col.height > WATER_LEVEL + 1)
		return;
	if (col.surface == BLOCK_WATER_STILL || col.surface == BLOCK_WATER_FLOWING || col.surface == BLOCK_ICE ||
		col.surface == BLOCK_SNOW_LAYER || col.surface == BLOCK_SNOW)
		return;
	if (sandNoise > 0.0)
		col.surface = BLOCK_SAND;
	else if (gravelNoise > 3.0)
		col.surface = BLOCK_GRAVEL;
}
