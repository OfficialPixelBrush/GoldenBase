#include "generatorInfdev20100227.h"
#include "java_math.h"
#include <algorithm>
#include <cstdint>

namespace {
constexpr int32_t kPyramidRegion = 1024;

Int2 InfdevPyramidOrigin(JavaRandom &rand, int32_t regionX, int32_t regionZ) {
	// Java int overflow on the seed mix.
	rand.setSeed(int64_t(int32_t(regionX + regionZ * 13871)));
	return Int2{(regionX << 10) + CHUNK_HEIGHT + rand.nextInt(512),
				(regionZ << 10) + CHUNK_HEIGHT + rand.nextInt(512)};
}

int32_t PyramidTopFromChebyshev(int32_t dist) {
	int32_t top = (CHUNK_HEIGHT - 1) - dist;
	if (top == 0xFF)
		top = 1;
	return top;
}

int32_t ChebyshevToRect(int32_t px, int32_t pz, int32_t x0, int32_t x1, int32_t z0, int32_t z1) {
	int32_t dx = 0;
	if (px < x0)
		dx = x0 - px;
	else if (px > x1)
		dx = px - x1;
	int32_t dz = 0;
	if (pz < z0)
		dz = z0 - pz;
	else if (pz > z1)
		dz = pz - z1;
	return dx > dz ? dx : dz;
}

void ApplyInfdevPyramid(JavaRandom &rand, int32_t blockX, int32_t blockZ, int32_t stride, TileColumn &col) {
	int32_t x0 = blockX;
	int32_t z0 = blockZ;
	int32_t x1 = blockX;
	int32_t z1 = blockZ;
	// Expand to the whole sample cell so ~255-block pyramids still register
	// when a pixel is larger than one block. Past one region per pixel, every
	// cell would contain a pyramid — keep point sampling there.
	if (stride > 1 && stride < kPyramidRegion) {
		x1 = blockX + stride - 1;
		z1 = blockZ + stride - 1;
	}

	int32_t rx0 = x0 / kPyramidRegion;
	int32_t rx1 = x1 / kPyramidRegion;
	int32_t rz0 = z0 / kPyramidRegion;
	int32_t rz1 = z1 / kPyramidRegion;
	if (rx1 < rx0)
		std::swap(rx0, rx1);
	if (rz1 < rz0)
		std::swap(rz0, rz1);

	int32_t bestTop = -1;
	for (int32_t rx = rx0; rx <= rx1; ++rx) {
		for (int32_t rz = rz0; rz <= rz1; ++rz) {
			const Int2 origin = InfdevPyramidOrigin(rand, rx, rz);
			const int32_t dist = ChebyshevToRect(origin.x, origin.y, x0, x1, z0, z1);
			bestTop = std::max(bestTop, PyramidTopFromChebyshev(dist));
		}
	}

	if (bestTop > int32_t(col.height)) {
		bestTop = std::clamp(bestTop, 1, CHUNK_HEIGHT - 1);
		col.height = int8_t(bestTop);
		col.surface = BLOCK_BRICKS;
	}
}
} // namespace

GeneratorInfdev20100227::GeneratorInfdev20100227(int64_t pSeed, float multiplier) : Generator(pSeed, multiplier) {
	this->seed = pSeed;

	rand = JavaRandom(this->seed);
	noiseGen1 = NoiseOctaves<NoisePerlin>(rand, 16,16);
	noiseGen2 = NoiseOctaves<NoisePerlin>(rand, 16,16);
	noiseGen3 = NoiseOctaves<NoisePerlin>(rand, 8 , 8);
	noiseGen4 = NoiseOctaves<NoisePerlin>(rand, 4 , 4);
	noiseGen5 = NoiseOctaves<NoisePerlin>(rand, 4 , 4);
	noiseGen6 = NoiseOctaves<NoisePerlin>(rand, 5 , 5);
}

