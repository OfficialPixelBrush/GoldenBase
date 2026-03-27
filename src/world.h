#pragma once

#include <cstdint>
class World {
    public:
        int64_t seed;
        World(int64_t seed);
        ~World();
};