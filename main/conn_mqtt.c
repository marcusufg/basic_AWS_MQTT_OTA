#include "smp_global.h"

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#define AWS_IOT_ENDPOINT  "a23kawqisrluu3-ats.iot.us-east-2.amazonaws.com"
// #define AWS_IOT_ENDPOINT  "192.168.15.31" //greengrass
#define AWS_MQTT_PORT    ( 8883 )

#define OS_NAME                   "FreeRTOS"
#define OS_VERSION                tskKERNEL_VERSION_NUMBER

#define HARDWARE_PLATFORM_NAME    CONFIG_HARDWARE_PLATFORM_NAME

#define OTA_LIB                   "otalib@1.0.0"



/* OpenSSL sockets transport implementation. */
#include "network_transport.h"

/* Clock for timer. */
#include "clock.h"

/* pthread include. */
#include <pthread.h>
#include "semaphore.h"
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_pthread.h"

/* MQTT include. */
#include "core_mqtt.h"
#include "core_mqtt_state.h"
#include "mqtt_subscription_manager.h"

/*Include backoff algorithm header for retry logic.*/
#include "backoff_algorithm.h"

/* OTA Library include. */
#include "ota.h"
#include "ota_config.h"

/* OTA Library Interface include. */
#include "ota_os_freertos.h"
#include "ota_mqtt_interface.h"
#include "ota_pal.h"

/* Include firmware version struct definition. */
#include "ota_appversion32.h"

extern const char pcAwsCodeSigningCertPem[] asm("_binary_aws_codesign_crt_start");
// char* payload;
// char* topic_name;

static uint16_t globalUnsubscribePacketIdentifier = 0U;
static uint16_t globalAckPacketIdentifier = 0U;


#define AWS_IOT_MQTT_ALPN                   "x-amzn-mqtt-ca"

/**
 * @brief Length of ALPN protocol name.
 */
#define AWS_IOT_MQTT_ALPN_LENGTH            ( ( uint16_t ) ( sizeof( AWS_IOT_MQTT_ALPN ) - 1 ) )

/**
 * @brief Length of MQTT server host name.
 */
#define AWS_IOT_ENDPOINT_LENGTH             ( ( uint16_t ) ( sizeof( AWS_IOT_ENDPOINT ) - 1 ) )

/**
 * @brief Transport timeout in milliseconds for transport send and receive.
 */
#define TRANSPORT_SEND_RECV_TIMEOUT_MS      ( 1500U )

/**
 * @brief Timeout for receiving CONNACK packet in milli seconds.
 */
#define CONNACK_RECV_TIMEOUT_MS             ( 2000U )

#ifndef MQTT_PRE_STATE_UPDATE_HOOK

/**
 * @brief Hook called just before an update to the MQTT state is made.
 */
    #define MQTT_PRE_STATE_UPDATE_HOOK( pContext )
#endif /* !MQTT_PRE_STATE_UPDATE_HOOK */

/**
 * @brief The maximum time interval in seconds which is allowed to elapse
 * between two Control Packets.
 *
 * It is the responsibility of the Client to ensure that the interval between
 * Control Packets being sent does not exceed the this Keep Alive value. In the
 * absence of sending any other Control Packets, the Client MUST send a
 * PINGREQ Packet.
 */
#define MQTT_KEEP_ALIVE_INTERVAL_SECONDS    ( 60U )

/**
 * @brief Maximum number or retries to publish a message in case of failures.
 */
#define MQTT_PUBLISH_RETRY_MAX_ATTEMPS      ( 3U )

#ifndef MQTT_POST_STATE_UPDATE_HOOK

/**
 * @brief Hook called just after an update to the MQTT state has
 * been made.
 */
    #define MQTT_POST_STATE_UPDATE_HOOK( pContext )
#endif /* !MQTT_POST_STATE_UPDATE_HOOK */

/**
 * @brief Period for waiting on ack.
 */
#define MQTT_ACK_TIMEOUT_MS                 ( 5000U )

#ifndef MQTT_PRE_SEND_HOOK

/**
 * @brief Hook called before a 'send' operation is executed.
 */
    #define MQTT_PRE_SEND_HOOK( pContext )
#endif /* !MQTT_PRE_SEND_HOOK */

#ifndef MQTT_POST_SEND_HOOK
/**
 * @brief Hook called after the 'send' operation is complete.
 */
    #define MQTT_POST_SEND_HOOK( pContext )
#endif /* !MQTT_POST_SEND_HOOK */

/**
 * @brief Period for loop sleep in milliseconds.
 */
#define OTA_EXAMPLE_LOOP_SLEEP_PERIOD_MS    ( 15U )

/**
 * @brief Size of the network buffer to receive the MQTT message.
 *
 * The largest message size is data size from the AWS IoT streaming service,
 * otaconfigFILE_BLOCK_SIZE + extra for headers.
 */

#define OTA_NETWORK_BUFFER_SIZE                  ( otaconfigFILE_BLOCK_SIZE + 128 )

/**
 * @brief The delay used in the main OTA task loop to periodically output the OTA
 * statistics like number of packets received, dropped, processed and queued per connection.
 */
#define OTA_EXAMPLE_TASK_DELAY_MS                ( 1000U )

/**
 * @brief The timeout for waiting for the agent to get suspended after closing the
 * connection.
 */
#define OTA_SUSPEND_TIMEOUT_MS                   ( 5000U )

/**
 * @brief The timeout for waiting before exiting the OTA.
 */
#define OTA_EXIT_TIMEOUT_MS                 ( 3000U )

/**
 * @brief The maximum size of the file paths used in the.
 */
#define OTA_MAX_FILE_PATH_SIZE                   ( 260U )

/**
 * @brief The maximum size of the stream name required for downloading update file
 * from streaming service.
 */
#define OTA_MAX_STREAM_NAME_SIZE                 ( 128U )

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying connection to server.
 */
#define CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS    ( 60000U )

/**
 * @brief The base back-off delay (in milliseconds) to use for connection retry attempts.
 */
#define CONNECTION_RETRY_BACKOFF_BASE_MS         ( 5000U )

/**
 * @brief Number of milliseconds in a second.
 */
#define NUM_MILLISECONDS_IN_SECOND               ( 1000U )

/**
 * @brief The maximum number of retries for connecting to server.
 */
#define CONNECTION_RETRY_MAX_ATTEMPTS            ( 10U )

/**
 * @brief The MQTT metrics string expected by AWS IoT.
 */
#define METRICS_STRING                           "?SDK=" OS_NAME "&Version=" OS_VERSION "&Platform=" HARDWARE_PLATFORM_NAME "&OTALib=" OTA_LIB

/**
 * @brief The length of the MQTT metrics string expected by AWS IoT.
 */
#define METRICS_STRING_LENGTH                    ( ( uint16_t ) ( sizeof( METRICS_STRING ) - 1 ) )


#ifdef CLIENT_USERNAME
    #define CLIENT_USERNAME_WITH_METRICS    CLIENT_USERNAME METRICS_STRING
#endif

/**
 * @brief The common prefix for all OTA topics.
 */
#define OTA_TOPIC_PREFIX    "$aws/things/+/"

/**
 * @brief The string used for jobs topics.
 */
#define OTA_TOPIC_JOBS      "jobs"

/**
 * @brief The string used for streaming service topics.
 */
#define OTA_TOPIC_STREAM    "streams"

/**
 * @brief The length of the outgoing publish records array used by the coreMQTT
 * library to track QoS > 0 packet ACKS for outgoing publishes.
 */
#define OUTGOING_PUBLISH_RECORD_LEN    ( 10U )

/**
 * @brief The length of the incoming publish records array used by the coreMQTT
 * library to track QoS > 0 packet ACKS for incoming publishes.
 */
#define INCOMING_PUBLISH_RECORD_LEN    ( 10U )

/*-----------------------------------------------------------*/

/* Linkage for error reporting. */
extern int errno;


/**
 * @brief Static buffer for TLS Context Semaphore.
 */
static StaticSemaphore_t xTlsContextSemaphoreBuffer;

/**
 * @brief Network connection context.
 */
static NetworkContext_t networkContext;

/**
 * @brief MQTT connection context.
 */
// static MQTTContext_t smp_mqttContext;
MQTTContext_t smp_mqttContext;

/**
 * @brief Keep a flag for indicating if the MQTT connection is alive.
 */
static bool mqttSessionEstablished = false;

/**
 * @brief Mutex for synchronizing coreMQTT API calls.
 */
pthread_mutex_t mqttMutex;

/**
 * @brief Semaphore for synchronizing buffer operations.
 */
// static osi_sem_t bufferSemaphore;
static osi_sem_t bufferSemaphore;

/**
 * @brief Semaphore for synchronizing wait for ack.
 */
static osi_sem_t ackSemaphore;

/**
 * @brief Enum for type of OTA job messages received.
 */
typedef enum jobMessageType
{
    jobMessageTypeNextGetAccepted = 0,
    jobMessageTypeNextNotify,
    jobMessageTypeMax
} jobMessageType_t;

/**
 * @brief The network buffer must remain valid when OTA library task is running.
 */
static uint8_t otaNetworkBuffer[ OTA_NETWORK_BUFFER_SIZE ];

/**
 * @brief Update File path buffer.
 */
uint8_t updateFilePath[ OTA_MAX_FILE_PATH_SIZE ];

/**
 * @brief Certificate File path buffer.
 */
