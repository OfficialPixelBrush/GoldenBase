#include "beta173Caver.h"
//#include "beta173Feature.h"
//#include "beta173Tree.h"
#include "beta173Biome.h"
#include "biomes.h"
#include "generator.h"
#include "blockHelper.h"

/**
 * @brief A faithful reimplementation of the Alpha 1.1.2_01 Generator
 * 
 */
class GeneratorAlpha112_01 : public Generator {
  private:
	JavaRandom rand;
	// Perlin Noise Generators
	NoiseOctaves<NoisePerlin> lowNoiseGen;
	NoiseOctaves<NoisePerlin> highNoiseGen;
	NoiseOctaves<NoisePerlin> selectorNoiseGen;
	NoiseOctaves<NoisePerlin> sandGravelNoiseGen;
	NoiseOctaves<NoisePerlin> stoneNoiseGen;
	NoiseOctaves<NoisePerlin> continentalnessNoiseGen;
	NoiseOctaves<NoisePerlin> depthNoiseGen;
	NoiseOctaves<NoisePerlin> treeDensityNoiseGen;

	// Stored noise Fields
	std::vector<double> terrainNoiseField;
	std::vector<double> lowNoiseField;
	std::vector<double> highNoiseField;
	std::vector<double> selectorNoiseField;
	std::vector<double> continentalnessNoiseField;
	std::vector<double> depthNoiseField;

	std::vector<double> sandNoise;
	std::vector<double> gravelNoise;
	std::vector<double> stoneNoise;

	// Cave Gen
	Beta173Caver caver;

	void GenerateTerrain(Int2 chunkPos, Chunk &c);
	void GenerateTerrainNoise(std::vector<double> &terrainMap, Int3 chunkPos, Int3 max);
	void ReplaceSurfaceBlocks(Int2 chunkPos, Chunk &c);
	Biome GetBiomeAt(Int2 worldPos);

  public:
	GeneratorAlpha112_01(int64_t seed);
	~GeneratorAlpha112_01() = default;
	Chunk GenerateChunk(Int2 chunkPos) override;
	bool PopulateChunk(Int2 chunkPos) override;
};