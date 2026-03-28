mkdir -p build
cd build
emcmake cmake .. -G Ninja
cmake --build . --config Debug
cd ..
cp build/GoldenBase.wasm ./dist
cp build/GoldenBase.js ./dist