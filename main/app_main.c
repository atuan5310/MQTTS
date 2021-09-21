/* MQTT Mutual Authentication Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "esp_sntp.h"
#include <time.h>
#include <sys/time.h>
#include "esp_attr.h"

static const char *TAG = "MQTTS_EXAMPLE";
char data[50]="{\"Ten\":\"esp32\",\"Tinh nang\":\"Bao mat\"}";
char topic[50];
char my_json_string[100];
char lenh[50];
char time_s[50];

#define BLINK_GPIO 2
#define BLINK 16

// static void obtain_time(void);
// static void initialize_sntp(void);

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

#if CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t mqtt_eclipse_org_pem_start[]  = "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END CERTIFICATE-----";
#else
extern const uint8_t mqtt_eclipse_org_pem_start[]   asm("_binary_mqtt_eclipse_org_pem_start");
#endif
extern const uint8_t mqtt_eclipse_org_pem_end[]   asm("_binary_mqtt_eclipse_org_pem_end");

extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "LED", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "/topic/qos0", data, 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            sprintf(topic,"%.*s", event->topic_len, event->topic); 
            sprintf(data,"%.*s", event->data_len, event->data); 

            time_t now;
            time_t timestamp_esp;
            struct tm timeinfo;           
            
            time(&now);
            localtime_r(&now, &timeinfo);
            // Set timezone to VietNam Standard Time
            setenv("TZ", "CST-7", 1);
            tzset();
            localtime_r(&now, &timeinfo);
            timestamp_esp = mktime(&timeinfo);
            ESP_LOGI(TAG, "The timestamp in esp is: %ld", timestamp_esp);

            cJSON *root = cJSON_Parse(data);
            if (strcmp(topic, "LED")== 0){
                char *lenh = cJSON_GetObjectItem(root,"lenh")->valuestring;
                ESP_LOGI(TAG, "lenh=%s",lenh);

                char * ptr_end;
                long int long_time_s;
                char *time_s = cJSON_GetObjectItem(root,"Time")->valuestring;
                ESP_LOGI(TAG, "TimeStamp in server is: %s",time_s);
                long_time_s = strtol (time_s,&ptr_end,10);
                
                if (abs(timestamp_esp-long_time_s)<20){
                    ESP_LOGI(TAG, "Lệnh OK");
                    if (strcmp(lenh, "ON")== 0) {
                        gpio_set_level(BLINK_GPIO, 1);
                        gpio_set_level(BLINK, 1);
                    } else if (strcmp(lenh, "OFF")== 0) {
                        gpio_set_level(BLINK_GPIO, 0);
                        gpio_set_level(BLINK, 0);
                    }
                } else {
                    ESP_LOGI(TAG, "Lệnh lỗi thời gian");
                }

            }
            
            // const char *my_json_string = cJSON_Print(root);
	        // ESP_LOGI(TAG, "my_json_string\n%s",my_json_string);

            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtts://192.168.0.101:8883",
        .event_handle = mqtt_event_handler,
        .cert_pem = (const char *)mqtt_eclipse_org_pem_start,
        .client_cert_pem = (const char *)client_cert_pem_start,
        .client_key_pem = (const char *)client_key_pem_start,
        .username = "esp32",
        .password = "12341234",
    };

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(BLINK);
    gpio_set_direction(BLINK, GPIO_MODE_OUTPUT);

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    cJSON *root = cJSON_Parse(data);
    const char *my_json_string = cJSON_Print(root);
	ESP_LOGI(TAG, "my_json_string\n%s",my_json_string);

    // time_t now;
    // struct tm timeinfo;
    // time_t timestamp;

    // time(&now);
    // localtime_r(&now, &timeinfo);
    // // Is time set? If not, tm_year will be (1970 - 1900).
    // if (timeinfo.tm_year < (2016 - 1900)) {
    //     ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
    //     obtain_time();
    //     // update 'now' variable with current time
    //     time(&now);
    // }
    
    // // Set timezone to VietNam Standard Time
    // setenv("TZ", "CST-7", 1);
    // tzset();
    // localtime_r(&now, &timeinfo);
    // timestamp = mktime(&timeinfo);
    // ESP_LOGI(TAG, "The timestamp in VietNam is: %ld", timestamp);

    time_t now;
    time_t timestamp;
    struct tm timeinfo;
    char strftime_buf[64];

    time(&now);
    localtime_r(&now, &timeinfo);

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    time(&now);
    localtime_r(&now, &timeinfo);
    // Set timezone to VietNam Standard Time
    setenv("TZ", "CST-7", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    timestamp = mktime(&timeinfo);
    ESP_LOGI(TAG, "The timestamp in VietNam is: %ld", timestamp);

    mqtt_app_start();

    time(&now);
    localtime_r(&now, &timeinfo);
    timestamp = mktime(&timeinfo);
    ESP_LOGI(TAG, "The timestamp in VietNam is: %ld", timestamp);

    time(&now);
    // Set timezone to China Standard Time
    setenv("TZ", "CST-7", 1);
    tzset();

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

    // esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    // esp_mqtt_client_start(client);
    // esp_mqtt_client_publish(client, "/topic/qos0", "tuan", 0, 0, 0);
}

// static void obtain_time(void)
// {
//     ESP_ERROR_CHECK( nvs_flash_init() );
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK( esp_event_loop_create_default() );

//     /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
//      * Read "Establishing Wi-Fi or Ethernet Connection" section in
//      * examples/protocols/README.md for more information about this function.
//      */
//     ESP_ERROR_CHECK(example_connect());

//     initialize_sntp();

//     // wait for time to be set
//     time_t now = 0;
//     struct tm timeinfo = { 0 };
//     int retry = 0;
//     const int retry_count = 10;
//     while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
//         ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
//         vTaskDelay(2000 / portTICK_PERIOD_MS);
//     }
//     time(&now);
//     localtime_r(&now, &timeinfo);

//     ESP_ERROR_CHECK( example_disconnect() );
// }

// static void initialize_sntp(void)
// {
//     ESP_LOGI(TAG, "Initializing SNTP");
//     sntp_setoperatingmode(SNTP_OPMODE_POLL);
//     sntp_setservername(0, "pool.ntp.org");
//     sntp_set_time_sync_notification_cb(time_sync_notification_cb);
//     sntp_init();
// }