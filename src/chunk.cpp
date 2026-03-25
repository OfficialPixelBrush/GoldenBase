#include "chunk.h"
#include <iostream>

Biome Chunk::GetBiome(int32_t x, int32_t z) {
	return biomeArray[(z & 15) << 4 | (x & 15)];
}

Biome Chunk::SetBiome(Biome biome, int32_t x, int32_t z) {
	biomeArray[(z & 15) << 4 | (x & 15)] = biome;
	return biome;
}

void Chunk::SetBiomes(std::vector<Biome> biomes) {
	for (size_t i = 0; i < biomes.size(); ++i) {
		biomeArray[i] = biomes[i];
	}
}

int8_t Chunk::GetHeightValue(uint8_t x, uint8_t z) { return this->heightMap[(z & 15) << 4 | (x & 15)]; }

void Chunk::GenerateHeightMap() {
	int32_t lowestBlock = CHUNK_HEIGHT - 1;
	int32_t x, z;
	for (x = 0; x < CHUNK_WIDTH_X; ++x) {
		for (z = 0; z < CHUNK_WIDTH_Z; ++z) {
			int32_t y = CHUNK_HEIGHT - 1;

			for (y = CHUNK_HEIGHT - 1; y > 0; --y) {
				if (GetOpacity(int16_t(GetBlockType(Int3{x, y-1, z}))) != 0)
					break;
			}

			this->heightMap[z << 4 | x] = int8_t(y);
			if (y < lowestBlock) {
				lowestBlock = y;
			}

			// Check if the world has no sky (nether)
			/*
			int8_t light = 15;
			for (y = CHUNK_HEIGHT - 1; y > 0; --y) {
				light -= GetOpacity(int16_t(GetBlockType(Int3{x, y, z})));
				if (light <= 0)
					break;
				SetSkyLight(light, Int3{x, y, z});
			}*/
		}
	}
	this->lowestBlockHeight = lowestBlock;

	/*
	for (x = 0; x < 16; ++x) {
		for (z = 0; z < 16; ++z) {
			this->UpdateSkylight_do(x, z);
		}
	}
	*/
	this->modified = true;
}

void Chunk::PrintHeightmap() {
	std::cout << std::hex;
	for (int32_t x = 0; x < 16; ++x) {
		std::cout << "[";
		for (int32_t z = 0; z < 16; ++z) {
			std::cout << "0x" << int32_t(this->heightMap[(z & 15) << 4 | (x & 15)]);
			if (z < 15)
				std::cout << ",";
		}
		std::cout << "]," << "\n";
	}
	std::cout << std::dec;
}

void Chunk::ClearChunk() {
	std::fill(std::begin(blockTypeArray), std::end(blockTypeArray), BLOCK_AIR);
	std::fill(std::begin(blockMetaArray), std::end(blockMetaArray), 0);
	std::fill(std::begin(blockLightArray), std::end(blockLightArray), 0);
	std::fill(std::begin(heightMap), std::end(heightMap), 0);
}

void Chunk::RelightBlock(int32_t x, int32_t y, int32_t z) {
	int32_t oldY = this->heightMap[(z << 4) | x] & 255;
	int32_t newY = oldY;
	if (y > oldY)
		newY = y;

	// match Infdev logic: walk downward until a solid block
	while (newY > 0 && GetOpacity(GetBlockType({x, newY - 1, z})) == 0)
		--newY;

	if (newY != oldY) {
		// visual update
		// this->world->MarkBlocksDirtyVertical(x, z, newY, oldY);

		this->heightMap[(z << 4) | x] = int8_t(newY);

		if (newY < this->lowestBlockHeight) {
			this->lowestBlockHeight = newY;
		} else {
			int32_t m = CHUNK_HEIGHT - 1;
			for (int32_t ix = 0; ix < 16; ++ix) {
				for (int32_t iz = 0; iz < 16; ++iz) {
					int32_t h = this->heightMap[(iz << 4) | ix] & 255;
					if (h < m)
						m = h;
				}
			}
			this->lowestBlockHeight = m;
		}

		int32_t wx = (this->xPos << 4) + x;
		int32_t wz = (this->zPos << 4) + z;

		// height lowered
		if (newY < oldY) {
			for (int32_t yy = newY; yy < oldY; ++yy)
				this->SetSkyLight(15, {x, yy, z});
		}
		// height raised
		else {
			//this->world->ScheduleLightingUpdate(true, {wx, oldY, wz}, {wx, newY, wz});
			for (int32_t yy = oldY; yy < newY; ++yy)
				this->SetSkyLight(0, {x, yy, z});
		}

		int32_t light = 15;
		int32_t stopY = newY;
		int32_t yy = newY;

		// propagate downward
		while (yy > 0 && light > 0) {
			--yy;
			int32_t op = GetOpacity(GetBlockType({x, yy, z}));
			if (op == 0)
				op = 1;
			light -= op;
			if (light < 0)
				light = 0;
			this->SetSkyLight(light, {x, yy, z});
		}

		// drop until solid
		while (yy > 0 && GetOpacity(GetBlockType({x, yy - 1, z})) == 0)
			--yy;

		if (yy != stopY) {
			//this->world->ScheduleLightingUpdate(true, {wx - 1, yy, wz - 1}, {wx + 1, stopY, wz + 1});
		}

		this->modified = true;
	}
}

