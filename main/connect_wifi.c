/**
********************************************************************************
* @file         connect_wifi.c
* @brief        Connect wifi module
* @since        Created on 2023-09-24
* @author       Tran Minh Nhat - 2014008
********************************************************************************
*/

#include "connect_wifi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "wifi_manager.h"

int wifi_connect_status = 0;
static const char *TAG = "Connect WiFi";
int s_retry_num = 0;


/* FreeRTOS event group to signal when we are connected*/
EventGroupHandle_t s_wifi_event_group;

static void smartConfigTask(void * pvParameters)
{
    EventBits_t uxBits;
    ESP_LOGI(TAG, "Smart Config start");
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if(uxBits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}


static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
        ESP_LOGI(TAG, "WiFi start connecting...");
        esp_err_t err = ESP_OK;
        /* Reading the WiFi configuration from the flash */
        ESP_LOGI(TAG, "Reading the WiFi configuration from the flash");
        FILE *read_file = fopen("/spiffs/wifi_conf", "r");
        if (read_file == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading the WiFi configuration");
            err = ESP_FAIL;
        }
        if(err == ESP_FAIL){
            xTaskCreate(smartConfigTask, "smartConfigTask", 4096, NULL, 3, NULL);
        }else{
            uint8_t read_buf[65] = {0};
            fgets((char *)read_buf, sizeof(read_buf), read_file);
            fclose(read_file);
            ESP_LOGI(TAG, "Read the password from file: %s", read_buf);         
            wifi_config_t wifi_config;
            bzero(&wifi_config, sizeof(wifi_config_t));
            const char * myssid = "MANGDAYKTX H2-808";
            memcpy(wifi_config.sta.ssid, (const void *)myssid, sizeof(wifi_config.sta.ssid));
            memcpy(wifi_config.sta.password, (const void *)read_buf, sizeof(wifi_config.sta.password));
            wifi_config.sta.scan_method = WIFI_FAST_SCAN;
            wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
            wifi_config.sta.threshold.rssi = DEFAULT_RSSI;
            wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
            ESP_ERROR_CHECK(esp_wifi_disconnect());
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
            err = esp_wifi_connect();
            if(err == ESP_ERR_WIFI_SSID){
                xTaskCreate(smartConfigTask, "smartConfigTask", 4096, NULL, 3, NULL);
            }
        }
    }else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
        ESP_LOGI(TAG, "Start wifi failed...");
        if(s_retry_num < MAXIMUM_RETRY){
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        }else{
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            ESP_LOGI(TAG, "Connect to the AP fail");
            xTaskCreate(smartConfigTask, "smartConfigTask", 4096, NULL, 3, NULL);
        }
    }else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;   
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }else if(event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE){
        ESP_LOGI(TAG, "Scan done");
    }else if(event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL){
        ESP_LOGI(TAG, "Found channel");
    }else if(event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD){
        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        ESP_LOGI(TAG, "Got SSID: %s and PSWD: %s", evt->ssid, evt->password);

        wifi_config_t wifi_config;
        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.scan_method = WIFI_FAST_SCAN;
        wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        wifi_config.sta.threshold.rssi = DEFAULT_RSSI;
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;        

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        if(esp_wifi_connect() == ESP_OK){
            /* Save to Wifi Configuration to the flash */
            ESP_LOGI(TAG, "Opening file to save WiFi config");
            FILE* write_file = fopen("/spiffs/wifi_conf", "w");
            if (write_file == NULL) {
                ESP_LOGE(TAG, "Failed to open file to save WiFi config");
                return;
            }
            fprintf(write_file, (const char *)evt->password);
            fclose(write_file);
            ESP_LOGI(TAG, "Write the wifi config to file successfully");
        }
    }else if(event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE){
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}



static void wifiConnect(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}


void cb_connection_ok(void *pvParameter){
	ip_event_got_ip_t* param = (ip_event_got_ip_t*)pvParameter;

	/* transform IP to human readable string */
	char str_ip[16];
	esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);

	ESP_LOGI(TAG, "I have a connection and my IP is %s!", str_ip);
}


void monitoring_task(void *pvParameter)
{
	for(;;){
		ESP_LOGI(TAG, "free heap: %ld",esp_get_free_heap_size());
		vTaskDelay( pdMS_TO_TICKS(10000) );
	}
}


void wifiTask(void *pvParameters)
{
    ESP_LOGI(TAG, "WiFi mode: Smart Config");
    // wifiConnect();

    /* Test captive portal */
    wifi_manager_start();
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);

    /* -------------------- */
    // s_wifi_event_group = xEventGroupCreate();
    while(1){
        // EventBits_t uxBits = xEventGroupWaitBits(s_wifi_event_group, WIFI_TURN_ON_BIT, true, false, portMAX_DELAY);
        // if(uxBits & WIFI_TURN_ON_BIT) {
        //     ESP_LOGI(TAG, "WIFI TURN ON");
        // }
        // ESP_LOGI(TAG, "free heap: %ld",esp_get_free_heap_size());
		vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
