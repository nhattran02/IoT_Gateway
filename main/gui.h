/**
********************************************************************************
* @file         gui.h
* @brief        Header file for gui.c
* @since        Created on 2023-09-26
* @author       Tran Minh Nhat - 2014008
********************************************************************************
*/

#ifndef GUI_H_
#define GUI_H_

#include "esp_vfs.h"
#include "ili9340.h"
#include "connect.h"

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   160
#define GRAM_X_OFFSET   0
#define GRAM_Y_OFFSET   0

#define X_START         110
#define Y_START         0

#define X_END           0
#define Y_END           160

#define MOSI_GPIO       23
#define SCLK_GPIO       18
#define CS_GPIO         14
#define DC_GPIO         27
#define RESET_GPIO      33
#define BACKLIGHT_GPIO  -1

/* Disable Touch Controller */
#define XPT_MISO_GPIO   -1
#define XPT_CS_GPIO     -1
#define XPT_IRQ_GPIO    -1
#define XPT_SCLK_GPIO   -1
#define XPT_MOSI_GPIO   -1



#define CONFIG_USE_RGB_COLOR    1

typedef enum
{
    ALIGN_CENTER = 0,
    ALIGN_RIGHT = 1,
    ALIGN_LEFT = 2,
}e_align_t;

typedef enum 
{
    CONNECT_CONFIG = 0,
    CLOUD_CONFIG = 1, 
    SENSOR_CONFIG = 2,
    OPTION4 = 3,
}main_screen_option_t;

typedef enum
{
    CONNECT_WIFI = 0,
    CONNECT_TLE = 1,
    CONNECT_BLE = 2,
    CONNECT_EXIT = 3,    
}connect_screen_option_t;


typedef enum 
{
    WIFI_ON_OFF = 0,
    WIFI_SCAN = 1,
    WIFI_ADD_NEW = 2,
    WIFI_EXIT = 3,
}wifi_screen_option_t;


void GUITask(void *pvParameters);


#endif /* GUI_H_ */
