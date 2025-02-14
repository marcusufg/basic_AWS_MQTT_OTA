@REM idf.py -p %1 flash monitor
build &&^
echo.  &&^
echo.If there is some flash error, please confirm builded addresses of *.bin files and following command: &&^
echo.  &&^
echo.python.exe esp-idf\components\esptool_py\esptool\esptool.py -p COMx -b 460800 --after hard_reset --chip %1%  write_flash --flash_mode dio --flash_size 4MB --flash_freq 40m 0x1000 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x16000 build\ota_data_initial.bin 0x60000 build\basic_OTA.bin &&^
echo.  &&^
echo.  &&^
if %1 == esp32 (
    python.exe esp-idf\components\esptool_py\esptool\esptool.py -p %2 -b 460800 --after hard_reset --chip esp32  write_flash --flash_mode dio --flash_size 4MB --flash_freq 40m 0x1000 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x16000 build\ota_data_initial.bin 0x60000 build\basic_OTA.bin && idf.py -p %2 monitor
) else if %1 == esp32s2 (
    python.exe esp-idf\components\esptool_py\esptool\esptool.py -p %2 -b 460800 --after hard_reset --chip esp32s2  write_flash --flash_mode dio --flash_size 4MB --flash_freq 40m 0x1000 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x16000 build\ota_data_initial.bin 0x60000 build\basic_OTA.bin && idf.py -p %2 monitor
) else if %1 == esp32s3 (
    python.exe esp-idf\components\esptool_py\esptool\esptool.py -p %2 -b 460800 --after hard_reset --chip %1  write_flash --flash_mode dio --flash_size 8MB --flash_freq 40m 0x0 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x16000 build\ota_data_initial.bin 0x60000 build\basic_OTA.bin && idf.py -p %2 monitor
) else (
    echo "please inform target: esp32, esp32s2 or esp32s3"
)