Chunk GeneratorInfdev20100227::GenerateChunk(Int2 chunkPos) {
	Chunk c(chunkPos);
	c.state = ChunkState::Generating;
	int32_t chunkStartX = chunkPos.x << 4;
	int32_t chunkStartZ = chunkPos.y << 4;
	int32_t blockIndex = 0;

	for (int32_t blockX = chunkStartX; blockX < chunkStartX + 16; ++blockX) {
		for (int32_t blockZ = chunkStartZ; blockZ < chunkStartZ + 16; ++blockZ) {
			int32_t regionX = blockX / 1024;
			int32_t regionZ = blockZ / 1024;
			// Generate terrain height
			float noiseGen1Value =
				float(this->noiseGen1.GenerateOctaves(double((float)blockX / 0.03125F), 0.0,
															   double((float)blockZ / 0.03125F)) -
						this->noiseGen2.GenerateOctaves(double((float)blockX / 0.015625F), 0.0,
															   double((float)blockZ / 0.015625F))) /
				512.0F / 4.0F;
			float noiseGen5Value = (float)this->noiseGen5.GenerateOctaves(double((float)blockX / 4.0F),
																				 double((float)blockZ / 4.0F));
			float noiseGen6Value = (float)this->noiseGen6.GenerateOctaves(double((float)blockX / 8.0F),
																				 double((float)blockZ / 8.0F)) /
								   8.0F;
			noiseGen5Value =
				noiseGen5Value > 0.0F
					? float(this->noiseGen3.GenerateOctaves(double((float)blockX * 0.25714284F * 2.0F),
																	 double((float)blockZ * 0.25714284F * 2.0F)) *
							  (double)noiseGen6Value / 4.0)
					: float(this->noiseGen4.GenerateOctaves(double((float)blockX * 0.25714284F),
																	 double((float)blockZ * 0.25714284F)) *
							  (double)noiseGen6Value);
			int32_t terrainHeight = Java::FloatToInt32(noiseGen1Value + 64.0F + noiseGen5Value);
			if ((float)this->noiseGen5.GenerateOctaves((double)blockX, (double)blockZ) < 0.0F) {
				terrainHeight = terrainHeight / 2 << 1;
				if ((float)this->noiseGen5.GenerateOctaves(double(blockX / 5), double(blockZ / 5)) < 0.0F) {
					++terrainHeight;
				}
			}

			// Generate value for chunk decorations
			// TODO: Maybe replace this with java random for accuracy?
			float decorationChance = static_cast<float>(std::rand()) / float(RAND_MAX);

			for (int32_t blockY = 0; blockY < CHUNK_HEIGHT; ++blockY) {
				// Determine Block Type based on parameters
				BlockType blockType = BLOCK_AIR;
				if ((blockX == 0 || blockZ == 0) && blockY <= terrainHeight + 2) {
					blockType = BLOCK_OBSIDIAN;
				} else if (blockY == terrainHeight + 1 && terrainHeight >= WATER_LEVEL && decorationChance < 0.02f) {
					// Skip this, just adds clutter + relies on JavaRandom, so unreliable!
					//blockType = BLOCK_DANDELION;
				} else if (blockY == terrainHeight && terrainHeight >= WATER_LEVEL) {
					blockType = BLOCK_GRASS;
				} else if (blockY <= terrainHeight - 2) {
					blockType = BLOCK_STONE;
				} else if (blockY <= terrainHeight) {
					blockType = BLOCK_DIRT;
				} else if (blockY <= WATER_LEVEL) {
					blockType = BLOCK_WATER_STILL;
				}

				// Generate Brick Pyramids
				this->rand.setSeed(int64_t(regionX + regionZ * 13871));
				int32_t pyramidOffsetX = (regionX << 10) + CHUNK_HEIGHT + this->rand.nextInt(512);
				int32_t pyramidOffsetZ = (regionZ << 10) + CHUNK_HEIGHT + this->rand.nextInt(512);
				pyramidOffsetX = blockX - pyramidOffsetX;
				pyramidOffsetZ = blockZ - pyramidOffsetZ;
				if (pyramidOffsetX < 0)
					pyramidOffsetX = -pyramidOffsetX;
				if (pyramidOffsetZ < 0)
					pyramidOffsetZ = -pyramidOffsetZ;
				if (pyramidOffsetZ > pyramidOffsetX)
					pyramidOffsetX = pyramidOffsetZ;
				pyramidOffsetX = (CHUNK_HEIGHT - 1) - pyramidOffsetX;
				if (pyramidOffsetX == 0xFF)
					pyramidOffsetX = 1;
				if (pyramidOffsetX < terrainHeight)
					pyramidOffsetX = terrainHeight;
				if (blockY <= pyramidOffsetX && (blockType == BLOCK_AIR || blockType == BLOCK_WATER_STILL)) {
					blockType = BLOCK_BRICKS;
				}

				// Clamping
				if (blockType < BLOCK_AIR)
					blockType = BLOCK_AIR;

				c.SetBlockType(blockType, BlockIndexToPosition(blockIndex++));
			}
		}
	}
	// To prevent population
	if (!lowDetail)
		c.GenerateHeightMap();
	c.state = ChunkState::Generated;
	return c;
}