void Chunk::UpdateSkylight_do(int32_t x, int32_t z) {
	int32_t height = this->GetHeightValue(x, z);
	x += this->xPos << 4;
	z += this->zPos << 4;
	this->CheckSkylightNeighborHeight(x - 1, z, height);
	this->CheckSkylightNeighborHeight(x + 1, z, height);
	this->CheckSkylightNeighborHeight(x, z - 1, height);
	this->CheckSkylightNeighborHeight(x, z + 1, height);
}

void Chunk::CheckSkylightNeighborHeight(int32_t x, int32_t z, int32_t height) {
	/*
	int32_t worldHeight = this->world->GetHeightValue(Int2{x, z});
	if (worldHeight > height) {
		this->world->ScheduleLightingUpdate(true, Int3{x, height, z}, Int3{x, worldHeight, z});
	} else if (worldHeight < height) {
		this->world->ScheduleLightingUpdate(true, Int3{x, worldHeight, z}, Int3{x, height, z});
	}
	*/

	this->modified = true;
}

/*
Block* Chunk::GetBlock(Int3 pos) {
	if (pos.x < 0 || pos.y < 0 || pos.z < 0 ||
		pos.x >= CHUNK_WIDTH_X || pos.y >= CHUNK_HEIGHT || pos.z >= CHUNK_WIDTH_Z)
		return nullptr;
	return &blocks[PositionToBlockIndex(pos)];
}
Block* Chunk::GetBlock(int32_t x, int8_t y, int32_t z) {
	return GetBlock(Int3{x,y,z});
}
*/

bool Chunk::CanBlockSeeTheSky(Int3 pos) {
	if (pos.x < 0 || pos.x >= 16 || pos.z < 0 || pos.z >= 16) {
		// Out of bounds
		return false;
	}
	// if (!heightMap) return false;
	return pos.y >= (this->heightMap[pos.z << 4 | pos.x] & 255);
}

int8_t Chunk::GetLight(bool skyLight, Int3 pos) {
	if (skyLight)
		return GetSkyLight(pos);
	return GetBlockLight(pos);
}

int8_t Chunk::GetTotalLight(Int3 pos) {
	int8_t totalLight = GetLight(true, pos);
	int8_t blockLight = GetLight(false, pos);
	if (blockLight > totalLight) {
		totalLight = blockLight;
	}

	return totalLight;
}

void Chunk::SetLight(bool skyLight, Int3 pos, int8_t newLight) {
	if (skyLight) {
		SetSkyLight(newLight, pos);
		return;
	}
	SetBlockLight(newLight, pos);
	this->modified = true;
}

BlockType Chunk::GetBlockType(Int3 pos) {
	if (!InChunkBounds(pos))
		return BLOCK_AIR;
	return blockTypeArray[PositionToBlockIndex(pos)];
}

void Chunk::SetBlockType(BlockType type, Int3 pos) {
	if (!InChunkBounds(pos))
		return;
	blockTypeArray[PositionToBlockIndex(pos)] = type;
	RelightBlock(pos.x, pos.y, pos.z);
	this->modified = true;
}

int8_t Chunk::GetBlockMeta(Int3 pos) {
	if (!InChunkBounds(pos))
		return 0;
	const int32_t index = PositionToBlockIndex(pos) / 2;
	uint8_t value = blockMetaArray[index];

	if ((pos.y & 1) == 0) {
		return (value >> 4) & 0x0F;
	} else {
		return value & 0x0F;
	}
}

void Chunk::SetBlockMeta(int8_t meta, Int3 pos) {
	if (!InChunkBounds(pos))
		return;
	const int32_t index = PositionToBlockIndex(pos) / 2;
	meta &= 0x0F;
	if ((pos.y & 1) == 0) {
		blockMetaArray[index] &= 0x0F;
		blockMetaArray[index] |= (meta << 4);
	} else {
		blockMetaArray[index] &= 0xF0;
		blockMetaArray[index] |= meta;
	}
	modified = true;
}

int8_t Chunk::GetBlockLight(Int3 pos) {
	if (!InChunkBounds(pos))
		return 0;
	return (blockLightArray[PositionToBlockIndex(pos)] >> 4) & 0xF;
}

void Chunk::SetBlockLight(int8_t value, Int3 pos) {
	if (!InChunkBounds(pos))
		return;
	int32_t index = PositionToBlockIndex(pos);
	blockLightArray[index] &= 0x0F;
	blockLightArray[index] |= ((value & 0xF) << 4);
	this->modified = true;
}

