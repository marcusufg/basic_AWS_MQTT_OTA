#include <string.h>
#include <assert.h>

#include "mqtt_subscription_manager.h"


typedef struct SubscriptionManagerRecord
{
    const char * pTopicFilter;
    uint16_t topicFilterLength;
    SubscriptionManagerCallback_t callback;
} SubscriptionManagerRecord_t;

#ifndef MAX_SUBSCRIPTION_CALLBACK_RECORDS
    #define MAX_SUBSCRIPTION_CALLBACK_RECORDS    8
#endif

/**
 * @brief The registry to store records of topic filters and their subscription callbacks.
 */
static SubscriptionManagerRecord_t callbackRecordList[ MAX_SUBSCRIPTION_CALLBACK_RECORDS ] = { 0 };

/*-----------------------------------------------------------*/

void SubscriptionManager_DispatchHandler( MQTTContext_t * pContext,
                                          MQTTPublishInfo_t * pPublishInfo )
{
    bool matchStatus = false;
    size_t listIndex = 0u;

    assert( pPublishInfo != NULL );
    assert( pContext != NULL );

    /* Iterate through record list to find matching topics, and invoke their callbacks. */
    for( listIndex = 0; listIndex < MAX_SUBSCRIPTION_CALLBACK_RECORDS; listIndex++ )
    {
        if( ( callbackRecordList[ listIndex ].pTopicFilter != NULL ) &&
            ( MQTT_MatchTopic( pPublishInfo->pTopicName,
                               pPublishInfo->topicNameLength,
                               callbackRecordList[ listIndex ].pTopicFilter,
                               callbackRecordList[ listIndex ].topicFilterLength,
                               &matchStatus ) == MQTTSuccess ) &&
            ( matchStatus == true ) )
        {
            LogInfo( ( "Invoking subscription callback of matching topic filter: "
                       "TopicFilter=%.*s, TopicName=%.*s",
                       callbackRecordList[ listIndex ].topicFilterLength,
                       callbackRecordList[ listIndex ].pTopicFilter,
                       pPublishInfo->topicNameLength,
                       pPublishInfo->pTopicName ) );

            /* Invoke the callback associated with the record as the topics match. */
            callbackRecordList[ listIndex ].callback( pContext, pPublishInfo );
        }
    }
}

/*-----------------------------------------------------------*/

SubscriptionManagerStatus_t SubscriptionManager_RegisterCallback( const char * pTopicFilter,
                                                                  uint16_t topicFilterLength,
                                                                  SubscriptionManagerCallback_t callback )
{
    assert( pTopicFilter != NULL );
    assert( topicFilterLength != 0 );
    assert( callback != NULL );
printf("SubscriptionManager_RegisterCallback pTopicFilter: %s\n",pTopicFilter);
printf("SubscriptionManager_RegisterCallback topicFilterLength: %d\n",topicFilterLength);

    SubscriptionManagerStatus_t returnStatus;
    size_t availableIndex = MAX_SUBSCRIPTION_CALLBACK_RECORDS;
    bool recordExists = false;
    size_t index = 0u;

    /* Search for the first available spot in the list to store the record, and also check if
     * a record for the topic filter already exists. */
    while( ( recordExists == false ) && ( index < MAX_SUBSCRIPTION_CALLBACK_RECORDS ) )
    {
        /* Check if the index represents an empty spot in the registry. If we had already
         * found an empty spot in the list, we will not update it. */
        if( ( availableIndex == MAX_SUBSCRIPTION_CALLBACK_RECORDS ) &&
            ( callbackRecordList[ index ].pTopicFilter == NULL ) )
        {
            availableIndex = index;
        }

        /* Check if the current record's topic filter in the registry matches the topic filter
         * we are trying to register. */
        else if( ( callbackRecordList[ index ].topicFilterLength == topicFilterLength ) &&
                 ( strncmp( pTopicFilter, callbackRecordList[ index ].pTopicFilter, topicFilterLength )
                   == 0 ) )
        {
            recordExists = true;
        }

        index++;
    }

    if( recordExists == true )
    {
        /* The record for the topic filter already exists. */
        LogError( ( "Failed to register callback: Record for topic filter already exists: TopicFilter=%.*s",
                    topicFilterLength,
                    pTopicFilter ) );

        returnStatus = SUBSCRIPTION_MANAGER_RECORD_EXISTS;
    }
    else if( availableIndex == MAX_SUBSCRIPTION_CALLBACK_RECORDS )
    {
        /* The registry is full. */
        LogError( ( "Unable to register callback: Registry list is full: TopicFilter=%.*s, MaxRegistrySize=%u",
                    topicFilterLength,
                    pTopicFilter,
                    MAX_SUBSCRIPTION_CALLBACK_RECORDS ) );

        returnStatus = SUBSCRIPTION_MANAGER_REGISTRY_FULL;
    }
    else
    {
        callbackRecordList[ availableIndex ].pTopicFilter = pTopicFilter;
        callbackRecordList[ availableIndex ].topicFilterLength = topicFilterLength;
        callbackRecordList[ availableIndex ].callback = callback;

        returnStatus = SUBSCRIPTION_MANAGER_SUCCESS;

        LogDebug( ( "Added callback to registry: TopicFilter=%.*s",
                    topicFilterLength,
                    pTopicFilter ) );
    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

void SubscriptionManager_RemoveCallback( const char * pTopicFilter,
                                         uint16_t topicFilterLength )
{
    assert( pTopicFilter != NULL );
    assert( topicFilterLength != 0 );

    size_t index;
    SubscriptionManagerRecord_t * pRecord = NULL;

    /* Iterate through the records list to find the matching record. */
    for( index = 0; index < MAX_SUBSCRIPTION_CALLBACK_RECORDS; index++ )
    {
        pRecord = &callbackRecordList[ index ];

        /* Only match the non-empty records. */
        if( pRecord->pTopicFilter != NULL )
        {
            if( ( topicFilterLength == pRecord->topicFilterLength ) &&
                ( strncmp( pTopicFilter, pRecord->pTopicFilter, topicFilterLength ) == 0 ) )
            {
                break;
            }
        }
    }

    /* Delete the record by clearing the found entry in the records list. */
    if( index < MAX_SUBSCRIPTION_CALLBACK_RECORDS )
    {
        pRecord->pTopicFilter = NULL;
        pRecord->topicFilterLength = 0u;
        pRecord->callback = NULL;

        LogDebug( ( "Deleted callback record for topic filter: TopicFilter=%.*s",
                    topicFilterLength,
                    pTopicFilter ) );
    }
    else
    {
        LogWarn( ( "Attempted to remove callback for un-registered topic filter: TopicFilter=%.*s",
                   topicFilterLength,
                   pTopicFilter ) );
    }
}
/*-----------------------------------------------------------*/
