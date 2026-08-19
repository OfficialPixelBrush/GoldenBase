#include "generator.h"
#include "beta173Caver.h"

/**
 * @brief A faithful reimplementation of the Infdev 20100611 world generator
 * 
 */
class GeneratorInfdev20100611 : public Generator {
  private:
	JavaRandom rand;
	NoiseOctavesPerlin noiseGen1;
	NoiseOctavesPerlin noiseGen2;
	NoiseOctavesPerlin noiseGen3;
	NoiseOctavesPerlin noiseGen4;
	NoiseOctavesPerlin noiseGen5;
	NoiseOctavesPerlin noiseGen6;
	NoiseOctavesPerlin noiseGen7;
	//NoiseOctavesPerlin mobSpawnerNoise;
	double InitializeNoiseField(double var1, double var3, double var5);
    std::vector<double> noise1;
    std::vector<double> noise2;
    std::vector<double> noise3;
    std::vector<double> noise6;
    std::vector<double> noise7;
    std::vector<double> noiseArray;
	Beta173Caver caver;
	void FillDensityStrip(std::vector<double> &terrainMap, double coordX, double coordZ, double scaleXZ, double scaleY,
						  Int3 max);

  public:
	GeneratorInfdev20100611(int64_t seed, float multiplier);
	~GeneratorInfdev20100611() = default;
	Chunk GenerateChunk(Int2 chunkPos) override;
	bool PopulateChunk(Int2 chunkPos) override;
	void SetDetailLevel(int32_t stride) override;
	void SampleColumns(int32_t originX, int32_t originZ, int32_t samples, int32_t stride, TileColumn *out) override;
	bool infdev20100616 = false;
};