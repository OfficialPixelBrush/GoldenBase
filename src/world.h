#pragma once

#include <cstdint>
class World {
    public:
        World(int64_t seed);
        ~World();
    
    private:
        int64_t seed;
};