uint8_t certFilePath[ OTA_MAX_FILE_PATH_SIZE ];

/**
 * @brief Stream name buffer.
 */
uint8_t streamName[ OTA_MAX_STREAM_NAME_SIZE ];

/**
 * @brief Decode memory.
 */
uint8_t decodeMem[ otaconfigFILE_BLOCK_SIZE ];

/**
 * @brief Bitmap memory.
 */
uint8_t bitmap[ OTA_MAX_BLOCK_BITMAP_SIZE ];

/**
 * @brief Event buffer.
 */
static OtaEventData_t eventBuffer[ otaconfigMAX_NUM_OTA_DATA_BUFFERS ];

/**
 * @brief The buffer passed to the OTA Agent from application while initializing.
 */
static OtaAppBuffer_t otaBuffer =
{
    .pUpdateFilePath    = updateFilePath,
    .updateFilePathsize = OTA_MAX_FILE_PATH_SIZE,
    .pCertFilePath      = certFilePath,
    .certFilePathSize   = OTA_MAX_FILE_PATH_SIZE,
    .pStreamName        = streamName,
    .streamNameSize     = OTA_MAX_STREAM_NAME_SIZE,
    .pDecodeMemory      = decodeMem,
    .decodeMemorySize   = otaconfigFILE_BLOCK_SIZE,
    .pFileBitmap        = bitmap,
    .fileBitmapSize     = OTA_MAX_BLOCK_BITMAP_SIZE
};

/**
 * @brief Array to track the outgoing publish records for outgoing publishes
 * with QoS > 0.
 *
 * This is passed into #MQTT_InitStatefulQoS to allow for QoS > 0.
 *
 */
static MQTTPubAckInfo_t pOutgoingPublishRecords[ OUTGOING_PUBLISH_RECORD_LEN ];

/**
 * @brief Array to track the incoming publish records for incoming publishes
 * with QoS > 0.
 *
 * This is passed into #MQTT_InitStatefulQoS to allow for QoS > 0.
 *
 */
static MQTTPubAckInfo_t pIncomingPublishRecords[ INCOMING_PUBLISH_RECORD_LEN ];

static int32_t sendBuffer( MQTTContext_t * pContext,
                           const uint8_t * pBufferToSend,
                           size_t bytesToSend );

/**
 * @brief Get the correct ack type to send.
 *
 * @param[in] state Current state of publish.
 *
 * @return Packet Type byte of PUBACK, PUBREC, PUBREL, or PUBCOMP if one of
 * those should be sent, else 0.
 */
static uint8_t getAckTypeToSend( MQTTPublishState_t state );

/**
 * @brief Convert a byte indicating a publish ack type to an #MQTTPubAckType_t.
 *
 * @param[in] packetType First byte of fixed header.
 *
 * @return Type of ack.
 */
static MQTTPubAckType_t getAckFromPacketType( uint8_t packetType );

/**
 * @brief Send acks for received QoS 1/2 publishes.
 *
 * @param[in] pContext MQTT Connection context.
 * @param[in] packetId packet ID of original PUBLISH.
 * @param[in] publishState Current publish state in record.
 *
 * @return #MQTTSuccess, #MQTTIllegalState or #MQTTSendFailed.
 */
static MQTTStatus_t sendPublishAcks( MQTTContext_t * pContext,
                                     uint16_t packetId,
                                     MQTTPublishState_t publishState );

// /**
//  * @brief Handle received MQTT PUBLISH packet.
//  *
//  * @param[in] pContext MQTT Connection context.
//  * @param[in] pIncomingPacket Incoming packet.
//  *
//  * @return MQTTSuccess, MQTTIllegalState or deserialization error.
//  */
// static MQTTStatus_t handleIncomingPublish( MQTTContext_t * pContext,
//                                            MQTTPacketInfo_t * pIncomingPacket );

/*-----------------------------------------------------------*/

int smp_main( int argc, char** argv );

/**
 * @brief Sends an MQTT CONNECT packet over the already connected TCP socket.
 *
 * @param[in] pMqttContext MQTT context pointer.
 * @param[in] createCleanSession Creates a new MQTT session if true.
 * If false, tries to establish the existing session if there was session
 * already present in broker.
 * @param[out] pSessionPresent Session was already present in the broker or not.
 * Session present response is obtained from the CONNACK from broker.
 *
 * @return EXIT_SUCCESS if an MQTT session is established;
 * EXIT_FAILURE otherwise.
 */
static int establishMqttSession( MQTTContext_t * pMqttContext );


/**
 * @brief Publish message to a topic.
 *
 * This function publishes a message to a given topic & QoS.
 *
 * @param[in] pacTopic Mqtt topic filter.
 *
 * @param[in] topicLen Length of the topic filter.
 *
 * @param[in] pMsg Message to publish.
 *
 * @param[in] msgSize Message size.
 *
 * @param[in] qos Quality of Service
 *
 * @return OtaMqttSuccess if success , other error code on failure.
 */
static OtaMqttStatus_t mqttPublish( const char*  const pacTopic,
                                    uint16_t topicLen,
                                    const char*  pMsg,
                                    uint32_t msgSize,
                                    uint8_t qos );

/**
 * @brief Subscribe to the MQTT topic filter, and registers the handler for the topic filter with the subscription manager.
 *
 * This function subscribes to the Mqtt topics with the Quality of service
 * received as parameter. This function also registers a callback for the
 * topicfilter.
 *
 * @param[in] pTopicFilter Mqtt topic filter.
 *
 * @param[in] topicFilterLength Length of the topic filter.
 *
 * @param[in] qos Quality of Service
 *
 * @return OtaMqttSuccess if success , other error code on failure.
 */
static OtaMqttStatus_t mqttSubscribe( const char*  pTopicFilter,
                                      uint16_t topicFilterLength,
                                      uint8_t qos );

/**
 * @brief Unsubscribe to the Mqtt topics.
 *
 * This function unsubscribes to the Mqtt topics with the Quality of service
 * received as parameter.
 *
 * @param[in] pTopicFilter Mqtt topic filter.
 *
 * @param[in] topicFilterLength Length of the topic filter.
 *
 * @param[qos] qos Quality of Service
 *
 * @return  OtaMqttSuccess if success , other error code on failure.
 */
static OtaMqttStatus_t mqttUnsubscribe( const char*  pTopicFilter,
                                        uint16_t topicFilterLength,
                                        uint8_t qos );

/**
 * @brief Thread to call the OTA agent task.
 *
 * @param[in] pParam Can be used to pass down functionality to the agent task
 * @return void* returning null.
 */
static void * otaThread( void * pParam );

/**
 * @brief Start OTA.
 *
 * The OTA task is created with initializing the OTA agent and
 * setting the required interfaces. The loop then starts,
 * establishing an MQTT connection with the broker and waiting
 * for an update. After a successful update the OTA agent requests
 * a manual reset to the downloaded executable.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE.
 */
static int startMain( void );

/**
 * @brief Set OTA interfaces.
 *
 * @param[in]  pOtaInterfaces pointer to OTA interface structure.
 */
static void setOtaInterfaces( OtaInterfaces_t * pOtaInterfaces );

/**
 * @brief Disconnect from the MQTT broker and close connection.
 *
 */
static void disconnect( void );

/**
 * @brief Attempt to connect to the MQTT broker.
 *
 * @return int EXIT_SUCCESS if a connection is established.
 */
static int establishConnection( void );

/**
 * @brief Initialize MQTT by setting up transport interface and network.
 *
 * @param[in] pMqttContext Structure representing MQTT connection.
 * @param[in] pNetworkContext Network context to connect on.
 * @return int EXIT_SUCCESS if MQTT component is initialized
 */
static int initializeMqtt( MQTTContext_t * pMqttContext,
                           NetworkContext_t * pNetworkContext );

/**
 * @brief Retry logic to establish a connection to the server.
 *
 * If the connection fails, keep retrying with exponentially increasing
 * timeout value, until max retries, max timeout or successful connect.
 *
 * @param[in] pNetworkContext Network context to connect on.
 * @return int EXIT_FAILURE if connection failed after retries.
 */
static int connectToServerWithBackoffRetries( NetworkContext_t * pNetworkContext );

/**
 * @brief Random number to be used as a back-off value for retrying connection.
 *
 * @return uint32_t The generated random number.
 */
static uint32_t generateRandomNumber();

/* Callbacks used to handle different events. */

/**
 * @brief The OTA agent has completed the update job or it is in
 * self test mode. If it was accepted, we want to activate the new image.
 * This typically means we should reset the device to run the new firmware.
 * If now is not a good time to reset the device, it may be activated later
 * by your user code. If the update was rejected, just return without doing
 * anything and we'll wait for another job. If it reported that we should
 * start test mode, normally we would perform some kind of system checks to
 * make sure our new firmware does the basic things we think it should do
 * but we'll just go ahead and set the image as accepted.
 * The accept function varies depending on your platform. Refer to the OTA
 * PAL implementation for your platform in aws_ota_pal.c to see what it
 * does for you.
 *
 * @param[in] event Event from OTA lib of type OtaJobEvent_t.
 * @return None.
 */
static void otaAppCallback( OtaJobEvent_t event,
                            void * pData );