int8_t Chunk::GetSkyLight(Int3 pos) {
	if (!InChunkBounds(pos))
		return 0;
	return blockLightArray[PositionToBlockIndex(pos)] & 0x0F;
}

void Chunk::SetSkyLight(int8_t value, Int3 pos) {
	if (!InChunkBounds(pos))
		return;
	int32_t index = PositionToBlockIndex(pos);
	blockLightArray[index] &= 0xF0;
	blockLightArray[index] |= (value & 0xF);
	this->modified = true;
}

void Chunk::SetBlockTypeAndMeta(BlockType type, int8_t meta, Int3 pos) {
	SetBlockType(type, pos);
	SetBlockMeta(meta, pos);
	this->modified = true;
}

bool Chunk::InChunkBounds(Int3 &pos) {
	pos.x &= CHUNK_WIDTH_X - 1;
	pos.z &= CHUNK_WIDTH_Z - 1;
	if (pos.y < 0 || pos.y >= CHUNK_HEIGHT)
		return false;
	return true;
}

// Get all the Block Data of a Chunk as an array
std::array<uint8_t, CHUNK_WIDTH_X * CHUNK_HEIGHT * CHUNK_WIDTH_Z> Chunk::GetBlockTypes() {
	std::array<uint8_t, CHUNK_WIDTH_X * CHUNK_HEIGHT * CHUNK_WIDTH_Z> data;
	int32_t index = 0;
	// BlockData
	for (int16_t cX = 0; cX < CHUNK_WIDTH_X; cX++) {
		for (int16_t cZ = 0; cZ < CHUNK_WIDTH_Z; cZ++) {
			for (uint16_t cY = 0; cY < CHUNK_HEIGHT; cY++) {
				data[index++] = uint8_t(GetBlockType(Int3{cX, cY, cZ}));
			}
		}
	}
	return data;
}

// Get all the Meta Data of a Chunk as an array
std::array<uint8_t, CHUNK_WIDTH_X *(CHUNK_HEIGHT / 2) * CHUNK_WIDTH_Z> Chunk::GetBlockMetas() {
	std::array<uint8_t, CHUNK_WIDTH_X *(CHUNK_HEIGHT / 2) * CHUNK_WIDTH_Z> data;
	int32_t index = 0;
	// Block Metadata
	for (int16_t cX = 0; cX < CHUNK_WIDTH_X; cX++) {
		for (int16_t cZ = 0; cZ < CHUNK_WIDTH_Z; cZ++) {
			for (uint16_t cY = 0; cY < (CHUNK_HEIGHT / 2); cY++) {
				// Default to safe values
				uint8_t b1v = GetBlockMeta(Int3{cX, cY * 2, cZ});
				uint8_t b2v = GetBlockMeta(Int3{cX, cY * 2 + 1, cZ});
				data[index++] = int8_t(b2v << 4 | b1v);
			}
		}
	}
	return data;
}

// Get all the Block Light Data of a Chunk as an array
std::array<uint8_t, CHUNK_WIDTH_X *(CHUNK_HEIGHT / 2) * CHUNK_WIDTH_Z> Chunk::GetBlockLights() {
	std::array<uint8_t, CHUNK_WIDTH_X *(CHUNK_HEIGHT / 2) * CHUNK_WIDTH_Z> data;
	int32_t index = 0;
	// Block Light
	for (int16_t cX = 0; cX < CHUNK_WIDTH_X; cX++) {
		for (int16_t cZ = 0; cZ < CHUNK_WIDTH_Z; cZ++) {
			for (uint16_t cY = 0; cY < (CHUNK_HEIGHT / 2); cY++) {
				// Default to safe values
				uint8_t b1v = GetBlockLight(Int3{cX, cY * 2, cZ});
				uint8_t b2v = GetBlockLight(Int3{cX, cY * 2 + 1, cZ});
				data[index++] = int8_t(b2v << 4 | b1v);
			}
		}
	}
	return data;
}

// Get all the Sky Light Data of a Chunk as an array
std::array<uint8_t, CHUNK_WIDTH_X *(CHUNK_HEIGHT / 2) * CHUNK_WIDTH_Z> Chunk::GetSkyLights() {
	std::array<uint8_t, CHUNK_WIDTH_X *(CHUNK_HEIGHT / 2) * CHUNK_WIDTH_Z> data;
	int32_t index = 0;

	// Sky Light
	for (int16_t cX = 0; cX < CHUNK_WIDTH_X; cX++) {
		for (int16_t cZ = 0; cZ < CHUNK_WIDTH_Z; cZ++) {
			for (uint16_t cY = 0; cY < (CHUNK_HEIGHT / 2); cY++) {
				// Default to safe values
				uint8_t b1v = GetSkyLight(Int3{cX, cY * 2, cZ});
				uint8_t b2v = GetSkyLight(Int3{cX, cY * 2 + 1, cZ});
				data[index++] = int8_t(b2v << 4 | b1v);
			}
		}
	}
	return data;
}