# clear
idf.py build &&
if [[ $1 == esp32 ]]; then
    python esp-idf/components/esptool_py/esptool/esptool.py -p $2 -b 460800 --after hard_reset --chip esp32  write_flash --flash_mode dio --flash_size 4MB --flash_freq 40m 0x1000 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x16000 build/ota_data_initial.bin 0x60000 build/basic_OTA.bin && idf.py -p $2 monitor
fi 

if [[ $1 == esp32s2 ]]; then
    python esp-idf/components/esptool_py/esptool/esptool.py -p $2 -b 460800 --after hard_reset --chip esp32s2  write_flash --flash_mode dio --flash_size 4MB --flash_freq 40m 0x1000 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x16000 build/ota_data_initial.bin 0x60000 build/basic_OTA.bin && idf.py -p $2 monitor
fi

if [[ $1 == esp32s3 ]]; then
    python esp-idf/components/esptool_py/esptool/esptool.py -p $2 -b 460800 --after hard_reset --chip $1  write_flash --flash_mode dio --flash_size 8MB --flash_freq 40m 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x16000 build/ota_data_initial.bin 0x60000 build/basic_OTA.bin && idf.py -p $2 monitor
fi

# nothing?
if [[ $1 == "" ]]; then
  echo "please make a choise: esp32, esp32s2 or esp32s3"
fi