/**
 * @brief callback to use with the MQTT context to notify incoming packet events.
 *
 * @param[in] pMqttContext MQTT context which stores the connection.
 * @param[in] pPacketInfo Parameters of the incoming packet.
 * @param[in] pDeserializedInfo Deserialized packet information to be dispatched by
 * the subscription manager to event callbacks.
 */
void mqttEventCallback( MQTTContext_t * pMqttContext,
                               MQTTPacketInfo_t * pPacketInfo,
                               MQTTDeserializedInfo_t * pDeserializedInfo );

/**
 * @brief Callback registered with the OTA library that notifies the OTA agent
 * of an incoming PUBLISH containing a job document.
 *
 * @param[in] pContext MQTT context which stores the connection.
 * @param[in] pPublishInfo MQTT packet information which stores details of the
 * job document.
 */
void mqttJobCallback( MQTTContext_t * pContext,
                             MQTTPublishInfo_t * pPublishInfo );

/**
 * @brief Callback that notifies the OTA library when a data block is received.
 *
 * @param[in] pContext MQTT context which stores the connection.
 * @param[in] pPublishInfo MQTT packet that stores the information of the file block.
 */
void mqttDataCallback( MQTTContext_t * pContext,
                              MQTTPublishInfo_t * pPublishInfo );

static SubscriptionManagerCallback_t otaMessageCallback[] = { mqttJobCallback, mqttDataCallback };

/*-----------------------------------------------------------*/

static int32_t sendBuffer( MQTTContext_t * pContext,
                           const uint8_t * pBufferToSend,
                           size_t bytesToSend )
{
    int32_t sendResult;
    uint32_t timeoutMs;
    int32_t bytesSentOrError = 0;
    const uint8_t * pIndex = pBufferToSend;

    assert( pContext != NULL );
    assert( pContext->getTime != NULL );
    assert( pContext->transportInterface.send != NULL );
    assert( pIndex != NULL );

    /* Set the timeout. */
    timeoutMs = pContext->getTime() + MQTT_SEND_TIMEOUT_MS;

    while( ( bytesSentOrError < ( int32_t ) bytesToSend ) && ( bytesSentOrError >= 0 ) )
    {
        sendResult = pContext->transportInterface.send( pContext->transportInterface.pNetworkContext,
                                                        pIndex,
                                                        bytesToSend - ( size_t ) bytesSentOrError );

        if( sendResult > 0 )
        {
            /* It is a bug in the application's transport send implementation if
             * more bytes than expected are sent. */
            assert( sendResult <= ( ( int32_t ) bytesToSend - bytesSentOrError ) );

            bytesSentOrError += sendResult;
            pIndex = &pIndex[ sendResult ];

            /* Set last transmission time. */
            pContext->lastPacketTxTime = pContext->getTime();

            LogDebug( ( "sendBuffer: Bytes Sent=%ld, Bytes Remaining=%lu",
                        ( long int ) sendResult,
                        ( unsigned long ) ( bytesToSend - ( size_t ) bytesSentOrError ) ) );
        }
        else if( sendResult < 0 )
        {
            bytesSentOrError = sendResult;
            LogError( ( "sendBuffer: Unable to send packet: Network Error." ) );
        }
        else
        {
            /* MISRA Empty body */
        }

        /* Check for timeout. */
        if( pContext->getTime() >= timeoutMs )
        {
            LogError( ( "sendBuffer: Unable to send packet: Timed out." ) );
            break;
        }
    }

    return bytesSentOrError;
}

void otaEventBufferFree( OtaEventData_t * const pxBuffer )
{
    if( osi_sem_take( &bufferSemaphore, 0 ) == 0 )
    {
        pxBuffer->bufferUsed = false;
        ( void ) osi_sem_give( &bufferSemaphore );
    }
    else
    {
        LogError( ( "Failed to get buffer semaphore: "
                    ",errno=%s",
                    strerror( errno ) ) );
    }
}

/*-----------------------------------------------------------*/

OtaEventData_t * otaEventBufferGet( void )
{
    uint32_t ulIndex = 0;
    OtaEventData_t * pFreeBuffer = NULL;

    if( osi_sem_take( &bufferSemaphore, 0 ) == 0 )
    {
        for( ulIndex = 0; ulIndex < otaconfigMAX_NUM_OTA_DATA_BUFFERS; ulIndex++ )
        {
            if( eventBuffer[ ulIndex ].bufferUsed == false )
            {
                eventBuffer[ ulIndex ].bufferUsed = true;
                pFreeBuffer = &eventBuffer[ ulIndex ];
                break;
            }
        }

        ( void ) osi_sem_give( &bufferSemaphore );
    }
    else
    {
        LogError( ( "Failed to get buffer semaphore: "
                    ",errno=%s",
                    strerror( errno ) ) );
    }

    return pFreeBuffer;
}

/*-----------------------------------------------------------*/

static void otaAppCallback( OtaJobEvent_t event,
                            void * pData )
{
    OtaErr_t err = OtaErrUninitialized;
    int ret;

    switch( event )
    {
        case OtaJobEventActivate:
            LogInfo( ( "Received OtaJobEventActivate callback from OTA Agent." ) );
            OTA_ActivateNewImage();

            OTA_Shutdown( 0, 1 );
            LogError( ( "New image activation failed." ) );

            break;

        case OtaJobEventFail:
            LogInfo( ( "Received OtaJobEventFail callback from OTA Agent." ) );

            /* Nothing special to do. The OTA agent handles it. */
            break;

        case OtaJobEventStartTest:
            LogInfo( ( "Received OtaJobEventStartTest callback from OTA Agent." ) );
            err = OTA_SetImageState( OtaImageStateAccepted );

            if( err == OtaErrNone ) {
                /* Erasing passive partition */
                ret = otaPal_EraseLastBootPartition();
                if (ret != ESP_OK) {
                   ESP_LOGE("otaAppCallback", "Failed to erase last boot partition! (%d)", ret);
                }
            } else {
                LogError( ( " Failed to set image state as accepted." ) );
            }

            break;

        case OtaJobEventProcessed:
            LogDebug( ( "Received OtaJobEventProcessed callback from OTA Agent." ) );

            if( pData != NULL )
            {
                otaEventBufferFree( ( OtaEventData_t * ) pData );
            }

            break;

        case OtaJobEventSelfTestFailed:
            LogDebug( ( "Received OtaJobEventSelfTestFailed callback from OTA Agent." ) );

            /* Requires manual activation of previous image as self-test for
             * new image downloaded failed.*/
            LogError( ( "Self-test failed, shutting down OTA Agent." ) );

            /* Shutdown OTA Agent, if it is required that the unsubscribe operations are not
             * performed while shutting down please set the second parameter to 0 instead of 1. */
            OTA_Shutdown( 0, 1 );

            break;

        default:
            LogDebug( ( "Received invalid callback event from OTA Agent." ) );
    }
}

jobMessageType_t getJobMessageType( const char*  pTopicName,
                                    uint16_t topicNameLength )
{
    uint16_t index = 0U;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    bool isMatch = false;
    jobMessageType_t jobMessageIndex = jobMessageTypeMax;

    /* For suppressing compiler-warning: unused variable. */
    ( void ) mqttStatus;

    /* Lookup table for OTA job message string. */
    static const char*  const pJobTopicFilters[ jobMessageTypeMax ] =
    {
        OTA_TOPIC_PREFIX OTA_TOPIC_JOBS "/$next/get/accepted",
        OTA_TOPIC_PREFIX OTA_TOPIC_JOBS "/notify-next",
    };

    /* Match the input topic filter against the wild-card pattern of topics filters
    * relevant for the OTA Update service to determine the type of topic filter. */
    for( ; index < jobMessageTypeMax; index++ )
    {
        mqttStatus = MQTT_MatchTopic( pTopicName,
                                      topicNameLength,
                                      pJobTopicFilters[ index ],
                                      strlen( pJobTopicFilters[ index ] ),
                                      &isMatch );
        assert( mqttStatus == MQTTSuccess );

        if( isMatch )
        {
            jobMessageIndex = index;
            break;
        }
    }

    return jobMessageIndex;
}

/*-----------------------------------------------------------*/

void mqttJobCallback( MQTTContext_t * pContext,
                             MQTTPublishInfo_t * pPublishInfo )
{
    OtaEventData_t * pData;
    OtaEventMsg_t eventMsg = { 0 };
    jobMessageType_t jobMessageType = 0;

    assert( pPublishInfo != NULL );
    assert( pContext != NULL );

    ( void ) pContext;

    jobMessageType = getJobMessageType( pPublishInfo->pTopicName, pPublishInfo->topicNameLength );

    switch( jobMessageType )
    {
        case jobMessageTypeNextGetAccepted:
        case jobMessageTypeNextNotify:

            pData = otaEventBufferGet();

            if( pData != NULL )
            {
                memcpy( pData->data, pPublishInfo->pPayload, pPublishInfo->payloadLength );
                pData->dataLength = pPublishInfo->payloadLength;
                eventMsg.eventId = OtaAgentEventReceivedJobDocument;
                eventMsg.pEventData = pData;

                /* Send job document received event. */
                OTA_SignalEvent( &eventMsg );
            }
            else
            {
                LogError( ( "No OTA data buffers available." ) );
            }

            break;

        default:
            LogInfo( ( "Received job message %s size %zu.\n\n",
                       pPublishInfo->pTopicName,
                       pPublishInfo->payloadLength ) );
    }
}

/*-----------------------------------------------------------*/

