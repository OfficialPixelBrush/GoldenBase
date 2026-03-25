mkdir -p build
cd build
emcmake cmake .. -G Ninja
cmake --build . #--config Release
cd ..
cp build/GoldenBase.wasm ./
cp build/GoldenBase.js ./