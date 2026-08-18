#include "beta173Caver.h"
//#include "beta173Feature.h"
//#include "beta173Tree.h"
#include "beta173Biome.h"
#include "biomes.h"
#include "generator.h"
#include "blockHelper.h"

/**
 * @brief A faithful reimplementation of the Beta 1.7.3 world generator
 * 
 */
class GeneratorBeta173 : public Generator {
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
	//NoiseOctaves<NoisePerlin> treeDensityNoiseGen;

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

	// Biome Vectors
	std::vector<Biome> biomeMap;
	std::vector<double> temperature;
	std::vector<double> humidity;
	std::vector<double> weirdness;

	// Cave Gen
	Beta173Caver caver;
	Beta173Biome biomeGen;

	void GenerateTerrain(Int2 chunkPos, Chunk &c);
	void GenerateTerrainNoise(std::vector<double> &terrainMap, Int3 chunkPos, Int3 max);
	void FillDensityStrip(std::vector<double> &terrainMap, double coordX, double coordZ, double scaleXZ, double scaleY,
						  Int3 max, int32_t climateWidth, int32_t climateZ0);
	void ReplaceBlocksForBiome(Int2 chunkPos, Chunk &c);
	Biome GetBiomeAt(Int2 worldPos);
	bool PseudoPopulateChunk(Int2 chunkPos, Chunk &c);

  public:
	GeneratorBeta173(int64_t seed, float multiplier);
	~GeneratorBeta173() = default;
	Chunk GenerateChunk(Int2 chunkPos) override;
	bool PopulateChunk(Int2 chunkPos) override;
	void SetDetailLevel(int32_t stride) override;
	void SampleColumns(int32_t originX, int32_t originZ, int32_t samples, int32_t stride, TileColumn *out) override;
	bool gravelFix = true;
};