void mqttDataCallback( MQTTContext_t * pContext,
                              MQTTPublishInfo_t * pPublishInfo )
{
    OtaEventData_t * pData;
    OtaEventMsg_t eventMsg = { 0 };

    assert( pPublishInfo != NULL );
    assert( pContext != NULL );

    ( void ) pContext;

    LogInfo( ( "Received data message callback, size %zu.\n\n", pPublishInfo->payloadLength ) );
    LogInfo( ( "Received data message topic:\n%s\n", (char*)pPublishInfo->pTopicName ) );
    LogInfo( ( "Received data message payload:\n%s\n", (char*)pPublishInfo->pPayload ) );

    pData = otaEventBufferGet();

    if( pData != NULL )
    {
        memcpy( pData->data, pPublishInfo->pPayload, pPublishInfo->payloadLength );
        pData->dataLength = pPublishInfo->payloadLength;
        eventMsg.eventId = OtaAgentEventReceivedFileBlock;
        eventMsg.pEventData = pData;

        /* Send job document received event. */
        OTA_SignalEvent( &eventMsg );
    }
    else
    {
        LogError( ( "No OTA data buffers available." ) );
    }
}


/*-----------------------------------------------------------*/

static uint8_t getAckTypeToSend( MQTTPublishState_t state )
{
    uint8_t packetTypeByte = 0U;

    switch( state )
    {
        case MQTTPubAckSend:
            packetTypeByte = MQTT_PACKET_TYPE_PUBACK;
            break;

        case MQTTPubRecSend:
            packetTypeByte = MQTT_PACKET_TYPE_PUBREC;
            break;

        case MQTTPubRelSend:
            packetTypeByte = MQTT_PACKET_TYPE_PUBREL;
            break;

        case MQTTPubCompSend:
            packetTypeByte = MQTT_PACKET_TYPE_PUBCOMP;
            break;

        case MQTTPubAckPending:
        case MQTTPubCompPending:
        case MQTTPubRecPending:
        case MQTTPubRelPending:
        case MQTTPublishDone:
        case MQTTPublishSend:
        case MQTTStateNull:
        default:
            /* Take no action for states that do not require sending an ack. */
            break;
    }

    return packetTypeByte;
}

static MQTTPubAckType_t getAckFromPacketType( uint8_t packetType )
{
    MQTTPubAckType_t ackType = MQTTPuback;

    switch( packetType )
    {
        case MQTT_PACKET_TYPE_PUBACK:
            ackType = MQTTPuback;
            break;

        case MQTT_PACKET_TYPE_PUBREC:
            ackType = MQTTPubrec;
            break;

        case MQTT_PACKET_TYPE_PUBREL:
            ackType = MQTTPubrel;
            break;

        case MQTT_PACKET_TYPE_PUBCOMP:
        default:

            /* This function is only called after checking the type is one of
             * the above four values, so packet type must be PUBCOMP here. */
            assert( packetType == MQTT_PACKET_TYPE_PUBCOMP );
            ackType = MQTTPubcomp;
            break;
    }

    return ackType;
}

static MQTTStatus_t sendPublishAcks( MQTTContext_t * pContext,
                                     uint16_t packetId,
                                     MQTTPublishState_t publishState )
{
    MQTTStatus_t status = MQTTSuccess;
    MQTTPublishState_t newState = MQTTStateNull;
    int32_t sendResult = 0;
    uint8_t packetTypeByte = 0U;
    MQTTPubAckType_t packetType;
    MQTTFixedBuffer_t localBuffer;
    uint8_t pubAckPacket[ MQTT_PUBLISH_ACK_PACKET_SIZE ];

    localBuffer.pBuffer = pubAckPacket;
    localBuffer.size = MQTT_PUBLISH_ACK_PACKET_SIZE;

    assert( pContext != NULL );

    packetTypeByte = getAckTypeToSend( publishState );

    if( packetTypeByte != 0U )
    {
        packetType = getAckFromPacketType( packetTypeByte );

        status = MQTT_SerializeAck( &localBuffer,
                                    packetTypeByte,
                                    packetId );

        if( status == MQTTSuccess )
        {
            MQTT_PRE_SEND_HOOK( pContext );

            /* Here, we are not using the vector approach for efficiency. There is just one buffer
             * to be sent which can be achieved with a normal send call. */
            sendResult = sendBuffer( pContext,
                                     localBuffer.pBuffer,
                                     MQTT_PUBLISH_ACK_PACKET_SIZE );

            MQTT_POST_SEND_HOOK( pContext );
        }

        if( sendResult == ( int32_t ) MQTT_PUBLISH_ACK_PACKET_SIZE )
        {
            pContext->controlPacketSent = true;

            MQTT_PRE_STATE_UPDATE_HOOK( pContext );

            status = MQTT_UpdateStateAck( pContext,
                                          packetId,
                                          packetType,
                                          MQTT_SEND,
                                          &newState );

            MQTT_POST_STATE_UPDATE_HOOK( pContext );

            if( status != MQTTSuccess )
            {
                LogError( ( "Failed to update state of publish %hu.",
                            ( unsigned short ) packetId ) );
            }
        }
        else
        {
            LogError( ( "Failed to send ACK packet: PacketType=%02x, SentBytes=%ld, "
                        "PacketSize=%lu.",
                        ( unsigned int ) packetTypeByte, ( long int ) sendResult,
                        MQTT_PUBLISH_ACK_PACKET_SIZE ) );
            status = MQTTSendFailed;
        }
    }

    return status;
}


void mqttEventCallback( MQTTContext_t * pMqttContext,
                        MQTTPacketInfo_t * pPacketInfo,
                        MQTTDeserializedInfo_t * pDeserializedInfo )
{
    /* assert pMqttContext != NULL */
    if (pMqttContext == NULL)
    {
        ESP_LOGE("mqttEventCallback", "pMqttContext is null");
        return;
    }
    /* assert pPacketInfo != NULL */
    if (pPacketInfo == NULL)
    {
        ESP_LOGE("mqttEventCallback", "pPacketInfo is null");
        return;
    }
    /* assert pDeserializedInfo != NULL */
    if (pDeserializedInfo == NULL)
    {
        ESP_LOGE("mqttEventCallback", "pDeserializedInfo is null");
        return;
    }

    if ((pPacketInfo->type & 0xF0U) == MQTT_PACKET_TYPE_PUBLISH)
    {
        /* assert pDeserializedInfo->pPublishInfo != NULL */
        if (pDeserializedInfo->pPublishInfo == NULL)
        {
            ESP_LOGE("mqttEventCallback", "pDeserializedInfo->pPublishInfo is null");
            return;
        }

        if (!(strstr(pDeserializedInfo->pPublishInfo->pTopicName, "/streams/") && strstr(pDeserializedInfo->pPublishInfo->pTopicName, "/data/cbor")))
            mqtt_subscriptions_callbacks(pDeserializedInfo->pPublishInfo->pPayload, 
                                    pDeserializedInfo->pPublishInfo->payloadLength,
                                    pDeserializedInfo->pPublishInfo->pTopicName,
                                    pDeserializedInfo->pPublishInfo->topicNameLength
                                    );

        /* Handle incoming publish */
        SubscriptionManager_DispatchHandler( pMqttContext, pDeserializedInfo->pPublishInfo );
    }
    else
    {
        /* Handle other packets */
        switch (pPacketInfo->type)
        {
            case MQTT_PACKET_TYPE_SUBACK:
                ESP_LOGI("mqttEventCallback", "Received SUBACK");
                break;

            case MQTT_PACKET_TYPE_UNSUBACK:
                ESP_LOGI("mqttEventCallback", "Received UNSUBACK");
                break;

            case MQTT_PACKET_TYPE_PINGRESP:
                /* Nothing to be done from application as library handles PINGRESP */
                ESP_LOGW("mqttEventCallback", "PINGRESP should not be handled by the application"
                         " callback when using MQTT_ProcessLoop");
                break;

            case MQTT_PACKET_TYPE_PUBACK:
                ESP_LOGI("mqttEventCallback", "PUBACK received for packet id %u", pDeserializedInfo->packetIdentifier);
                break;

            /* Any other packet type is invalid. */
            default:
                ESP_LOGE("mqttEventCallback", "Unknown packet type received:(%02x)", pPacketInfo->type);
        }
    }
}

/*-----------------------------------------------------------*/

static uint32_t generateRandomNumber()
{
    return( rand() );
}

/*-----------------------------------------------------------*/

