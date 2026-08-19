#include "generator.h"


/**
 * @brief A faithful reimplementation of the Infdev 20100420 world generator
 * 
 */
class GeneratorInfdev20100420 : public Generator {
  private:
	JavaRandom rand;
	NoiseOctaves<NoisePerlin> noiseGen1;
	NoiseOctaves<NoisePerlin> noiseGen2;
	NoiseOctaves<NoisePerlin> noiseGen3;
	NoiseOctaves<NoisePerlin> noiseGen4;
	NoiseOctaves<NoisePerlin> noiseGen5;
	//NoiseOctaves<NoisePerlin> noiseGen6;
	//NoiseOctaves<NoisePerlin> mobSpawnerNoise;
	double InitializeNoiseField(double var1, double var3, double var5);
    std::vector<double> noise1;
    std::vector<double> noise2;
    std::vector<double> noise3;
    std::vector<double> noiseArray;
	void FillDensityStrip(std::vector<double> &terrainMap, double coordX, double coordZ, double scaleXZ, double scaleY,
						  Int3 max);

  public:
	GeneratorInfdev20100420(int64_t seed, float multiplier);
	~GeneratorInfdev20100420() = default;
	Chunk GenerateChunk(Int2 chunkPos) override;
	bool PopulateChunk(Int2 chunkPos) override;
	void SetDetailLevel(int32_t stride) override;
	void SampleColumns(int32_t originX, int32_t originZ, int32_t samples, int32_t stride, TileColumn *out) override;
	bool infdev20100413 = false;
};