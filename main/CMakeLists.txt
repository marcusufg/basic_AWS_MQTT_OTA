set(COMPONENT_SRCDIRS 
	"."
	"components/shared/src/"
	# "../components/ANY_PRIVATE_COMPONENT/src/"
	"${PROJECT_DIR}/libraries/corePKCS11/corePKCS11/source/dependency/3rdparty/mbedtls_utils"
)

set(COMPONENT_ADD_INCLUDEDIRS
	"${PROJECT_DIR}/libraries/ota-for-aws-iot-embedded-sdk/ota-for-aws-iot-embedded-sdk/source/include/"
	"${PROJECT_DIR}/libraries/ota-for-aws-iot-embedded-sdk/config/"
	"${PROJECT_DIR}/libraries/ota-for-aws-iot-embedded-sdk/port/"
	"${PROJECT_DIR}/libraries/ota-for-aws-iot-embedded-sdk/ota-for-aws-iot-embedded-sdk/source/portable/os/"
	"${PROJECT_DIR}/libraries/common/posix_compat/"
	"${PROJECT_DIR}/libraries/corePKCS11/corePKCS11/source/include"
	"${PROJECT_DIR}/libraries/corePKCS11/corePKCS11/source/dependency/3rdparty/mbedtls_utils"
	"${PROJECT_DIR}/libraries/coreMQTT/coreMQTT//source/include"
	# "${PROJECT_DIR}/libraries/coreMQTT-Agent/coreMQTT-Agent//source/include"
)

idf_component_register(
					SRC_DIRS "${COMPONENT_SRCDIRS}"
					INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
					REQUIRES 
						# sdmmc
						# fatfs
						color_print
						# button
						cJSON
						# app_update
						# # ttn-esp32
						# lora
						# KXTJ3_accel
						# esp32-owb
						# esp32-ds18b20
						# esp-tls
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
						esp_adc
						esp_timer
						esp_rom
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

target_add_binary_data(${COMPONENT_TARGET} "${PROJECT_DIR}/certs/root_cert_auth.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "${PROJECT_DIR}/certs/client.crt" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "${PROJECT_DIR}/certs/client.key" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "${PROJECT_DIR}/certs/aws_codesign.crt" TEXT)