static int initializeMqtt( MQTTContext_t * pMqttContext,
                           NetworkContext_t * pNetworkContext )
{
    int returnStatus = EXIT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTBadParameter;
    MQTTFixedBuffer_t networkBuffer;
    TransportInterface_t transport;

    assert( pMqttContext != NULL );
    assert( pNetworkContext != NULL );

    /* Fill in TransportInterface send and receive function pointers.
     * TCP sockets are used to send and receive data
     * from network.  TLS over TCP channel is used as the transport
     * layer for the MQTT connection. Network context is SSL context
     * for OpenSSL.*/
    transport.pNetworkContext = pNetworkContext;
    transport.send = espTlsTransportSend;
    transport.recv = espTlsTransportRecv;
    transport.writev = NULL;

    /* Fill the values for network buffer. */
    networkBuffer.pBuffer = otaNetworkBuffer;
    networkBuffer.size = OTA_NETWORK_BUFFER_SIZE;

    /* Initialize MQTT library. */
    mqttStatus = MQTT_Init( pMqttContext,
                            &transport,
                            Clock_GetTimeMs,
                            mqttEventCallback,
                            &networkBuffer );

    if( mqttStatus != MQTTSuccess )
    {
        returnStatus = EXIT_FAILURE;
        LogError( ( "MQTT_Init failed: Status = %s.", MQTT_Status_strerror( mqttStatus ) ) );
    }
    else
    {
        mqttStatus = MQTT_InitStatefulQoS( pMqttContext,
                                           pOutgoingPublishRecords,
                                           OUTGOING_PUBLISH_RECORD_LEN,
                                           pIncomingPublishRecords,
                                           INCOMING_PUBLISH_RECORD_LEN );

        if( mqttStatus != MQTTSuccess )
        {
            returnStatus = EXIT_FAILURE;
            LogError( ( "MQTT_InitStatefulQoS failed: Status = %s.", MQTT_Status_strerror( mqttStatus ) ) );
        }
    }    

    return returnStatus;
}
/*-----------------------------------------------------------*/

static int connectToServerWithBackoffRetries( NetworkContext_t * pNetworkContext )
{
    int returnStatus = EXIT_SUCCESS;
    BackoffAlgorithmStatus_t backoffAlgStatus = BackoffAlgorithmSuccess;
    TlsTransportStatus_t tlsStatus = TLS_TRANSPORT_SUCCESS;
    BackoffAlgorithmContext_t reconnectParams;
    pNetworkContext->pcHostname = AWS_IOT_ENDPOINT;
    pNetworkContext->xPort = AWS_MQTT_PORT;
    pNetworkContext->pxTls = NULL;
    pNetworkContext->xTlsContextSemaphore = xSemaphoreCreateMutexStatic(&xTlsContextSemaphoreBuffer);

    pNetworkContext->disableSni = 0;
    uint16_t nextRetryBackOff;

    int smp_attempts = 0;

    /* Initialize credentials for establishing TLS session. */
    pNetworkContext->pcServerRootCAPem = root_cert_auth_pem_start;

    #ifdef CONFIG_EXAMPLE_USE_SECURE_ELEMENT

        pNetworkContext->pcClientCertPem = NULL;
        pNetworkContext->pcClientKeyPem = NULL;
        pNetworkContext->use_secure_element = true;

    #elif CONFIG_EXAMPLE_USE_DS_PERIPHERAL

        pNetworkContext->pcClientCertPem = client_cert_pem_start;
        pNetworkContext->pcClientKeyPem = NULL;
        #error "Populate the ds_data structure and remove this line"
        /* pNetworkContext->ds_data = DS_DATA; */
        /* The ds_data can be populated using the API's provided by esp_secure_cert_mgr */
    
    #else

        /* If #CLIENT_USERNAME is defined, username/password is used for authenticating
        * the client. */
        #ifndef CLIENT_USERNAME
            pNetworkContext->pcClientCertPem = client_cert_pem_start;
            pNetworkContext->pcClientKeyPem = client_key_pem_start;
        #endif

    #endif
    /* AWS IoT requires devices to send the Server Name Indication (SNI)
     * extension to the Transport Layer Security (TLS) protocol and provide
     * the complete endpoint address in the host_name field. Details about
     * SNI for AWS IoT can be found in the link below.
     * https://docs.aws.amazon.com/iot/latest/developerguide/transport-security.html */

    if (AWS_MQTT_PORT == 443)
    {
        static const char*  pcAlpnProtocols[] = { NULL, NULL };

        #ifdef CLIENT_USERNAME
            pcAlpnProtocols[0] = AWS_IOT_PASSWORD_ALPN;
        #else
            pcAlpnProtocols[0] = AWS_IOT_MQTT_ALPN;
        #endif

        pNetworkContext->pAlpnProtos = pcAlpnProtocols;
    }
    else
    {
        pNetworkContext->pAlpnProtos = NULL;
    }

    /* Initialize reconnect attempts and interval */
    BackoffAlgorithm_InitializeParams( &reconnectParams,
                                       CONNECTION_RETRY_BACKOFF_BASE_MS,
                                       CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS,
                                       CONNECTION_RETRY_MAX_ATTEMPTS );

    /* Attempt to connect to MQTT broker. If connection fails, retry after
     * a timeout. Timeout value will exponentially increase until maximum
     * attempts are reached.
     */
    do
    {
        /* Establish a TLS session with the MQTT broker. This example connects
         * to the MQTT broker as specified in AWS_IOT_ENDPOINT and AWS_MQTT_PORT
         * at the config header. */
        ESP_LOGI("connectToServerWithBackoffRetries", "Establishing a TLS session to AWS_IOT_ENDPOINT");

        tlsStatus = xTlsConnect(pNetworkContext);
        if (tlsStatus != TLS_TRANSPORT_SUCCESS)
        {
            /* Generate a random number and get back-off value (in milliseconds) for the next connection retry. */
            backoffAlgStatus = BackoffAlgorithm_GetNextBackoff( &reconnectParams, generateRandomNumber(), &nextRetryBackOff );
            smp_attempts++;

            if( backoffAlgStatus == BackoffAlgorithmRetriesExhausted )
            {
                ESP_LOGE("connectToServerWithBackoffRetries", "Connection to the broker failed, all attempts exhausted.");
                returnStatus = EXIT_FAILURE;
                esp_restart();
            }
            else if( backoffAlgStatus == BackoffAlgorithmSuccess )
            {
                ESP_LOGW("connectToServerWithBackoffRetries", "Connection to the broker failed. Retrying connection after %hu ms backoff.",
                                                                (unsigned short)nextRetryBackOff);
                Clock_SleepMs(nextRetryBackOff);
            }

            if (smp_attempts > 3)
            {
                ESP_LOGE("smp_attempts", "Tryed 3 times. Restarting.");
                esp_restart();
            }
        }
    } while ((tlsStatus != TLS_TRANSPORT_SUCCESS) && (backoffAlgStatus == BackoffAlgorithmSuccess));

    return returnStatus;
}

/*-----------------------------------------------------------*/

