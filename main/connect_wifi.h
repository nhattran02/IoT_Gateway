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
#define MAXIMUM_RETRY       5
#define WIFI_LED_STATUS     2
#define H2E_IDENTIFIER      "IoT_Gateway"


/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1


extern int wifi_connect_status;

void wifiTask(void *pvParameters);



#endif
