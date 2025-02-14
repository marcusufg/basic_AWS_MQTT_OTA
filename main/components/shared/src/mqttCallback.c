#include "smp_global.h"

char* topic_to_compare;

int publish_ret;

void send_infosys()
{
    /* Make and return a JSON with all the relevant information about the system */
    char* infosys_tmp;
    char* ssid;
    char* rssi;
    char* heap_livre;
    char* heap_minimo;
    wifi_ap_record_t ap; esp_wifi_sta_get_ap_info(&ap);
    
    asprintf(&ssid, "%s",ap.ssid);
    asprintf(&rssi, "%d",ap.rssi);
    asprintf(&heap_livre, "%u",(unsigned int)xPortGetFreeHeapSize());
    asprintf(&heap_minimo, "%u",(unsigned int)xPortGetMinimumEverFreeHeapSize());

    cJSON* json_infosys = cJSON_CreateObject();
    
    cJSON* json_device_id = cJSON_CreateString(device_id);
    cJSON* json_v_FW = cJSON_CreateString(v_fw);
    cJSON* json_ssid = cJSON_CreateString(ssid);
    cJSON* json_rssi = cJSON_CreateString(rssi);
    cJSON* json_heap_livre = cJSON_CreateString(heap_livre);
    cJSON* json_heap_minimo = cJSON_CreateString(heap_minimo);

    cJSON_AddItemToObject(json_infosys, "device_id", json_device_id);
    cJSON_AddItemToObject(json_infosys, "v_fw", json_v_FW);
    cJSON_AddItemToObject(json_infosys, "ssid", json_ssid);
    cJSON_AddItemToObject(json_infosys, "rssi", json_rssi);
    
    cJSON_AddItemToObject(json_infosys, "heap", json_heap_livre);
    cJSON_AddItemToObject(json_infosys, "heap_min", json_heap_minimo);
    infosys_tmp = cJSON_Print(json_infosys);
    cJSON_Delete(json_infosys);
    // if (printf_enabled) printf("infosys:\n%s\n",infosys_tmp);

    char submission_topic[64];
    sprintf((char*)&submission_topic, "S/%s/infosys", device_id);
    // if (printf_enabled) printf("submission_topic: %s\n",submission_topic);

    int infosys_len = strlen(infosys_tmp);
    char infosys[infosys_len]; memset(infosys, 0x00, infosys_len);
    sprintf((char*)&infosys, "%s", infosys_tmp);
    vPortFree(infosys_tmp);

    publish_ret = mqtt_publish_wifi( &smp_mqttContext, (char*)&infosys, (char*)&submission_topic, 0);
    
    if (ssid) vPortFree(ssid);
    if (rssi) vPortFree(rssi);
    if (heap_livre) vPortFree(heap_livre);
    if (heap_minimo) vPortFree(heap_minimo);
}

void mqtt_subscriptions_callbacks(char* payload_input, size_t payload_length, char* topic_name_input, size_t topic_length)
{
    char* payload = strdup(payload_input);
    char* arrival_topic = strdup(topic_name_input);
    payload[payload_length] = 0;
    arrival_topic[topic_length] = '\0';
    color_printf(LOCAL_COLOR_PRINT_COMMUNICATION, "MQTT message received at: %s", arrival_topic);

    {
        /* Local Group - System Actions */
        {
            asprintf(&topic_to_compare, "S/%s/sys/reset", device_id);
            if (strcmp(arrival_topic, topic_to_compare) == 0) 
            {
                color_printf(LOCAL_COLOR_PRINT_ALERT, "Reset requested!");
                esp_restart();
            }
            vPortFree(topic_to_compare);

            asprintf(&topic_to_compare, "S/%s/sys/getsys", device_id);
            if (strcmp(arrival_topic, topic_to_compare) == 0) 
                send_infosys();
            vPortFree(topic_to_compare);
        }

        /* Local Group - Shadow Actions */
        {
            asprintf(&topic_to_compare, "$aws/things/%s/shadow/get/accepted", device_id);
            if (strcmp(arrival_topic, topic_to_compare) == 0)
            {
                // printf("payload shadow/get/accepted: \n%s\n\n", payload); //for debug

                // TODO: manipulate the shadow data
            }
            vPortFree(topic_to_compare);

            asprintf(&topic_to_compare, "$aws/things/%s/shadow/update/accepted", device_id);
            if (strcmp(arrival_topic, topic_to_compare) == 0)
            {
                // printf("payload shadow/update/accepted: \n%s\n\n", payload); //for debug

                // TODO: manipulate the shadow data
            }
            vPortFree(topic_to_compare);
        }
    }

    vPortFree(payload);
    vPortFree(arrival_topic);
}