static int establishMqttSession( MQTTContext_t * pMqttContext )
{
    int returnStatus = EXIT_SUCCESS;
    MQTTStatus_t mqttStatus = MQTTBadParameter;
    MQTTConnectInfo_t connectInfo = { 0 };

    bool sessionPresent = false;

    assert( pMqttContext != NULL );

    connectInfo.cleanSession = true;

    /* The client identifier is used to uniquely identify this MQTT client to
     * the MQTT broker. In a production device the identifier can be something
     * unique, such as a device serial number. */
    connectInfo.pClientIdentifier = device_id;
    connectInfo.clientIdentifierLength = strlen(device_id);

    connectInfo.keepAliveSeconds = MQTT_KEEP_ALIVE_INTERVAL_SECONDS;

    #ifdef CLIENT_USERNAME
        connectInfo.pUserName = CLIENT_USERNAME_WITH_METRICS;
        connectInfo.userNameLength = strlen( CLIENT_USERNAME_WITH_METRICS );
        connectInfo.pPassword = CLIENT_PASSWORD;
        connectInfo.passwordLength = strlen( CLIENT_PASSWORD );
    #else
        connectInfo.pUserName = METRICS_STRING;
        connectInfo.userNameLength = METRICS_STRING_LENGTH;
        /* Password for authentication is not used. */
        connectInfo.pPassword = NULL;
        connectInfo.passwordLength = 0U;
    #endif /* ifdef CLIENT_USERNAME */

    if( pthread_mutex_lock( &mqttMutex ) == 0 )
    {
        /* Send MQTT CONNECT packet to broker. */
        mqttStatus = MQTT_Connect( pMqttContext, &connectInfo, NULL, CONNACK_RECV_TIMEOUT_MS, &sessionPresent );
 
        pthread_mutex_unlock( &mqttMutex );
    }
    else
    {
        LogError( ( "Failed to acquire mutex for executing MQTT_Connect"
                    ",errno=%s",
                    strerror( errno ) ) );
    }

    if( mqttStatus != MQTTSuccess )
    {
        returnStatus = EXIT_FAILURE;
        LogError( ( "Connection with MQTT broker failed with status %s.",
                    MQTT_Status_strerror( mqttStatus ) ) );
    }
    else
    {
        LogInfo( ( "MQTT connection successfully established with broker.\n\n" ) );
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static int establishConnection( void )
{
    int returnStatus = EXIT_FAILURE;

    /* Attempt to connect to the MQTT broker. If connection fails, retry after
     * a timeout. Timeout value will be exponentially increased till the maximum
     * attempts are reached or maximum timeout value is reached. The function
     * returns EXIT_FAILURE if the TCP connection cannot be established to
     * broker after configured number of attempts. */
    returnStatus = connectToServerWithBackoffRetries( &networkContext );

    if( returnStatus != EXIT_SUCCESS )
    {
        /* Log error to indicate connection failure. */
        color_printf(COLOR_PRINT_RED, "Failed to connect to MQTT broker xxxxxxxxxxxxxxxx-ats.iot.us-east-2.amazonaws.com");
    }
    else
    {
        /* Establish MQTT session on top of TCP+TLS connection. */
        color_printf(COLOR_PRINT_PURPLE, "Creating MQTT connection to xxxxxxxxxxxxxxxx-ats.iot.us-east-2.amazonaws.com");

        /* Sends an MQTT Connect packet using the established TLS session,
        * then waits for connection acknowledgment (CONNACK) packet. */
        returnStatus = establishMqttSession( &smp_mqttContext );

        if( returnStatus != EXIT_SUCCESS )
        {
            color_printf(COLOR_PRINT_RED, "Failed creating MQTT connection to xxxxxxxxxxxxxxxx-ats.iot.us-east-2.amazonaws.com");
        }
        else
        {
            color_printf(COLOR_PRINT_PURPLE, "Success creating MQTT connection to xxxxxxxxxxxxxxxx-ats.iot.us-east-2.amazonaws.com");
            mqttSessionEstablished = true;
        }
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static void disconnect( void )
{
    /* Disconnect from broker. */
    ESP_LOGW("disconnect", "Disconnecting the MQTT connection with AWS_IOT_ENDPOINT");

    if (mqttSessionEstablished == true)
    {
        if (pthread_mutex_lock( &mqttMutex ) == 0)
        {
            /* Disconnect MQTT session. */
            MQTT_Disconnect(&smp_mqttContext);

            /* Clear the mqtt session flag. */
            mqttSessionEstablished = false;

            pthread_mutex_unlock(&mqttMutex);
        }
        else
        {
            ESP_LOGE("disconnect", "Failed to acquire mutex to execute MQTT_Disconnect, errno=%s", strerror(errno));
        }
    }
    else
    {
        ESP_LOGE("disconnect", "MQTT already disconnected");
    }

    /* End TLS session, then close TCP connection. */
    (void)xTlsDisconnect(&networkContext);
}

/*-----------------------------------------------------------*/

void registerSubscriptionManagerCallback( const char*  pTopicFilter,
                                                 uint16_t topicFilterLength )
{
    bool isMatch = false;
    MQTTStatus_t mqttStatus = MQTTSuccess;
    SubscriptionManagerStatus_t subscriptionStatus = SUBSCRIPTION_MANAGER_SUCCESS;

    uint16_t index = 0U;

    /* For suppressing compiler-warning: unused variable. */
    ( void ) mqttStatus;

    /* Lookup table for OTA message string. */
    static const char*  const pWildCardTopicFilters[] =
    {
        OTA_TOPIC_PREFIX OTA_TOPIC_JOBS "/#",
        OTA_TOPIC_PREFIX OTA_TOPIC_STREAM "/#"
    };

    /* Match the input topic filter against the wild-card pattern of topics filters
    * relevant for the OTA Update service to determine the type of topic filter. */
    for( ; index < 2; index++ )
    {
        mqttStatus = MQTT_MatchTopic( pTopicFilter,
                                      topicFilterLength,
                                      pWildCardTopicFilters[ index ],
                                      strlen( pWildCardTopicFilters[ index ] ),
                                      &isMatch );
        assert( mqttStatus == MQTTSuccess );

        if( isMatch )
        {
            /* Register callback to subscription manager. */
            subscriptionStatus = SubscriptionManager_RegisterCallback( pWildCardTopicFilters[ index ],
                                                                       strlen( pWildCardTopicFilters[ index ] ),
                                                                       otaMessageCallback[ index ] );
            if( subscriptionStatus != SUBSCRIPTION_MANAGER_SUCCESS )
            {
                LogWarn( ( "Failed to register a callback to subscription manager with error = %d.",
                           subscriptionStatus ) );
            }

            break;
        }
    }
}

static int waitForPacketAck( MQTTContext_t * pMqttContext,
                             uint16_t usPacketIdentifier,
                             uint32_t ulTimeout )
{
    uint32_t ulMqttProcessLoopEntryTime;
    uint32_t ulMqttProcessLoopTimeoutTime;
    uint32_t ulCurrentTime;

    MQTTStatus_t eMqttStatus = MQTTSuccess;
    int returnStatus = EXIT_FAILURE;

    /* Reset the ACK packet identifier being received. */
    globalAckPacketIdentifier = 0U;

    ulCurrentTime = pMqttContext->getTime();
    ulMqttProcessLoopEntryTime = ulCurrentTime;
    ulMqttProcessLoopTimeoutTime = ulCurrentTime + ulTimeout;

    /* Call MQTT_ProcessLoop multiple times until the expected packet ACK
     * is received, a timeout happens, or MQTT_ProcessLoop fails. */
    while( ( globalAckPacketIdentifier != usPacketIdentifier ) &&
           ( ulCurrentTime < ulMqttProcessLoopTimeoutTime ) &&
           ( eMqttStatus == MQTTSuccess || eMqttStatus == MQTTNeedMoreBytes ) )
    {
        /* Event callback will set #globalAckPacketIdentifier when receiving
         * appropriate packet. */
        eMqttStatus = MQTT_ProcessLoop( pMqttContext );
        ulCurrentTime = pMqttContext->getTime();
    }

    if( ( ( eMqttStatus != MQTTSuccess ) && ( eMqttStatus != MQTTNeedMoreBytes ) ) ||
        ( globalAckPacketIdentifier != usPacketIdentifier ) )
    {
        ESP_LOGE("waitForPacketAck", "MQTT_ProcessLoop failed to receive ACK packet: Expected ACK Packet ID=%08"PRIx16", LoopDuration=%"PRIu32", Status=%s",
                                        usPacketIdentifier,
                                        (ulCurrentTime - ulMqttProcessLoopEntryTime),
                                        MQTT_Status_strerror(eMqttStatus));
    }
    else
    {
        returnStatus = EXIT_SUCCESS;
    }

    return returnStatus;
}
int32_t UnsubscribeFromTopic( const char*  pTopicFilter, uint16_t topicFilterLength )
{
    #define MQTT_PROCESS_LOOP_TIMEOUT_MS        ( 1500U )
    int returnStatus = EXIT_SUCCESS;
    MQTTStatus_t mqttStatus;
    MQTTContext_t * pMqttContext = &smp_mqttContext;
    MQTTSubscribeInfo_t pSubscriptionList[ 1 ];

    assert( pMqttContext != NULL );
    assert( pTopicFilter != NULL );
    assert( topicFilterLength > 0 );

    /* Start with everything at 0. */
    ( void ) memset( ( void * ) pSubscriptionList, 0x00, sizeof( pSubscriptionList ) );

    /* This example subscribes to only one topic and uses QOS1. */
    pSubscriptionList[ 0 ].qos = MQTTQoS1;
    pSubscriptionList[ 0 ].pTopicFilter = pTopicFilter;
    pSubscriptionList[ 0 ].topicFilterLength = topicFilterLength;

    /* Generate packet identifier for the UNSUBSCRIBE packet. */
    globalUnsubscribePacketIdentifier = MQTT_GetPacketId( pMqttContext );

    /* Send UNSUBSCRIBE packet. */
    mqttStatus = MQTT_Unsubscribe( pMqttContext,
                                   pSubscriptionList,
                                   sizeof( pSubscriptionList ) / sizeof( MQTTSubscribeInfo_t ),
                                   globalUnsubscribePacketIdentifier );

    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "Failed to send UNSUBSCRIBE packet to broker with error = %u.",
                    mqttStatus ) );
        returnStatus = EXIT_FAILURE;
    }
    else
    {
        LogInfo( ( "UNSUBSCRIBE sent topic %.*s to broker.",
                   topicFilterLength,
                   pTopicFilter ) );

        returnStatus = waitForPacketAck( pMqttContext,
                                         globalUnsubscribePacketIdentifier,
                                         MQTT_PROCESS_LOOP_TIMEOUT_MS );
    }

    return returnStatus;
}

static OtaMqttStatus_t mqttSubscribe( const char*  pTopicFilter,
                                      uint16_t topicFilterLength,
                                      uint8_t qos )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;

    MQTTStatus_t mqttStatus = MQTTBadParameter;
    MQTTContext_t * pMqttContext = &smp_mqttContext;
    int num_topics = 0;

    char* topic_to_sign = calloc(128,sizeof(char));    

    assert( pMqttContext != NULL );
    assert( pTopicFilter != NULL );
    assert( topicFilterLength > 0 );

    ( void ) qos;

    char* list_subscription_topics[3] = {0};
    asprintf(&topic_to_sign, "S/%s/sys/#", device_id);
    list_subscription_topics[num_topics] = topic_to_sign;
    num_topics++;

    asprintf(&topic_to_sign, "S/%s/cmd/#", device_id);
    list_subscription_topics[num_topics] = topic_to_sign;
    num_topics++;

    list_subscription_topics[num_topics] = pTopicFilter;
    num_topics++;


    MQTTSubscribeInfo_t pSubscriptionList[ num_topics ];
    ( void ) memset( ( void * ) pSubscriptionList, 0x00, sizeof( pSubscriptionList ) );

    for (int x = 0; x < num_topics; x++)
    {
        pSubscriptionList[x].qos = MQTTQoS0;
        pSubscriptionList[x].pTopicFilter = list_subscription_topics[x];
        pSubscriptionList[x].topicFilterLength = strlen(list_subscription_topics[x]);
        printf("list_subscription_topics[%d]: %s\n", x, list_subscription_topics[x]); // for debug
    }

    if( pthread_mutex_lock( &mqttMutex ) == 0 )
    {
        /* Send SUBSCRIBE packet. */
        {
            mqttStatus = MQTT_Subscribe( pMqttContext,
                                        pSubscriptionList,
                                        sizeof( pSubscriptionList ) / sizeof( MQTTSubscribeInfo_t ),
                                        MQTT_GetPacketId( pMqttContext ) );
        }
        color_printf(LOCAL_COLOR_PRINT_INFO, "Necessary topics were signed");  
        pthread_mutex_unlock( &mqttMutex );
    }
    else
    {
        LogError( ( "Failed to acquire mqtt mutex for executing MQTT_Subscribe"
                    ",errno=%s",
                    strerror( errno ) ) );
    }

    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "Failed to send SUBSCRIBE packet to broker with error = %u.",
                    mqttStatus ) );

        otaRet = OtaMqttSubscribeFailed;
    }
    else
    {
        LogInfo( ( "SUBSCRIBE topic %.*s to broker.\n\n",
                   topicFilterLength,
                   pTopicFilter ) );

        registerSubscriptionManagerCallback( pTopicFilter, topicFilterLength );
    }

    vPortFree(topic_to_sign);

    return otaRet;
}
/*-----------------------------------------------------------*/

