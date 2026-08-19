#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include "blocks.h"
#include "items.h"
#include "labels.h"
#include "numeric_structs.h"

#define CHUNK_HEIGHT 128
#define CHUNK_WIDTH 16
#define WATER_LEVEL CHUNK_HEIGHT/2

#define CHUNK_DATA_SIZE static_cast<size_t>(CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH * 2.5)

#define OLD_CHUNK_FILE_EXTENSION ".cnk"
#define CHUNK_FILE_EXTENSION ".ncnk"
#define MCREGION_FILE_EXTENSION ".mcr"

// Item
struct Item {
    int16_t id = ITEM_INVALID;
    int8_t  amount = 0;
    int16_t damage = 0; // Also known as metadata

    friend std::ostream& operator<<(std::ostream& os, const Item& i) {
        os << "(" << IdToLabel(i.id) << ": " << int32_t(i.damage) << " x" << int32_t(i.amount) << ")";
        return os;
    }
    
    std::string str() const {
        std::ostringstream oss;
        oss << *this; // Use the overloaded << operator
        return oss.str();
    }
};

// Block Struct
struct Block {
    BlockType type = BLOCK_AIR;
    int8_t meta = 0;
    int8_t blocklight = 0;
    int8_t skylight = 0;

    friend std::ostream& operator<<(std::ostream& os, const Block& b) {
        os << "(" << int32_t(b.type) << ":" << int32_t(b.meta) << ")";
        return os;
    }
    
    std::string str() const {
        std::ostringstream oss;
        oss << *this; // Use the overloaded << operator
        return oss.str();
    }
};

/**
 * @brief Axis-aligned Bounding Box
 * 
 */
struct AABB {
    Vec3 min;
    Vec3 max;
};

Vec3 Int3ToVec3(Int3 i);
Int3 Vec3ToInt3(Vec3 v);

Int3 Int3ToEntityInt3(Int3 pos);
Int3 Vec3ToEntityInt3(Vec3 pos);
Vec3 EntityInt3ToVec3(Int3 pos);
AABB CalculateAABB(Vec3 position, AABB base);
