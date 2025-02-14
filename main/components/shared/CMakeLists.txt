set(COMPONENT_ADD_INCLUDEDIRS 
	"${PROJECT_DIR}/libraries/ota-for-aws-iot-embedded-sdk/ota-for-aws-iot-embedded-sdk/source/include/"
	"${PROJECT_DIR}/libraries/ota-for-aws-iot-embedded-sdk/config/"
	"${PROJECT_DIR}/libraries/ota-for-aws-iot-embedded-sdk/port/"
	"${PROJECT_DIR}/libraries/ota-for-aws-iot-embedded-sdk/ota-for-aws-iot-embedded-sdk/source/portable/os/"
	"include"
	"${IDF_PATH}/components/lwip/lwip/src/include"
)

set(COMPONENT_SRCDIRS 
	src
)

set(COMPONENT_REQUIRES color_print)

list(APPEND COMPONENT_REQUIRES
		# button
		cJSON
		# # pca9633
		# mcp23017
		# # ttn-esp32
		# lora
		# KXTJ3_accel
		# esp32-owb
		# esp32-ds18b20
		mqtt
		# esp_modem
		coreMQTT
		coreMQTT-Agent
		lwip
		# esp_adc
		# esp_timer
		esp-tls
		soc 
		nvs_flash
		ulp
		hal
		heap
		driver
		corePKCS11
		bootloader_support
		protocol_common_conection_functions
		sdmmc
		fatfs
)

idf_component_register(
	SRCS "${COMPONENT_SRCDIRS}"
	INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
	REQUIRES
		"${COMPONENT_REQUIRES}"
)