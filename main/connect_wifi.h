/**
********************************************************************************
* @file         connect_wifi.h
* @brief        Header file for connect_wifi.c
* @since        Created on 2023-09-24
* @author       Tran Minh Nhat - 2014008
********************************************************************************
*/

#ifndef _CONNECT_WIFI_H_
#define _CONNECT_WIFI_H_

#include <esp_system.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>


#define WIFI_SSID           "MANGDAYKTX H2-808"
#define WIFI_PASSWORD       "44448888a"  
#define DEFAULT_RSSI        -127
#define MAXIMUM_RETRY       5
#define WIFI_LED_STATUS     2
#define H2E_IDENTIFIER      "IoT_Gateway"
#define NUM_OF_WIFI_SUPPORT 10

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT      BIT0
#define ESPTOUCH_DONE_BIT       BIT1
#define WIFI_TURN_ON_BIT        BIT2
#define WIFI_SCAN_BIT           BIT3
#define WIFI_NEW_SSID_BIT       BIT4

extern int wifi_connect_status;
extern EventGroupHandle_t s_wifi_event_group;


void wifiTask(void *pvParameters);


#endif
