clear 
DIR="build"
if [ -d "$DIR" ]; then
  rm -rf build  
fi
echo "Rebuilding all project and compiling..."

if [[ "$1" == "esp32" ]]; then
  idf.py set-target $1
  # cmake -S . -B build -DVENDOR=espressif -DBOARD=esp32_devkitc -DCMAKE_BUILD_TYPE=Debug -DAFR_ESP_LWIP=1 -DAFR_ENABLE_DEMOS=0 -DAFR_ENABLE_TESTS=0 -DCOMPILER=xtensa-esp32 -DCMAKE_TOOLCHAIN_FILE=amazon-freertos/tools/cmake/toolchains/xtensa-esp32.cmake && idf.py build
  cmake -S . -B build -DVENDOR=espressif -DBOARD=esp32_devkitc -DCMAKE_BUILD_TYPE=Debug -DAFR_ESP_LWIP=1 -DAFR_ENABLE_DEMOS=0 -DAFR_ENABLE_TESTS=0 -DCOMPILER=xtensa-esp32 -DCMAKE_TOOLCHAIN_FILE=amazon-freertos/tools/cmake/toolchains/xtensa-esp32.cmake -GNinja 
  python esp-idf/tools/idf.py build
  # cmake -S . -B build -DIDF_SDKCONFIG_DEFAULTS=../sdkconfig.defaults -DVENDOR=espressif -DBOARD=esp32_devkitc -DCMAKE_BUILD_TYPE=Debug -DAFR_ESP_LWIP=1 -DAFR_ENABLE_DEMOS=0 -DAFR_ENABLE_TESTS=0 -DCOMPILER=xtensa-esp32 -DCMAKE_TOOLCHAIN_FILE=amazon-freertos/tools/cmake/toolchains/xtensa-esp32.cmake -GNinja && idf.py build
fi

# for esp32s2:
if [[ "$1" == "esp32s2" ]]; then
  idf.py set-target $1
  cmake -S . -B build -DVENDOR=espressif -DBOARD=esp32s2_saola_1 -DCMAKE_BUILD_TYPE=Debug -DAFR_ESP_LWIP=1 -DAFR_ENABLE_DEMOS=0 -DAFR_ENABLE_TESTS=0 -DCOMPILER=xtensa-esp32s2 -DCMAKE_TOOLCHAIN_FILE=amazon-freertos/tools/cmake/toolchains/xtensa-esp32s2.cmake -GNinja 
  python esp-idf/tools/idf.py build
fi

# for esp32s3:
if [[ "$1" == "esp32s3" ]]; then
  idf.py set-target $1
  cmake -S . -B build -DVENDOR=espressif -DBOARD=esp32s3_saola_1 -DCMAKE_BUILD_TYPE=Debug -DAFR_ESP_LWIP=1 -DCOMPILER=xtensa-esp32s3 -DCMAKE_TOOLCHAIN_FILE=amazon-freertos/tools/cmake/toolchains/xtensa-esp32s3.cmake -GNinja 
  python esp-idf/tools/idf.py build
fi

# nothing?
if [[ "$1" == "" ]]; then
  echo "please make a choise: esp32, esp32s2 or esp32s3"
fi