void GeneratorInfdev20100227::SetDetailLevel(int32_t stride) {
	Generator::SetDetailLevel(stride);
	noiseGen1.SetDetail(OctaveBudget(16, stride));
	noiseGen2.SetDetail(OctaveBudget(16, stride));
	noiseGen3.SetDetail(OctaveBudget(8, stride));
	noiseGen4.SetDetail(OctaveBudget(4, stride));
	noiseGen5.SetDetail(OctaveBudget(4, stride));
	noiseGen6.SetDetail(OctaveBudget(5, stride));
}

void GeneratorInfdev20100227::SampleColumns(int32_t originX, int32_t originZ, int32_t samples, int32_t stride, TileColumn *out) {
	SetDetailLevel(stride);
	for (int32_t pz = 0; pz < samples; ++pz) {
		for (int32_t px = 0; px < samples; ++px) {
			const int32_t blockX = originX + px * stride;
			const int32_t blockZ = originZ + pz * stride;
			float noiseGen1Value =
				float(this->noiseGen1.GenerateOctaves(double((float)blockX / 0.03125F), 0.0,
													  double((float)blockZ / 0.03125F)) -
					  this->noiseGen2.GenerateOctaves(double((float)blockX / 0.015625F), 0.0,
													  double((float)blockZ / 0.015625F))) /
				512.0F / 4.0F;
			float noiseGen5Value = (float)this->noiseGen5.GenerateOctaves(double((float)blockX / 4.0F),
																		  double((float)blockZ / 4.0F));
			float noiseGen6Value = (float)this->noiseGen6.GenerateOctaves(double((float)blockX / 8.0F),
																		  double((float)blockZ / 8.0F)) /
								   8.0F;
			noiseGen5Value =
				noiseGen5Value > 0.0F
					? float(this->noiseGen3.GenerateOctaves(double((float)blockX * 0.25714284F * 2.0F),
															double((float)blockZ * 0.25714284F * 2.0F)) *
							(double)noiseGen6Value / 4.0)
					: float(this->noiseGen4.GenerateOctaves(double((float)blockX * 0.25714284F),
															double((float)blockZ * 0.25714284F)) *
							(double)noiseGen6Value);
			int32_t terrainHeight = Java::FloatToInt32(noiseGen1Value + 64.0F + noiseGen5Value);
			if ((float)this->noiseGen5.GenerateOctaves((double)blockX, (double)blockZ) < 0.0F) {
				terrainHeight = terrainHeight / 2 << 1;
				if ((float)this->noiseGen5.GenerateOctaves(double(blockX / 5), double(blockZ / 5)) < 0.0F) {
					++terrainHeight;
				}
			}
			if (terrainHeight < 1)
				terrainHeight = 1;
			if (terrainHeight > CHUNK_HEIGHT - 1)
				terrainHeight = CHUNK_HEIGHT - 1;

			TileColumn &c = out[pz * samples + px];
			c.height = int8_t(terrainHeight);
			c.biome = BIOME_NONE;
			c.temperature = 1.0f;
			c.humidity = 0.5f;
			if (terrainHeight < WATER_LEVEL) {
				c.surface = BLOCK_WATER_STILL;
				c.height = WATER_LEVEL;
			} else {
				c.surface = BLOCK_GRASS;
			}
			ApplyInfdevPyramid(this->rand, blockX, blockZ, stride, c);
		}
	}
}

// Do nothing, since population didn't exist yet
bool GeneratorInfdev20100227::PopulateChunk([[maybe_unused]] Int2 chunkPos) { return true; }
