mkdir -p build
cd build
emcmake cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .
cd ..
cp build/GoldenBase.wasm ./dist
cp build/GoldenBase.js ./dist