/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * Based on code by Mojang Studios (2011)
*/

#pragma once

#include "java_random.h"
#include "noise_perlin.h"
#include <algorithm>
#include <span>
#include <vector>

class NoiseOctavesPerlin {
public:
	NoiseOctavesPerlin() {}
	NoiseOctavesPerlin(int32_t _octaves, int32_t _realOctaves = -1);
	NoiseOctavesPerlin(Java::Random& _rand, int32_t _octaves, int32_t _realOctaves = -1, bool = true);

	void SetDetail(int32_t _activeOctaves) { activeOctaves = _activeOctaves; }

	double GenerateOctaves(Vec2 _offset);
	double GenerateOctaves(Vec3 _offset);
	double GenerateOctaves(double _x, double _y);
	double GenerateOctaves(double _x, double _y, double _z);

	void GenerateOctaves(std::span<double> _noiseField, Vec3 _coordinate, Int32_3 _size, Vec3 _pScale);
	void GenerateOctaves(std::span<double> _noiseField, Int32_2 _offset, Int32_2 _size, Vec2 _scale,
	                     [[maybe_unused]] double _unused);

	void GenerateOctaves(std::vector<double> &_noiseField, double _coordX, double _coordY, double _coordZ, int32_t _sizeX,
	                     int32_t _sizeY, int32_t _sizeZ, double _scaleX, double _scaleY, double _scaleZ);
	void GenerateOctaves(std::vector<double> &_noiseField, int32_t _x, int32_t _z, int32_t _sizeX, int32_t _sizeZ,
	                     double _scaleX, double _scaleZ, [[maybe_unused]] double _unused);

private:
	int32_t octaves = 0;
	int32_t realOctaves = 0;
	int32_t activeOctaves = -1;
	std::vector<NoisePerlin> generatorCollection;

	struct DetailPlan {
		int32_t skip;
		int32_t count;
		double startScale;
	};

	DetailPlan PlanDetail(double _scaleBase = 2.0) const {
		int32_t cap = realOctaves;
		if (cap > octaves)
			cap = octaves;
		int32_t n = activeOctaves >= 0 ? std::min(activeOctaves, cap) : cap;
		if (n < 0)
			n = 0;
		int32_t skip = cap - n;
		double startScale = 1.0;
		for (int32_t i = 0; i < skip; ++i)
			startScale /= _scaleBase;
		return DetailPlan{skip, n, startScale};
	}
};

inline NoiseOctavesPerlin::NoiseOctavesPerlin(int32_t _poctaves, int32_t _realOctaves)
    : octaves(_poctaves), realOctaves(_realOctaves) {
	if (realOctaves < 0)
		realOctaves = octaves;
	generatorCollection.reserve(size_t(octaves));
	for (size_t i = 0; i < size_t(octaves); ++i)
		generatorCollection.push_back(NoisePerlin());
}

inline NoiseOctavesPerlin::NoiseOctavesPerlin(Java::Random& _rand, int32_t _poctaves, int32_t _realOctaves, bool)
    : octaves(_poctaves), realOctaves(_realOctaves) {
	if (realOctaves < 0)
		realOctaves = octaves;
	generatorCollection.reserve(size_t(octaves));
	for (size_t i = 0; i < size_t(octaves); ++i)
		generatorCollection.push_back(NoisePerlin(_rand));
}

inline double NoiseOctavesPerlin::GenerateOctaves(Vec2 _offset) {
	return GenerateOctaves(_offset.x, _offset.y);
}

inline double NoiseOctavesPerlin::GenerateOctaves(Vec3 _offset) {
	return GenerateOctaves(_offset.x, _offset.y, _offset.z);
}

inline double NoiseOctavesPerlin::GenerateOctaves(double _x, double _y) {
	double value = 0.0;
	const DetailPlan plan = PlanDetail();
	double scale = plan.startScale;
	for (int32_t i = plan.skip; i < plan.skip + plan.count; ++i) {
		value += generatorCollection[size_t(i)].GenerateNoise(Vec2{_x * scale, _y * scale}) / scale;
		scale /= 2.0;
	}
	return value;
}

inline double NoiseOctavesPerlin::GenerateOctaves(double _x, double _y, double _z) {
	double value = 0.0;
	const DetailPlan plan = PlanDetail();
	double scale = plan.startScale;
	for (int32_t i = plan.skip; i < plan.skip + plan.count; ++i) {
		value += generatorCollection[size_t(i)].GenerateNoise(Vec3{_x * scale, _y * scale, _z * scale}) / scale;
		scale /= 2.0;
	}
	return value;
}

inline void NoiseOctavesPerlin::GenerateOctaves(std::span<double> _noiseField, Vec3 _coordinate, Int32_3 _size,
                                                Vec3 _pScale) {
	const size_t count = size_t(_size.x) * size_t(_size.y) * size_t(_size.z);
	if (_noiseField.size() < count)
		return;

	std::fill_n(_noiseField.data(), count, 0.0);

	const DetailPlan plan = PlanDetail();
	if (plan.count <= 0)
		return;

	double multiplier = plan.startScale;
	for (int32_t octave = plan.skip; octave < plan.skip + plan.count; ++octave) {
		generatorCollection[size_t(octave)].GenerateNoise(_noiseField.first(count), _coordinate, _size,
		                                                  _pScale * multiplier, multiplier);
		multiplier /= 2.0;
	}
}

inline void NoiseOctavesPerlin::GenerateOctaves(std::span<double> _noiseField, Int32_2 _offset, Int32_2 _size,
                                                Vec2 _scale, [[maybe_unused]] double _unused) {
	this->GenerateOctaves(_noiseField, Vec3{double(_offset.x), 10.0, double(_offset.z)},
	                      Int32_3{_size.x, 1, _size.z}, Vec3{_scale.x, 1.0, _scale.z});
}

inline void NoiseOctavesPerlin::GenerateOctaves(std::vector<double> &_noiseField, double _coordX, double _coordY,
                                                double _coordZ, int32_t _sizeX, int32_t _sizeY, int32_t _sizeZ,
                                                double _scaleX, double _scaleY, double _scaleZ) {
	const size_t needed = size_t(_sizeX) * size_t(_sizeY) * size_t(_sizeZ);
	_noiseField.assign(needed, 0.0);
	GenerateOctaves(std::span<double>(_noiseField), Vec3{_coordX, _coordY, _coordZ}, Int32_3{_sizeX, _sizeY, _sizeZ},
	                Vec3{_scaleX, _scaleY, _scaleZ});
}

inline void NoiseOctavesPerlin::GenerateOctaves(std::vector<double> &_noiseField, int32_t _x, int32_t _z, int32_t _sizeX,
                                                int32_t _sizeZ, double _scaleX, double _scaleZ,
                                                [[maybe_unused]] double _unused) {
	GenerateOctaves(_noiseField, double(_x), 10.0, double(_z), _sizeX, 1, _sizeZ, _scaleX, 1.0, _scaleZ);
}
