#include "generator.h"

/**
 * @brief A faithful reimplementation of the Infdev 20100227 world generator (with seed support)
 * 
 */
class GeneratorInfdev20100227 : public Generator {
  private:
	JavaRandom rand;
	NoiseOctaves<NoisePerlin> noiseGen1;
	NoiseOctaves<NoisePerlin> noiseGen2;
	NoiseOctaves<NoisePerlin> noiseGen3;
	NoiseOctaves<NoisePerlin> noiseGen4;
	NoiseOctaves<NoisePerlin> noiseGen5;
	NoiseOctaves<NoisePerlin> noiseGen6;

  public:
	GeneratorInfdev20100227(int64_t seed, float multiplier);
	~GeneratorInfdev20100227() = default;
	Chunk GenerateChunk(Int2 chunkPos) override;
	bool PopulateChunk(Int2 chunkPos) override;
	void SetDetailLevel(int32_t stride) override;
	void SampleColumns(int32_t originX, int32_t originZ, int32_t samples, int32_t stride, TileColumn *out) override;
};