static OtaMqttStatus_t mqttPublish( const char* const pacTopic,
                                    uint16_t topicLen,
                                    const char*  pMsg,
                                    uint32_t msgSize,
                                    uint8_t qos )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;

    MQTTStatus_t mqttStatus = MQTTBadParameter;
    MQTTPublishInfo_t publishInfo = { 0 };
    MQTTContext_t * pMqttContext = &smp_mqttContext;
    BackoffAlgorithmStatus_t backoffAlgStatus = BackoffAlgorithmSuccess;
    BackoffAlgorithmContext_t reconnectParams;
    uint16_t nextRetryBackOff;

    /* Initialize reconnect attempts and interval */
    BackoffAlgorithm_InitializeParams( &reconnectParams,
                                    CONNECTION_RETRY_BACKOFF_BASE_MS,
                                    CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS,
                                    MQTT_PUBLISH_RETRY_MAX_ATTEMPS );

    /* Set the required publish parameters. */
    publishInfo.pTopicName = pacTopic;
    publishInfo.topicNameLength = topicLen;
    publishInfo.qos = qos;
    publishInfo.pPayload = pMsg;
    publishInfo.payloadLength = msgSize;

    if( pthread_mutex_lock( &mqttMutex ) == 0 )
    {
        int count_retrying_connection = 0;
        do
    {
            mqttStatus = MQTT_Publish( pMqttContext,
                                    &publishInfo,
                                    MQTT_GetPacketId( pMqttContext ) );

            if( qos == 1 )
            {
                /* Loop to receive packet from transport interface. */
                mqttStatus = MQTT_ReceiveLoop( &smp_mqttContext );
            }

            if (count_retrying_connection > 5)
                break;

            if( mqttStatus != MQTTSuccess )
            {
                /* Generate a random number and get back-off value (in milliseconds) for the next connection retry. */
                backoffAlgStatus = BackoffAlgorithm_GetNextBackoff( &reconnectParams, generateRandomNumber(), &nextRetryBackOff );

                if( backoffAlgStatus == BackoffAlgorithmRetriesExhausted )
                {
                    LogError( ( "Publish failed, all attempts exhausted." ) );
                }
                else if( backoffAlgStatus == BackoffAlgorithmSuccess )
                {
                    LogWarn( ( "Publish failed. Retrying connection "
                            "after %hu ms backoff.",
                            ( unsigned short ) nextRetryBackOff ) );
                    vTaskDelay( nextRetryBackOff/portTICK_PERIOD_MS );
                }
                count_retrying_connection++;
            }
        } while( ( mqttStatus != MQTTSuccess ) && ( backoffAlgStatus == BackoffAlgorithmSuccess ) );

        pthread_mutex_unlock( &mqttMutex );
    }
    else
    {
        LogError( ( "Failed to acquire mqtt mutex for executing MQTT_Publish"
                    ",errno=%s",
                    strerror( errno ) ) );
    }
    

    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "Failed to send PUBLISH packet to broker with error = %u.", mqttStatus ) );

        otaRet = OtaMqttPublishFailed;
    }
    else
    {
        LogInfo( ( "Sent PUBLISH packet to broker %.*s to broker.\n\n",
                   topicLen,
                   pacTopic ) );
    }

    return otaRet;
}

/*-----------------------------------------------------------*/

static OtaMqttStatus_t mqttUnsubscribe( const char*  pTopicFilter,
                                        uint16_t topicFilterLength,
                                        uint8_t qos )
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;
    MQTTStatus_t mqttStatus = MQTTBadParameter;

    MQTTSubscribeInfo_t pSubscriptionList[ 1 ];
    MQTTContext_t * pMqttContext = &smp_mqttContext;

    ( void ) qos;

    /* Start with everything at 0. */
    ( void ) memset( ( void * ) pSubscriptionList, 0x00, sizeof( pSubscriptionList ) );

    /* Set the topic and topic length. */
    pSubscriptionList[ 0 ].pTopicFilter = pTopicFilter;
    pSubscriptionList[ 0 ].topicFilterLength = topicFilterLength;

    if( pthread_mutex_lock( &mqttMutex ) == 0 )
    {
LogInfo( ( "mqttUnsubscribe mqttMutex pegou.\n\n" ) );
        /* Send UNSUBSCRIBE packet. */
        mqttStatus = MQTT_Unsubscribe( pMqttContext,
                                       pSubscriptionList,
                                       sizeof( pSubscriptionList ) / sizeof( MQTTSubscribeInfo_t ),
                                       MQTT_GetPacketId( pMqttContext ) );

        pthread_mutex_unlock( &mqttMutex );
LogInfo( ( "mqttUnsubscribe mqttMutex largou.\n\n" ) );
    }
    else
    {
        LogError( ( "Failed to acquire mutex for executing MQTT_Unsubscribe"
                    ",errno=%s",
                    strerror( errno ) ) );
    }

    if( mqttStatus != MQTTSuccess )
    {
        LogError( ( "Failed to send UNSUBSCRIBE packet to broker with error = %u.",
                    mqttStatus ) );

        otaRet = OtaMqttUnsubscribeFailed;
    }
    else
    {
        LogInfo( ( "UNSUBSCRIBE topic %.*s to broker.\n\n",
                   topicFilterLength,
                   pTopicFilter ) );
    }

    return otaRet;
}

/*-----------------------------------------------------------*/

static void setOtaInterfaces( OtaInterfaces_t * pOtaInterfaces )
{
    /* Initialize OTA library OS Interface. */
    pOtaInterfaces->os.event.init = OtaInitEvent_FreeRTOS;
    pOtaInterfaces->os.event.send = OtaSendEvent_FreeRTOS;
    pOtaInterfaces->os.event.recv = OtaReceiveEvent_FreeRTOS;
    pOtaInterfaces->os.event.deinit = OtaDeinitEvent_FreeRTOS;
    pOtaInterfaces->os.timer.start = OtaStartTimer_FreeRTOS;
    pOtaInterfaces->os.timer.stop = OtaStopTimer_FreeRTOS;
    pOtaInterfaces->os.timer.delete = OtaDeleteTimer_FreeRTOS;
    pOtaInterfaces->os.mem.malloc = Malloc_FreeRTOS;
    pOtaInterfaces->os.mem.free = Free_FreeRTOS;

    /* Initialize the OTA library MQTT Interface.*/
    #if GSM_ENABLED
    // pOtaInterfaces->mqtt.subscribe = sub_topic_GSM_OTA;
    // pOtaInterfaces->mqtt.publish = mqtt_publish_GSM_OTA;
    pOtaInterfaces->mqtt.subscribe = mqttSubscribe;
    pOtaInterfaces->mqtt.publish = mqttPublish;
    pOtaInterfaces->mqtt.unsubscribe = mqttUnsubscribe;
    #else
    pOtaInterfaces->mqtt.subscribe = mqttSubscribe;
    pOtaInterfaces->mqtt.publish = mqttPublish;
    pOtaInterfaces->mqtt.unsubscribe = mqttUnsubscribe;
    #endif

    /* Initialize the OTA library PAL Interface.*/
    pOtaInterfaces->pal.getPlatformImageState = otaPal_GetPlatformImageState;
    pOtaInterfaces->pal.setPlatformImageState = otaPal_SetPlatformImageState;
    pOtaInterfaces->pal.writeBlock = otaPal_WriteBlock;
    pOtaInterfaces->pal.activate = otaPal_ActivateNewImage;
    pOtaInterfaces->pal.closeFile = otaPal_CloseFile;
    pOtaInterfaces->pal.reset = otaPal_ResetDevice;
    pOtaInterfaces->pal.abort = otaPal_Abort;
    pOtaInterfaces->pal.createFile = otaPal_CreateFileForRx;
}

/*-----------------------------------------------------------*/

static void * otaThread( void * pParam )
{
    /* Calling OTA agent task. */
    OTA_EventProcessingTask( pParam );
    LogInfo( ( "OTA Agent stopped." ) );
    return NULL;
}
/*-----------------------------------------------------------*/


