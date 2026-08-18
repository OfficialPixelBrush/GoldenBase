#pragma once

#include "datatypes.h"

// Beta 1.7.3 ItemMap / MapColor palette (material map color of the surface block).
Int3 GetBlockMapColor(int block_id);
bool IsMapColorAir(int block_id);
bool IsMapWaterColor(int block_id);
