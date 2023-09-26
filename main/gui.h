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

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   160
#define GRAM_X_OFFSET   0
#define GRAM_Y_OFFSET   0

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



void guiInit(void);
void GUI(void *pvParameters);
TickType_t FillTest(TFT_t * dev, int width, int height);


#endif /* GUI_H_ */
