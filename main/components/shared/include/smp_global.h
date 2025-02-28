#include "smp_config.h"

/* ========== C INCLUDES ========== */
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
#include "math.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* ========== ESP-IDF INCLUDES ========== */
#include "esp_system.h"
#include "esp_event.h"
#include "protocol_common_conection_functions.h"
#include "esp_rom_efuse.h"
#include "esp_mac.h"  
#ifndef CONFIG_HEAP_TRACING
    #define CONFIG_HEAP_TRACING 1
    #include "esp_heap_trace.h"
#endif
#include "esp_err.h"
#include "esp_flash_partitions.h"
#include "esp_flash_encrypt.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "esp_image_format.h"
#include "esp_secure_boot.h"
#include "esp_private/esp_clk.h"
#if CONFIG_IDF_TARGET_ESP32
    #include "esp32/rom/ets_sys.h"    
#elif CONFIG_IDF_TARGET_ESP32S2
    #include "esp32s2/rom/efuse.h"
#elif CONFIG_IDF_TARGET_ESP32S3
    #include "esp32s3/rom/efuse.h"
#endif
#include "driver/adc.h"
#include "soc/adc_periph.h"
#include "hal/adc_types.h"
#include "hal/adc_hal_common.h"
#include "hal/i2c_hal.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"
#include "driver/i2s_common.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "soc/i2c_periph.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_periph.h"
#include "soc/io_mux_reg.h"
#include "soc/timer_periph.h"
#include "soc/sens_struct.h"
#include "esp_heap_task_info.h"

#if CONFIG_IDF_TARGET_ESP32
    #include "esp32/rom/ets_sys.h"    
#elif CONFIG_IDF_TARGET_ESP32S2
    #include "esp32s2/rom/ets_sys.h"
#elif CONFIG_IDF_TARGET_ESP32S3
    #include "esp32s3/rom/ets_sys.h"
#endif


#include "clock.h"
#include "sdkconfig.h"


#include "core_mqtt.h"
#include "transport_interface.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_netif_ppp.h"
#include "netif/ppp/pppapi.h"
#include "lwip/sio.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

/* OTA */
#include "ota.h"
#include "ota_config.h"
#include "ota_private.h"
#include "ota_os_freertos.h"
#include "ota_mqtt_interface.h"
#include "ota_pal.h"

#include "color_print.h"
#include "cJSON.h"

#include "aws_clientcredential.h"

/* Include firmware version struct definition. */
#include "ota_appversion32.h"

//SD card e FATFS: procure por esp_vfs_fat_sdmmc_mount
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "vfs_fat_internal.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/sdmmc_defs.h"
#include "driver/pcnt.h"
#include "driver/pulse_cnt.h"
#include "driver/rtc_io.h"
#include "driver/timer.h"
#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc_periph.h"
#include "soc/gpio_reg.h"
#include "sdmmc_cmd.h"
#include "diskio.h"
#include "ff.h"


/* ========== SYSTEM DEFINES ========== */
#define delay_ms(ms)                    vTaskDelay(pdMS_TO_TICKS(ms))
#define KEEP_ALIVE_SECONDS              ( 60 )
#define MQTT_TIMEOUT_MS                 ( 10000 )
#define WILL_TOPIC_NAME                 "basic_OTA/will"
#define WILL_TOPIC_NAME_LENGTH          ( ( uint16_t ) ( sizeof( WILL_TOPIC_NAME ) - 1 ) )
#define WILL_MESSAGE                    "MQTT connection lost."
#define WILL_MESSAGE_LENGTH             ( ( size_t ) ( sizeof( WILL_MESSAGE ) - 1 ) )
#define PUBLISH_RETRY_LIMIT             ( 10 )
#define PUBLISH_RETRY_MS                ( 1000 )

#define printf_enabled                              1

/* ========== LOCAL PRINT COLOR DEFINITIONS ========== */
#define LOCAL_COLOR_PRINT_ALERT             COLOR_PRINT_RED
#define LOCAL_COLOR_PRINT_HARDWARE          COLOR_PRINT_BLUE
#define LOCAL_COLOR_PRINT_COMMUNICATION     COLOR_PRINT_PURPLE
#define LOCAL_COLOR_PRINT_TASK              COLOR_PRINT_CYAN
#define LOCAL_COLOR_PRINT_INFO              COLOR_PRINT_GREEN
#define LOCAL_COLOR_PRINT_DEBUG             COLOR_PRINT_BROWN

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
(byte & 0x80 ? '1' : '0'), \
(byte & 0x40 ? '1' : '0'), \
(byte & 0x20 ? '1' : '0'), \
(byte & 0x10 ? '1' : '0'), \
(byte & 0x08 ? '1' : '0'), \
(byte & 0x04 ? '1' : '0'), \
(byte & 0x02 ? '1' : '0'), \
(byte & 0x01 ? '1' : '0')


// alternative way to pass certificates to the MQTT connection
extern const char root_cert_auth_pem_start[] asm("_binary_root_cert_auth_pem_start");
extern const char root_cert_auth_pem_end[] asm("_binary_root_cert_auth_pem_end");
extern const char client_cert_pem_start[] asm("_binary_client_crt_start");
extern const char client_cert_pem_end[] asm("_binary_client_crt_end");
extern const char client_key_pem_start[] asm("_binary_client_key_start");
extern const char client_key_pem_end[] asm("_binary_client_key_end");

int smp_main( int argc, char** argv );
int mqtt_publish_wifi(MQTTContext_t* pxMqttContext, const char* payload, const char* submission_topic, bool useretained);

void send_infosys();
void mqtt_subscriptions_callbacks(char* payload, size_t payload_length, char* topic_name, size_t topic_length);
    

static const uint32_t _maxCommandTimeMs = 2000;

/* ========== EXTERN VARIABLES ========== */
extern const AppVersion32_t appFirmwareVersion;

extern bool connected_by_wifi;

extern char* v_fw;
extern char* wifi_ssid;
extern char* wifi_pwd;
extern char* Id;
extern char* device_id;
extern char* submission_payload;

extern MQTTContext_t smp_mqttContext;

extern esp_err_t ret;
extern uint8_t mac[6];







