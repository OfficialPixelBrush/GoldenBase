cmake_minimum_required(VERSION 3.16)
project(GoldenBase)

# Use Emscripten compiler
set(CMAKE_CXX_COMPILER emcc)
set(CMAKE_CXX_STANDARD 23)

# Source files
set(SOURCES
    src/main.cpp

    src/generators/infdev/inf20100227/generatorInfdev20100227.cpp
    src/generators/infdev/inf20100327/generatorInfdev20100327.cpp
    src/generators/beta/b173/beta173Biome.cpp
    src/generators/beta/b173/beta173Caver.cpp
    #src/generators/beta/b173/beta173Feature.cpp
    #src/generators/beta/b173/beta173Tree.cpp
    src/generators/beta/b173/generatorBeta173.cpp
    src/generator.cpp
    src/java/noise/noisePerlin.cpp
    src/java/noise/noiseSimplex.cpp
    src/java/noise/noiseGenerator.cpp
    
    src/helpers/datatypes.cpp
    src/helpers/blockHelper.cpp
    src/helpers/biomeColors.cpp
    src/helpers/helper.cpp

    src/enums/biomes.cpp
    src/chunk.cpp
)

# Output JS/WASM
add_executable(GoldenBase ${SOURCES})

# Add the flags you had in your emcc command
target_link_options(GoldenBase PRIVATE
    "-O3"
    "-sDISABLE_EXCEPTION_CATCHING=0"
    "-sWASM=1"
    "-sEXPORTED_RUNTIME_METHODS=[cwrap,HEAPU8]"
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createModule"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sASYNCIFY=1"
    "--bind"
)

include_directories(
    "${PROJECT_SOURCE_DIR}/src/generators/beta/b173"
    "${PROJECT_SOURCE_DIR}/src/generators/infdev/inf20100227"
    "${PROJECT_SOURCE_DIR}/src/generators/infdev/inf20100327"
    "${PROJECT_SOURCE_DIR}/src/java"
    "${PROJECT_SOURCE_DIR}/src/java/noise"
    "${PROJECT_SOURCE_DIR}/src/helpers"
    "${PROJECT_SOURCE_DIR}/src/enums"
    "${PROJECT_SOURCE_DIR}/src/"
)