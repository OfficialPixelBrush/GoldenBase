# GoldenBase
A ChunkBase-like website for pre-Release Minecraft terrain generation

# Dependencies
Follow the download and install instructions from the Emscripten page, [found here](https://emscripten.org/docs/getting_started/downloads.html).

You'll also need `Ninja` and `cmake`.

# Building

Just run the `build.sh` file, then use something like `python3 -m http.server` to host the files.

# Supported features
- Highly accurate terrain generation
- Version selection
- Biome visualization

## Supported versions/ranges

- Alpha v1.2.0 - Beta 1.7.3 ([Halloween Update terrain generator](https://minecraft.wiki/w/World_generation/History#Halloween_Update_terrain_generator))
- Infdev 20100327 (first version with the Farlands)
- Infdev 20100227-1433 (brick pyramids + obsidian wall)

## Planned generators

- A few more of the Infdev and Alpha generators
- Beta 1.8 and Beta 1.9 Generators
- Early Release Generators