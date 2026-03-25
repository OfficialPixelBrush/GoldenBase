mkdir -p build
cd build
emcmake cmake .. -G Ninja
cmake --build . #--config Release
cd ..