#include "smp_global.h"

char* device_id = "POC_1";
char* wifi_ssid = "SimemapIoT";
char* wifi_pwd = "100\%Simemap";

const AppVersion32_t appFirmwareVersion =
{
    .u.x.major = 1,
    .u.x.minor = 1,
    .u.x.build = 1,
};

static const char* TAG = "main";

bool connected_by_wifi = 0;

char* Id;
char* v_fw;

uint8_t mac[6];

static esp_err_t err = ESP_OK;

extern void esp_vApplicationTickHook();
extern void esp_vApplicationIdleHook();
void task_in_CPU1( void * pvParameters );

void enableFlushAfterPrintf() 
{
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stdin, 0, _IONBF, 0);
}

void task_free_heap(void *pvParameters)
{
    /* Enable task wdt in this task */
    esp_task_wdt_add(NULL);

    ESP_LOGI("task_free_heap", "Task task_free_heap created");

    while (1)
    {
        esp_task_wdt_reset(); /* Feed WDT */
        
        if (xPortGetMinimumEverFreeHeapSize() < 2000)
        {
            color_printf(COLOR_PRINT_RED, "RESET: MinimumEverFreeHeapSize too low\n");
            esp_restart();
        }
        if (connected_by_wifi)
            send_infosys();
            
        ESP_LOGW("heap_scan", "FreeHeapSize: %u | LargestFreeBlock: %d | MinimumEverFreeHeapSize: %u",
            (unsigned int)xPortGetFreeHeapSize(), 
            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), 
            (unsigned int)xPortGetMinimumEverFreeHeapSize());

        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}



/**
 * @brief Starting point function
 * 
 * @author Marcus A. P. Oliveira
 */
void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
 
    /* Set global log level */
    esp_log_set_level_master(ESP_LOG_INFO);
 
    /* Initialize NVS partition */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    enableFlushAfterPrintf();

    ESP_LOGI("main", "[APP] Starting..");
    ESP_LOGI("main", "[APP] Free memory: %"PRIu32" bytes", esp_get_free_heap_size());
    ESP_LOGI("main", "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI("main", "esp_clk_cpu_freq: %d", esp_clk_cpu_freq());

    /* Start task wdt with 120 seconds */
    esp_task_wdt_config_t wdt_config = 
    {
        .timeout_ms = 120000,
        .idle_core_mask = (1 << 0), //CPU 0
        // .idle_core_mask = (1 << 1), //CPU 1
        .trigger_panic = true,
    };
    esp_task_wdt_reconfigure(&wdt_config);

    asprintf(&v_fw, "%u.%u.%u", appFirmwareVersion.u.x.major, appFirmwareVersion.u.x.minor, appFirmwareVersion.u.x.build);
    ESP_LOGI("main", "\nFirmware v. %u.%u.%u\n", appFirmwareVersion.u.x.major, appFirmwareVersion.u.x.minor, appFirmwareVersion.u.x.build);

    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    asprintf(&Id, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI("main", "MAC: %s", Id);

    smp_connect();

    xTaskCreate(&task_free_heap, "task_free_heap", 10000, NULL, 3, NULL);

    smp_main(0,NULL);
}



// hook functions according to https://esp32.com/viewtopic.php?t=23133
void IRAM_ATTR vApplicationTickHook()
{
    esp_vApplicationTickHook();
}

void vApplicationIdleHook()
{
    esp_vApplicationIdleHook();
}

void vApplicationDaemonTaskStartupHook( void )
{
}

// if CPU1 is enabled, this function will be called
void task_in_CPU1( void * pvParameters )
{ 
    /* Enable task wdt in this task */
    esp_task_wdt_add(NULL);

    while(1)
    {
        /* --- Feed wdt --- */
        esp_task_wdt_reset();
        printf("hi from CPU1\n");

        vTaskDelay(pdMS_TO_TICKS(5000));
    } 
}