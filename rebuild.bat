@echo off

if exist build rd /s /q build

if %1 == esp32 (
    idf.py set-target %1
    cmake -S . -B build -DVENDOR=espressif -DIDF_TARGET=esp32 -DBOARD=esp32_devkitc -DCMAKE_BUILD_TYPE=Debug -DAFR_ESP_LWIP=1 -DCOMPILER=xtensa-esp32 -GNinja -DPYTHON_DEPS_CHECKED=1 -DESP_PLATFORM=1 -DCOMPONENT_CACHE_ENABLED=ON && idf.py build
) else if %1 == esp32s2 (
    idf.py set-target %1
    cmake -S . -B build -DVENDOR=espressif -DIDF_TARGET=esp32s2 -DBOARD=esp32s2_saola_1 -DCOMPILER=xtensa-esp32s2 -DAFR_ESP_LWIP=1 -DCOMPILER=xtensa-esp32s2 -GNinja -DPYTHON_DEPS_CHECKED=1 -DESP_PLATFORM=1 -DCOMPONENT_CACHE_ENABLED=ON && idf.py build
) else if %1 == esp32s3 (
    idf.py set-target %1
    cmake -S . -B build -DVENDOR=espressif -DIDF_TARGET=esp32s3 -DBOARD=esp32s3_saola_1 -DCOMPILER=xtensa-esp32s3 -DAFR_ESP_LWIP=1 -DCOMPILER=xtensa-esp32s3 -GNinja -DPYTHON_DEPS_CHECKED=1 -DESP_PLATFORM=1 -DCOMPONENT_CACHE_ENABLED=ON && idf.py build
) else (
    echo "please inform target: esp32, esp32s2 or esp32s3"
)