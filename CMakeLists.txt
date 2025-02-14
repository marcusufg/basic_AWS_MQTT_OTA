cmake_minimum_required(VERSION 3.16)

# to remove warnings
add_definitions(-w)

set(EXTRA_COMPONENT_DIRS 
		# "${CMAKE_CURRENT_LIST_DIR}/components/protocol_common_conection_functions"
		"${CMAKE_CURRENT_LIST_DIR}/libraries/ota-for-aws-iot-embedded-sdk"
		"${CMAKE_CURRENT_LIST_DIR}/libraries/coreMQTT"
		"${CMAKE_CURRENT_LIST_DIR}/libraries/coreMQTT-Agent"
		"${CMAKE_CURRENT_LIST_DIR}/libraries/corePKCS11"
		"${CMAKE_CURRENT_LIST_DIR}/libraries/coreJSON"
		"${CMAKE_CURRENT_LIST_DIR}/libraries/backoffAlgorithm"
		"${CMAKE_CURRENT_LIST_DIR}/libraries/common/posix_compat"
		"${CMAKE_CURRENT_LIST_DIR}/main/components/shared"
)


if (CONFIG_IDF_TARGET_ESP32S2)
	set(SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s2")
elseif (CONFIG_IDF_TARGET_ESP32S3)
	set(SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3")
elseif (CONFIG_IDF_TARGET_ESP32)
	set(SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32")
endif()


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(basic_OTA)