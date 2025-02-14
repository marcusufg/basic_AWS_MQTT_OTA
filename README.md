# basic_AWS_MQTT_OTA
Simple code to run in ESP32 and request MQTT OTA updates from AWS

Author: Marcus A. P. Oliveira, 2025


After cloning this folder, follow these steps:
-   git clone https://github.com/espressif/esp-idf --recurse-submodules
-   . ./esp-idf/install.sh
-   . ./esp-idf/export.sh
-   . ./rebuild.sh esp32
Copy certificate files to /certs folder, using these names:
-   root_cert_auth.pem
-   aws_codesign.crt
-   client.crt
-   client.key
Follow steps 1 and 2 in https://docs.aws.amazon.com/freertos/latest/userguide/ota-updates-esp32-ble.html
From now, every modification in code, just run:
-   . ./build.sh 
Or to compile and run on microcontroller: (adjust according to your COM port)
-   . ./flash.sh esp32 /dev/cu.SLAB_USBtoUART
To create your own OTA job from CLI:
-   . ./ota.sh DEVICE_NAME