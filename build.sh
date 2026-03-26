mkdir -p build
cd build
emcmake cmake .. -G Ninja
cmake --build . --config Debug
cd ..
cp build/GoldenBase.wasm ./
cp build/GoldenBase.js ./