#pragma once
#include "biomes.h"
#include "noise_octaves_simplex.h"

/**
 * @brief A faithful reimplementation of the Beta 1.7.3 biome generator
 * 
 */
class Beta173Biome {
    private:
        // Simplex Noise Generators
        NoiseOctavesSimplex temperatureNoiseGen;
        NoiseOctavesSimplex humidityNoiseGen;
        NoiseOctavesSimplex weirdnessNoiseGen;
    public:
        Beta173Biome();
        Beta173Biome(int64_t seed);
        void GenerateBiomeMap(std::vector<Biome>& biomeMap, std::vector<double>& temperature, std::vector<double>& humidity, std::vector<double>& weirdness, Int2 blockPos, Int2 max, double step = 1.0);
	    void GenerateTemperature(std::vector<double>& temperature, std::vector<double>& weirdness, Int2 chunkPos, Int2 max);
		void SetDetailLevel(int32_t stride);
};