#include "helper.h"

// Check if the passed value is between a and b
bool Between(int32_t value, int32_t a, int32_t b) {
	if (a < b) {
		if (value > a && value < b) {
			return true;
		}
	} else {
		if (value > b && value < a) {
			return true;
		}
	}
	return false;
}

// Get the Euclidian distance between two Vec3s
double GetEuclidianDistance(Vec3 a, Vec3 b) {
	double x = (b.x-a.x)*(b.x-a.x);
	double y = (b.y-a.y)*(b.y-a.y);
	double z = (b.z-a.z)*(b.z-a.z);
	return abs(std::sqrt(x+y+z));
}

// Get the Euclidian distance between two Int3s
double GetEuclidianDistance(Int3 a, Int3 b) {
	int32_t x = (b.x-a.x)*(b.x-a.x);
	int32_t y = (b.y-a.y)*(b.y-a.y);
	int32_t z = (b.z-a.z)*(b.z-a.z);
	return abs(std::sqrt(double(x+y+z)));
}

// Get the Taxicab distance between two Vec3s
double GetTaxicabDistance(Vec3 a, Vec3 b) {
	double x = abs(a.x-b.x);
	double y = abs(a.y-b.y);
	double z = abs(a.z-b.z);
	return x+y+z;
}

// Get the Taxicab distance between two Int3s
double GetTaxicabDistance(Int3 a, Int3 b) {
	int32_t x = abs(a.x-b.x);
	int32_t y = abs(a.y-b.y);
	int32_t z = abs(a.z-b.z);
	return double(x+y+z);
}

// Get the Chebyshev distance between two Vec3s
double GetChebyshevDistance(Vec3 a, Vec3 b) {
	double x = abs(a.x-b.x);
	double y = abs(a.y-b.y);
	double z = abs(a.z-b.z);
	return std::max(std::max(x,y),z);
}

// Get the Chebyshev distance between two Int3s
double GetChebyshevDistance(Int3 a, Int3 b) {
	int32_t x = abs(a.x-b.x);
	int32_t y = abs(a.y-b.y);
	int32_t z = abs(a.z-b.z);
	return std::max(std::max(x,y),z);
}

// Turn an Int3 into a string
std::string GetInt3(Int3 position) {
	return "( " + std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + ")";
}
// Turn an Vec3 into a string
std::string GetVec3(Vec3 position) {
	return "( " + std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + ")";
}

// Determine the global position based on where within the passed chunk position the block position is
Int3 LocalToGlobalPosition(Int3 chunkPos, Int3 blockPos) {
	return Int3 {
        chunkPos.x*CHUNK_WIDTH_X + blockPos.x,
        blockPos.y,
        chunkPos.z*CHUNK_WIDTH_Z + blockPos.z
    };
}

// Get the Chunk Position of an Int3 coordinate
Int3 BlockToChunkPosition(Int3 position) {
	position.x = position.x >> 4;
	position.y = 0;
	position.z = position.z >> 4;
	return position;
}

// Get the Chunk Position of a Vec3 coordinate
Int3 BlockToChunkPosition(Vec3 position) {
	Int3 intPos = Vec3ToInt3(position);
	return BlockToChunkPosition(intPos);
}

Int3 BlockIndexToPosition(int32_t index) {
    Int3 pos;
    pos.y = index % CHUNK_HEIGHT;
    index /= CHUNK_HEIGHT;

    pos.z = index % CHUNK_WIDTH_Z;
    index /= CHUNK_WIDTH_Z;

    pos.x = index;
    return pos;
}

int32_t PositionToBlockIndex(Int3 pos) {
    return pos.y + pos.z * CHUNK_HEIGHT + pos.x * (CHUNK_HEIGHT * CHUNK_WIDTH_Z);
}

// Turn a float value into a byte, mapping the range 0-255 to 0°-360°
int8_t ConvertFloatToPackedByte(float value) {
	return static_cast<int8_t>((value/360.0f)*255.0f);
}

// Get the Block position from the Index
Int3 GetBlockPosition(int32_t index) {
    Int3 position;

    position.x = index / (CHUNK_HEIGHT * CHUNK_WIDTH_Z);  // Get x-coordinate
    index %= (CHUNK_HEIGHT * CHUNK_WIDTH_Z);               // Remainder after dividing by width

    position.z = index / CHUNK_HEIGHT;                     // Get z-coordinate
    position.y = index % CHUNK_HEIGHT;                     // Get y-coordinate

    return position;
}

// Get the Chunk Hash of a Chunk
uint64_t GetChunkHash(Int2 position) {
    return
        (uint64_t(std::bit_cast<uint32_t>(position.x)) << 32) |
        (uint64_t(std::bit_cast<uint32_t>(position.y)) & 0xFFFFFFFF)
    ;
}

// Turn the chunk hash into X and Z coordinates
Int2 DecodeChunkHash(int64_t hash) {
    return Int2 {
        int32_t(hash >> 32),
        int32_t(hash & 0xFFFFFFFF)
    };
}

void LimitBlockCoordinates(Int3 &position) {
    position.y = std::max(std::min(position.y,127),0);
}