#include "generatorInfdev20100420.h"

GeneratorInfdev20100420::GeneratorInfdev20100420(int64_t pSeed, float multiplier) : Generator(pSeed, multiplier) {
	this->seed = pSeed;

	rand = JavaRandom(this->seed);
	// Consume an extra Random construction to match Java's `new Random(var2)` no-op
	JavaRandom(this->seed);
	noiseGen1 = NoiseOctaves<NoisePerlin>(rand, 16,16);
	noiseGen2 = NoiseOctaves<NoisePerlin>(rand, 16,16);
	noiseGen3 = NoiseOctaves<NoisePerlin>(rand, 8 , 8);
	noiseGen4 = NoiseOctaves<NoisePerlin>(rand, lowDetail ? 0 : 4 , 4);
	noiseGen5 = NoiseOctaves<NoisePerlin>(rand, lowDetail ? 0 : 4 , 4);
	//noiseGen6 = NoiseOctaves<NoisePerlin>(rand, 5 , 5); // Unused
	//mobSpawnerNoise = NoiseOctaves<NoisePerlin>(rand, 5, 5);
}

Chunk GeneratorInfdev20100420::GenerateChunk(Int2 chunkPos) {
	Chunk c(chunkPos);
	c.state = ChunkState::Generating;
	rand.setSeed(int64_t(chunkPos.x) * 341873128712L + int64_t(chunkPos.y) * 132897987541L);
	//c.ClearChunk();

	int32_t chunkX = chunkPos.x << 2;
	int32_t chunkZ = chunkPos.y << 2;
    constexpr int sampleWidth = 5;
    constexpr int sampleHeight = 17;

	// Build the 5x17x5 noise array (Java: noiseArray[425])
	// Axes: [x(5)][z(5)][y(17)], flattened as (x*5+z)*17+y
	noiseGen3.GenerateOctaves(noise3, chunkX, 0, chunkZ, sampleWidth, sampleHeight, sampleWidth, 8.555150000000001, 4.277575000000001, 8.555150000000001);
	noiseGen1.GenerateOctaves(noise1, chunkX, 0, chunkZ, sampleWidth, sampleHeight, sampleWidth, 684.412, 684.412, 684.412);
	noiseGen2.GenerateOctaves(noise2, chunkX, 0, chunkZ, sampleWidth, sampleHeight, sampleWidth, 684.412, 684.412, 684.412);

	// Combine noise layers into noiseArray
	if (noiseArray.empty()) {
		noiseArray.resize(size_t(sampleWidth * sampleHeight * sampleWidth), 0.0);
	} else {
		for (size_t i = 0; i < noiseArray.size(); ++i) {
			noiseArray[i] = 0.0;
		}
	}

	int32_t noiseIdx = 0;
	for (int32_t noiseX = 0; noiseX < sampleWidth; ++noiseX) {
		for (int32_t noiseZ = 0; noiseZ < sampleWidth; ++noiseZ) {
			for (int32_t noiseY = 0; noiseY < sampleHeight; ++noiseY) {
				// Vertical density falloff: centres around y=8.5, scaled to ~128 blocks
				double densityOffset = ((double)noiseY - 8.5) * 12.0;
				if (densityOffset < 0.0) {
					densityOffset *= 2.0;
				}

				double n1  = noise1[noiseIdx] / 512.0;
				double n2  = noise2[noiseIdx] / 512.0;
				double n3t = (noise3[noiseIdx] / 10.0 + 1.0) / 2.0;

				double density;
				if (n3t < 0.0) {
					density = n1;
				} else if (n3t > 1.0) {
					density = n2;
				} else {
					density = n1 + (n2 - n1) * n3t;
				}

				density -= densityOffset;
				noiseArray[noiseIdx] = density;
				++noiseIdx;
			}
		}
	}

	// Terrain shape generation — trilinear interpolation over 4×4×8 sub-cells
	for (int32_t macroX = 0; macroX < sampleWidth-1; ++macroX) {
		for (int32_t macroZ = 0; macroZ < sampleWidth-1; ++macroZ) {
			for (int32_t macroY = 0; macroY < sampleHeight-1; ++macroY) {
				// Fetch the 8 corners of this macro-cell from noiseArray
				double n000 = noiseArray[(macroX * sampleWidth + macroZ)       * sampleHeight + macroY];
				double n001 = noiseArray[(macroX * sampleWidth + macroZ + 1)   * sampleHeight + macroY];
				double n100 = noiseArray[((macroX + 1) * sampleWidth + macroZ) * sampleHeight + macroY];
				double n101 = noiseArray[((macroX + 1) * sampleWidth + macroZ + 1) * sampleHeight + macroY];

				double n010 = noiseArray[(macroX * sampleWidth + macroZ)       * sampleHeight + macroY + 1];
				double n011 = noiseArray[(macroX * sampleWidth + macroZ + 1)   * sampleHeight + macroY + 1];
				double n110 = noiseArray[((macroX + 1) * sampleWidth + macroZ) * sampleHeight + macroY + 1];
				double n111 = noiseArray[((macroX + 1) * sampleWidth + macroZ + 1) * sampleHeight + macroY + 1];

				for (int32_t subY = 0; subY < 8; ++subY) {
					double lerpY = (double)subY / 8.0;
					double y00 = n000 + (n010 - n000) * lerpY;
					double y01 = n001 + (n011 - n001) * lerpY;
					double y10 = n100 + (n110 - n100) * lerpY;
					double y11 = n101 + (n111 - n101) * lerpY;

					for (int32_t subX = 0; subX < 4; ++subX) {
						double lerpX = (double)subX / 4.0;
						double xy0 = y00 + (y10 - y00) * lerpX;
						double xy1 = y01 + (y11 - y01) * lerpX;

						int32_t blockIndex =
							(subX + (macroX << 2)) << 11 |
							(0    + (macroZ << 2)) << 7  |
							(macroY << 3) + subY;

						for (int32_t subZ = 0; subZ < 4; ++subZ) {
							double lerpZ = (double)subZ / 4.0;
							double terrainDensity = xy0 + (xy1 - xy0) * lerpZ;

							BlockType blockType = BLOCK_AIR;
							if ((macroY << 3) + subY < WATER_LEVEL) {
								blockType = BLOCK_WATER_STILL;
							}

							if (terrainDensity > 0.0) {
								blockType = BLOCK_STONE;
							}

							c.SetBlockType(blockType, BlockIndexToPosition(blockIndex));
							blockIndex += CHUNK_HEIGHT;
						}
					}
				}
			}
		}
	}

	// "Biome" surface blocks (matches Java's infdev20100413 branch)
	for (int32_t blockX = 0; blockX < CHUNK_WIDTH; ++blockX) {
		for (int32_t blockZ = 0; blockZ < CHUNK_WIDTH; ++blockZ) {
			double nX = (double)((chunkPos.x << 4) + blockX);
			double nZ = (double)((chunkPos.y << 4) + blockZ);

			bool sandActive   = (this->noiseGen4.GenerateOctaves(nX * (1.0 / 32.0), nZ * (1.0 / 32.0), 0.0)
			                     + this->rand.nextDouble() * 0.2) > 0.0;
			bool gravelActive = (this->noiseGen4.GenerateOctaves(nZ * (1.0 / 32.0), 109.0134, nX * (1.0 / 32.0))
			                     + this->rand.nextDouble() * 0.2) > 3.0;
			int32_t stoneDepth = int32_t(this->noiseGen5.GenerateOctaves(nX * (1.0 / 32.0) * 2.0,
			                                                               nZ * (1.0 / 32.0) * 2.0)
			                             / 3.0 + 3.0 + this->rand.nextDouble() * 0.25);

			int32_t blockIndex = blockX << 11 | blockZ << 7 | 127;
			int32_t depth = -1;
			BlockType topBlock    = BLOCK_GRASS;
			BlockType fillerBlock = BLOCK_DIRT;

			for (int32_t blockY = CHUNK_HEIGHT - 1; blockY >= 0; --blockY) {
				if (c.GetBlockType(BlockIndexToPosition(blockIndex)) == BLOCK_AIR) {
					depth = -1;
				} else if (c.GetBlockType(BlockIndexToPosition(blockIndex)) == BLOCK_STONE) {
					if (depth == -1) {
						if (stoneDepth <= 0) {
							topBlock    = BLOCK_AIR;
							fillerBlock = BLOCK_STONE;
						} else if (blockY >= 60 && blockY <= 65) {
							topBlock    = BLOCK_GRASS;
							fillerBlock = BLOCK_DIRT;
							if (gravelActive) topBlock    = BLOCK_AIR;
							if (gravelActive) fillerBlock = BLOCK_GRAVEL;
							if (sandActive)   topBlock    = BLOCK_SAND;
							if (sandActive)   fillerBlock = BLOCK_SAND;
						}

						if (blockY < WATER_LEVEL && topBlock == BLOCK_AIR) {
							topBlock = BLOCK_WATER_STILL;
						}

						depth = stoneDepth;
						if (blockY >= WATER_LEVEL - 1) {
							c.SetBlockType(topBlock, BlockIndexToPosition(blockIndex));
						} else {
							c.SetBlockType(fillerBlock, BlockIndexToPosition(blockIndex));
						}
					} else if (depth > 0) {
						--depth;
						c.SetBlockType(fillerBlock, BlockIndexToPosition(blockIndex));
					}
				}

				--blockIndex;
			}
		}
	}

	c.GenerateHeightMap();
	c.state = ChunkState::Generated;
	return c;
}

bool GeneratorInfdev20100420::PopulateChunk(Int2 chunkPos) {
    return true;
}