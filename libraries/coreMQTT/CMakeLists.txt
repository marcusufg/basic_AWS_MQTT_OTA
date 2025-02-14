# This gives MQTT_INCLUDE_PUBLIC_DIRS, MQTT_SOURCES, and MQTT_SERIALIZER_SOURCES
include(${CMAKE_CURRENT_LIST_DIR}/coreMQTT/mqttFilePaths.cmake)

set(COREMQTT_PORT_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/port/network_transport
)

set(COREMQTT_INCLUDE_DIRS
    ${MQTT_INCLUDE_PUBLIC_DIRS}
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${CMAKE_CURRENT_LIST_DIR}/../common/logging/
    ${COREMQTT_PORT_INCLUDE_DIRS}
)

set(COREMQTT_PORT_SRCS
    ${CMAKE_CURRENT_LIST_DIR}/port/network_transport/network_transport.c
)

set(COREMQTT_SRCS
    ${MQTT_SOURCES}
    ${MQTT_SERIALIZER_SOURCES}
    ${COREMQTT_PORT_SRCS}
)

set(COREMQTT_REQUIRES
    esp-tls
    sdmmc
						fatfs
						color_print
						# button
						cJSON
						# # ttn-esp32
						# lora
						# KXTJ3_accel
						# esp32-owb
						# esp32-ds18b20
						mqtt
						coreMQTT
						coreMQTT-Agent
						# esp_modem
						lwip
						corePKCS11
						nvs_flash 
						spi_flash 
						esp_partition 
						pthread 
						esp_netif 
						esp_wifi
						# esp_adc
						# esp_timer
						# esp_rom
						# esp_http_client
						soc
						ulp
						hal
						heap
						driver
						esp_event
						wifi_provisioning
						mbedtls
						espressif__mdns
						mdns						
						bootloader_support 
						protocol_common_conection_functions
)

idf_component_register(
    SRCS
        ${COREMQTT_SRCS}
    INCLUDE_DIRS
        ${COREMQTT_INCLUDE_DIRS}
    REQUIRES
        ${COREMQTT_REQUIRES}
)

set_source_files_properties(
"${CMAKE_CURRENT_LIST_DIR}/coreMQTT/source/core_mqtt.c"
PROPERTIES COMPILE_FLAGS -Wno-format)