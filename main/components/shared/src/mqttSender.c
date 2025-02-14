#include "smp_global.h"

#include <pthread.h>
#include "backoff_algorithm.h"
#include "esp_pthread.h"

#define CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS    ( 5000U )
#define CONNECTION_RETRY_BACKOFF_BASE_MS         ( 500U )
#define MQTT_PUBLISH_RETRY_MAX_ATTEMPS           ( 3U )

typedef struct PublishPackets
{
    uint16_t packetId;
    MQTTPublishInfo_t pubInfo;
} PublishPackets_t;
static PublishPackets_t outgoingPublishPackets[ 5 ] = { 0 };

static BaseType_t prvgetNextFreeIndexForOutgoingPublishes(uint8_t *pucIndex);
static void vCleanupOutgoingPublishAt(uint8_t ucIndex);

static BaseType_t prvgetNextFreeIndexForOutgoingPublishes(uint8_t *pucIndex)
{
    BaseType_t xReturnStatus = pdFAIL;
    uint8_t ucIndex = 0;

    /* assert outgoingPublishPackets != NULL */
    if (outgoingPublishPackets == NULL)
    {
        ESP_LOGE("prvgetNextFreeIndexForOutgoingPublishes", "outgoingPublishPackets is null");
        return pdFALSE;
    }
    /* assert pucIndex != NULL */
    if (pucIndex == NULL)
    {
        ESP_LOGE("prvgetNextFreeIndexForOutgoingPublishes", "pucIndex is null");
        return pdFALSE;
    }

    for (ucIndex = 0; ucIndex < 1; ucIndex++)
    {
        /* A free ucIndex is marked by invalid packet id.
         * Check if the the ucIndex has a free slot. */
        if (outgoingPublishPackets[ucIndex].packetId == MQTT_PACKET_ID_INVALID)
        {
            xReturnStatus = pdPASS;
            break;
        }
    }

    /* Copy the available ucIndex into the output param. */
    *pucIndex = ucIndex;

    return xReturnStatus;
}

static void vCleanupOutgoingPublishAt( uint8_t ucIndex )
{
    /* assert outgoingPublishPackets != NULL */
    if (outgoingPublishPackets == NULL)
    {
        ESP_LOGE("vCleanupOutgoingPublishAt", "outgoingPublishPackets is null");
        return;
    }
    /* assert ucIndex < 1 */
    if (ucIndex >= 1)
    {
        ESP_LOGE("vCleanupOutgoingPublishAt", "ucIndex >= 1");
        return;
    }

    /* Clear the outgoing publish packet. */
    (void)memset(&(outgoingPublishPackets[ucIndex]), 0x00, sizeof(outgoingPublishPackets[ucIndex]));
}

int mqtt_publish_wifi(MQTTContext_t *pxMqttContext, const char* payload, const char* submission_topic, bool useretained)
{
    int xStatus = EXIT_SUCCESS;

    uint8_t publishIndex = 5;
    uint8_t ucPublishIndex = 0;
    // color_printf(LOCAL_COLOR_PRINT_INFO, "payload in mqtt_publish_wifi:\n%s\n", payload);
    BaseType_t xReturnStatus = pdPASS;
    MQTTStatus_t eMqttStatus = MQTTSuccess;    
    MQTTPublishInfo_t publishInfo = { 0 };
    BackoffAlgorithmStatus_t backoffAlgStatus = BackoffAlgorithmSuccess;
    BackoffAlgorithmContext_t reconnectParams;
    uint16_t packetId;
    uint16_t nextRetryBackOff;        

    MQTTContext_t* pMqttContext = &smp_mqttContext;

    if (pMqttContext == NULL)
    {
        ESP_LOGE("mqtt_publish_wifi", "pMqttContext is null");
        return EXIT_FAILURE;
    }
    if (submission_topic == NULL)
    {
        ESP_LOGE("mqtt_publish_wifi", "submission_topic is null");
        return EXIT_FAILURE;
    }

    xStatus = prvgetNextFreeIndexForOutgoingPublishes(&publishIndex);

    /* Initialize reconnect attempts and interval */
    BackoffAlgorithm_InitializeParams(&reconnectParams,
                                    CONNECTION_RETRY_BACKOFF_BASE_MS,
                                    CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS,
                                    MQTT_PUBLISH_RETRY_MAX_ATTEMPS
    );
    publishInfo.qos = MQTTQoS0;
    publishInfo.retain = useretained;
    publishInfo.pTopicName = (const char*)submission_topic;
    publishInfo.topicNameLength = strlen(submission_topic);
    publishInfo.pPayload = (const void *)payload;
    publishInfo.payloadLength = (size_t)strlen(payload);
    

    /* Get a new packet id. */
    packetId = MQTT_GetPacketId(pMqttContext);

    // /* Send PUBLISH packet. */
    eMqttStatus = MQTT_Publish( pMqttContext, &publishInfo, packetId );

    if (eMqttStatus != MQTTSuccess)
    {
        ESP_LOGE("mqtt_publish_wifi", "Failed to send PUBLISH packet to broker with error = %s", MQTT_Status_strerror(eMqttStatus));
        vCleanupOutgoingPublishAt(ucPublishIndex);
        xStatus = EXIT_FAILURE;
    }
    else
    {
        ESP_LOGI("mqtt_publish_wifi", "PUBLISH sent for topic %.*s to broker with packet ID %u",
                                        strlen(submission_topic),
                                        (const char*)submission_topic,
                                        packetId);
                                        // outgoingPublishPackets[publishIndex].packetId);
    }

    return xStatus;
}