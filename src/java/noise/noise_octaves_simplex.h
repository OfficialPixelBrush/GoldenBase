/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * Based on code by Mojang Studios (2011)
*/

#pragma once

#include "java_random.h"
#include "noise_simplex.h"
#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

class NoiseOctavesSimplex {
public:
	NoiseOctavesSimplex() {}
	NoiseOctavesSimplex(int32_t _octaves, int32_t _realOctaves = -1);
	NoiseOctavesSimplex(Java::Random& _rand, int32_t _octaves, int32_t _realOctaves = -1, bool = true);

	void SetDetail(int32_t _activeOctaves) { activeOctaves = _activeOctaves; }

	void GenerateOctaves(std::span<double> _noiseField, Vec2 _offset, Int32_2 _size, Vec2 _scale, double _lacunarity);
	void GenerateOctaves(std::span<double> _noiseField, Int32_2 _offset, Int32_2 _size, Vec2 _scale, double _lacunarity);
	void GenerateOctaves(std::span<double> _noiseField, Vec2 _offset, Int32_2 _size, Vec2 _scale, double _lacunarity,
	                     double _persistence);

	void GenerateOctaves(std::vector<double> &_noiseField, double _x, double _z, int32_t _sizeX, int32_t _sizeZ,
	                     double _scaleX, double _scaleZ, double _lacunarity);
	void GenerateOctaves(std::vector<double> &_noiseField, double _x, double _z, int32_t _sizeX, int32_t _sizeZ,
	                     double _scaleX, double _scaleZ, double _lacunarity, double _persistence);

private:
	int32_t octaves = 0;
	int32_t realOctaves = 0;
	int32_t activeOctaves = -1;
	std::vector<NoiseSimplex> generatorCollection;

	struct DetailPlan {
		int32_t skip;
		int32_t count;
	};

	DetailPlan PlanDetail() const {
		int32_t cap = realOctaves;
		if (cap > octaves)
			cap = octaves;
		int32_t n = activeOctaves >= 0 ? std::min(activeOctaves, cap) : cap;
		if (n < 0)
			n = 0;
		return DetailPlan{cap - n, n};
	}
};

inline NoiseOctavesSimplex::NoiseOctavesSimplex(int32_t _poctaves, int32_t _realOctaves)
    : octaves(_poctaves), realOctaves(_realOctaves) {
	if (realOctaves < 0)
		realOctaves = octaves;
	generatorCollection.reserve(size_t(octaves));
	for (size_t i = 0; i < size_t(octaves); ++i)
		generatorCollection.push_back(NoiseSimplex());
}

inline NoiseOctavesSimplex::NoiseOctavesSimplex(Java::Random& _rand, int32_t _poctaves, int32_t _realOctaves, bool)
    : octaves(_poctaves), realOctaves(_realOctaves) {
	if (realOctaves < 0)
		realOctaves = octaves;
	generatorCollection.reserve(size_t(octaves));
	for (size_t i = 0; i < size_t(octaves); ++i)
		generatorCollection.push_back(NoiseSimplex(_rand));
}

inline void NoiseOctavesSimplex::GenerateOctaves(std::span<double> _noiseField, Int32_2 _offset, Int32_2 _size,
                                                 Vec2 _scale, double _lacunarity) {
	this->GenerateOctaves(_noiseField, Vec2{double(_offset.x), double(_offset.y)}, _size, _scale, _lacunarity);
}

inline void NoiseOctavesSimplex::GenerateOctaves(std::span<double> _noiseField, Vec2 _offset, Int32_2 _size,
                                                 Vec2 _scale, double _lacunarity) {
	this->GenerateOctaves(_noiseField, _offset, _size, _scale, _lacunarity, 0.5);
}

inline void NoiseOctavesSimplex::GenerateOctaves(std::span<double> _noiseField, Vec2 _offset, Int32_2 _size,
                                                 Vec2 _scale, double _lacunarity, double _persistence) {
	_scale.x /= 1.5;
	_scale.y /= 1.5;
	const size_t count = size_t(_size.x) * size_t(_size.y);
	if (_noiseField.size() < count)
		return;

	std::fill_n(_noiseField.data(), count, 0.0);

	const DetailPlan plan = PlanDetail();
	if (plan.count <= 0)
		return;

	double frequency = 1.0;
	double amplitude = 1.0;
	for (int32_t i = 0; i < plan.skip; ++i) {
		amplitude *= _lacunarity;
		frequency *= _persistence;
	}

	for (int32_t octave = plan.skip; octave < plan.skip + plan.count; ++octave) {
		generatorCollection[size_t(octave)].GenerateNoise(_noiseField.first(count), _offset, _size, _scale * amplitude,
		                                                  0.55 / frequency);
		amplitude *= _lacunarity;
		frequency *= _persistence;
	}
}

inline void NoiseOctavesSimplex::GenerateOctaves(std::vector<double> &_noiseField, double _x, double _z, int32_t _sizeX,
                                                 int32_t _sizeZ, double _scaleX, double _scaleZ, double _lacunarity) {
	GenerateOctaves(_noiseField, _x, _z, _sizeX, _sizeZ, _scaleX, _scaleZ, _lacunarity, 0.5);
}

inline void NoiseOctavesSimplex::GenerateOctaves(std::vector<double> &_noiseField, double _x, double _z, int32_t _sizeX,
                                                 int32_t _sizeZ, double _scaleX, double _scaleZ, double _lacunarity,
                                                 double _persistence) {
	const size_t needed = size_t(_sizeX) * size_t(_sizeZ);
	_noiseField.assign(needed, 0.0);
	GenerateOctaves(std::span<double>(_noiseField), Vec2{_x, _z}, Int32_2{_sizeX, _sizeZ}, Vec2{_scaleX, _scaleZ},
	                _lacunarity, _persistence);
}
