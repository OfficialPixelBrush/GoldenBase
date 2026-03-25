#pragma once

#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>
#include <array>
#include <bit>

#include "blocks.h"
#include "items.h"
#include "datatypes.h"
#include "chunk.h"

Int3 LocalToGlobalPosition(Int3 chunkPos, Int3 blockPos);
Int3 BlockToChunkPosition(Vec3 position);
Int3 BlockToChunkPosition(Int3 position);

Int3 BlockIndexToPosition(int32_t index);
int32_t PositionToBlockIndex(Int3 pos);

int8_t ConvertFloatToPackedByte(float value);

bool Between(int32_t value, int32_t a, int32_t b);

double GetEuclidianDistance(Vec3 a, Vec3 b);
double GetEuclidianDistance(Int3 a, Int3 b);
double GetTaxicabDistance(Vec3 a, Vec3 b);
double GetTaxicabDistance(Int3 a, Int3 b);
double GetChebyshevDistance(Vec3 a, Vec3 b);
double GetChebyshevDistance(Int3 a, Int3 b);

// Handling of Chunk and Block Data
Int3 GetBlockPosition(int32_t index);

uint64_t GetChunkHash(Int2 position);
Int2 DecodeChunkHash(int64_t hash);

void LimitBlockCoordinates(Int3 &position);