int startMain( void )
{
    /* Status indicating a successful or not. */
    int returnStatus = EXIT_SUCCESS;

    /* coreMQTT library return status. */
    MQTTStatus_t mqttStatus = MQTTBadParameter;

    /* OTA library return status. */
    OtaErr_t otaRet = OtaErrNone;

    /* OTA Agent state returned from calling OTA_GetAgentState.*/
    OtaState_t state = OtaAgentStateStopped;

    /* OTA event message used for sending event to OTA Agent.*/
    OtaEventMsg_t eventMsg = { 0 };

    /* OTA library packet statistics per job.*/
    OtaAgentStatistics_t otaStatistics = { 0 };

    /* OTA Agent thread handle.*/
    pthread_t threadHandle;

    /* Status return from call to pthread_join. */
    int returnJoin = 0;

    /* OTA interface context required for library interface functions.*/
    OtaInterfaces_t otaInterfaces;

    /* Maximum time to wait for the OTA agent to get suspended. */
    int16_t suspendTimeout;

    /* Set OTA Library interfaces.*/
    setOtaInterfaces( &otaInterfaces );

    /* Set OTA Code Signing Certificate */
    if( !otaPal_SetCodeSigningCertificate( pcAwsCodeSigningCertPem ) )
    {
         ESP_LOGE("startMain", "Failed to allocate memory for Code Signing Certificate");
         returnStatus = EXIT_FAILURE;
    }

    /****************************** Init OTA Library. ******************************/
    if( returnStatus == EXIT_SUCCESS )
    {
        if( ( otaRet = OTA_Init( &otaBuffer,
                                 &otaInterfaces,
                                 ( const uint8_t * ) ( device_id ),
                                 otaAppCallback ) ) != OtaErrNone )
        {
            ESP_LOGE("startMain", "Failed to initialize OTA Agent, exiting = %u.", otaRet);

            returnStatus = EXIT_FAILURE;
        }
    }

    /****************************** Create OTA Task. ******************************/

    if( returnStatus == EXIT_SUCCESS )
    {
        if( pthread_create( &threadHandle, NULL, otaThread, NULL ) != 0 )
        {
            ESP_LOGE("startMain", "Failed to create OTA thread errno=%s", strerror(errno));

            returnStatus = EXIT_FAILURE;
        }
    }

    /****************************** OTA loop. *************************************/
    if( returnStatus == EXIT_SUCCESS )
    {
        /* Wait till OTA library is stopped, output statistics for currently running
         * OTA job */
  
        while( ( state = OTA_GetState() ) != OtaAgentStateStopped )
        {
            vTaskDelay(pdMS_TO_TICKS(50));

            if( mqttSessionEstablished != true )
            {
                connected_by_wifi = 0;

                /* Connect to MQTT broker and create MQTT connection. */
                if( EXIT_SUCCESS == establishConnection() )
                {
                    /* Check if OTA process was suspended and resume if required. */
                    if( state == OtaAgentStateSuspended )
                    {
                        /* Resume OTA operations. */
                        OTA_Resume();
                    }
                    else
                    {
                        /* Send start event to OTA Agent.*/
                        eventMsg.eventId = OtaAgentEventStart;
                        OTA_SignalEvent( &eventMsg );
                    }
                }
            }

            if( mqttSessionEstablished == true )
            {
                connected_by_wifi = 1;

                /* Acquire the mqtt mutex lock. */
                if( pthread_mutex_lock( &mqttMutex ) == 0 )
                {
                    mqttStatus = MQTT_ProcessLoop( &smp_mqttContext );

                    pthread_mutex_unlock( &mqttMutex );
                }
                else
                {
                    LogError( ( "Failed to acquire mutex to execute process loop"
                                ",errno=%s",
                                strerror( errno ) ) );
                }

                if( ( mqttStatus == MQTTSuccess ) || ( mqttStatus == MQTTNeedMoreBytes ) )
                {

                    /* Get OTA statistics for currently executing job. */
                    OTA_GetStatistics( &otaStatistics );
                    // LogInfo( ( " Received: %"PRIu32"   Queued: %"PRIu32"   Processed: %"PRIu32"   Dropped: %"PRIu32"",
                    //         otaStatistics.otaPacketsReceived,
                    //         otaStatistics.otaPacketsQueued,
                    //         otaStatistics.otaPacketsProcessed,
                    //         otaStatistics.otaPacketsDropped ) );

                    if (otaStatistics.otaPacketsReceived > 0) //experimental
                    {
                        // ESP_LOGE("startMain", "otaStatistics.otaPacketsReceived: %d",otaStatistics.otaPacketsReceived);
                        // vTaskSuspend( xHandle );
                    }
                        
                    // /* Delay to allow data to buffer for MQTT_ProcessLoop. */
                    vTaskDelay(pdMS_TO_TICKS(15));
                }
                else
                {
                    ESP_LOGE("startMain", "MQTT_ProcessLoop returned with status = %s.",
                                MQTT_Status_strerror(mqttStatus));

                    /* Disconnect from broker and close connection. */
                    disconnect();

                    /* Suspend OTA operations. */
                    otaRet = OTA_Suspend();

                    if( otaRet == OtaErrNone )
                    {
                        suspendTimeout = OTA_SUSPEND_TIMEOUT_MS;

                        while( ( ( state = OTA_GetState() ) != OtaAgentStateSuspended ) && ( suspendTimeout > 0 ) )
                        {
                            /* Wait for OTA Library state to suspend */
                            Clock_SleepMs( OTA_EXAMPLE_TASK_DELAY_MS );
                            suspendTimeout -= OTA_EXAMPLE_TASK_DELAY_MS;
                        }
                    }
                    else
                    {
                        ESP_LOGE("startMain", "OTA failed to suspend. StatusCode=%d.", otaRet);
                    }
                }
            }
        }
        // }
    }

    /****************************** Wait for OTA Thread. ******************************/

    if( returnStatus == EXIT_SUCCESS )
    {
        returnJoin = pthread_join( threadHandle, NULL );

        if( returnJoin != 0 )
        {
            ESP_LOGE("startMain", "Failed to join thread,error code = %d",returnJoin);

            returnStatus = EXIT_FAILURE;
        }
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/
int smp_main( int argc, char ** argv )
{
    ( void ) argc;
    ( void ) argv;

    /* Return error status. */
    int returnStatus = EXIT_SUCCESS;

    /* Semaphore initialization flag. */
    bool bufferSemInitialized = false;
    bool ackSemInitialized = false;
    bool mqttMutexInitialized = false;

    /* Maximum time in milliseconds to wait before exiting demo . */
    int16_t waitTimeoutMs = OTA_EXIT_TIMEOUT_MS;

    /* Initialize semaphore for buffer operations. */
    if( osi_sem_new( &bufferSemaphore, 0x7FFFU, 1 ) != 0 )
    {
        LogError( ( "Failed to initialize buffer semaphore"
                    ",errno=%s",
                    strerror( errno ) ) );

        returnStatus = EXIT_FAILURE;
    }
    else
    {
        bufferSemInitialized = true;
    }

    /* Initialize semaphore for ack. */
    if( osi_sem_new( &ackSemaphore, 0x7FFFU, 0 ) != 0 )
    {
        LogError( ( "Failed to initialize ack semaphore"
                    ",errno=%s",
                    strerror( errno ) ) );

        returnStatus = EXIT_FAILURE;
    }
    else
    {
        ackSemInitialized = true;
    }

    /* Initialize mutex for coreMQTT APIs. */
    if( pthread_mutex_init( &mqttMutex, NULL ) != 0 )
    {
        LogError( ( "Failed to initialize mutex for mqtt apis"
                    ",errno=%s",
                    strerror( errno ) ) );

        returnStatus = EXIT_FAILURE;
    }
    else
    {
        mqttMutexInitialized = true;
    }
    if( returnStatus == EXIT_SUCCESS )
    {
        /* Initialize MQTT library. Initialization of the MQTT library needs to be
         * done only once in this demo. */
        returnStatus = initializeMqtt( &smp_mqttContext, &networkContext );
    }

    if( returnStatus == EXIT_SUCCESS )
    {
        /* Start OTA demo. */
        returnStatus = startMain();
    }

    /* Disconnect from broker and close connection. */
    disconnect();

    if( bufferSemInitialized == true )
    {
        /* Cleanup semaphore created for buffer operations. */
        if( osi_sem_free( &bufferSemaphore ) != 0 )
        {
            LogError( ( "Failed to destroy buffer semaphore"
                        ",errno=%s",
                        strerror( errno ) ) );

            returnStatus = EXIT_FAILURE;
        }
    }

    if( ackSemInitialized == true )
    {
        /* Cleanup semaphore created for ack. */
        if( osi_sem_free( &ackSemaphore ) != 0 )
        {
            LogError( ( "Failed to destroy ack semaphore"
                        ",errno=%s",
                        strerror( errno ) ) );

            returnStatus = EXIT_FAILURE;
        }
    }

    if( mqttMutexInitialized == true )
    {
        LogInfo( ( "mqttMutex mqttMutexInitialized.\n\n" ) );
        /* Cleanup mutex created for MQTT operations. */
        if( pthread_mutex_destroy( &mqttMutex ) != 0 )
        {
            LogError( ( "Failed to destroy mutex for mqtt apis"
                        ",errno=%s",
                        strerror( errno ) ) );

            returnStatus = EXIT_FAILURE;
        }
    }

    /* Wait and log message before exiting demo. */
    while( waitTimeoutMs > 0 )
    {
        Clock_SleepMs( OTA_EXAMPLE_TASK_DELAY_MS );
        waitTimeoutMs -= OTA_EXAMPLE_TASK_DELAY_MS;

        LogError( ( "Exiting demo in %d sec", waitTimeoutMs / 1000 ) );
    }

    return returnStatus;
}
