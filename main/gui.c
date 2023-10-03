/**
********************************************************************************
* @file         gui.c
* @brief        Graphical User Interface module
* @since        Created on 2023-09-26
* @author       Tran Minh Nhat - 2014008
********************************************************************************
*/

#include "gui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include "ili9340.h"
#include "fontx.h"
#include "bmpfile.h"
#include "decode_jpeg.h"
#include "decode_png.h"
#include "pngle.h"
#include "driver/gpio.h"


FontxFile fx16G[2];
FontxFile fx24G[2];
// FontxFile fx32G[2];

static TFT_t dev;
static const char *TAG = "GUI module";

static void guiTextAlign(int screenWidth, int screenHeight, size_t stringLen, uint8_t fontWidth, uint8_t fontHeight, e_align_t alignment, uint16_t * xPos, uint16_t * yPos);

TickType_t guiBoot(TFT_t * dev, FontxFile *fx, int width, int height)
{
	TickType_t startTick, endTick, diffTick;
	startTick = xTaskGetTickCount();
	uint16_t xPos;
	uint16_t yPos;
	uint8_t ascii[30];
	uint16_t color;
	uint8_t buffer[FontxGlyphBufSize];
	uint8_t fontWidth;
	uint8_t fontHeight;
	GetFontx(fx, 0, buffer, &fontWidth, &fontHeight);
	ESP_LOGI(TAG, "fontWidth = %d; fontHeight = %d", fontWidth, fontHeight);

	lcdFillScreen(dev, WHITE_SMOKE);
	color = BLACK;

	const char * textLoading[] = {"Loading .", "Loading ..", "Loading ..."};
	const uint8_t loadingSize = sizeof(textLoading)/sizeof(textLoading[0]);
	for(int i = 0; i < loadingSize; i++){
		strcpy((char *)ascii, textLoading[i]);
		guiTextAlign(width, height, strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
		lcdDrawString(dev, fx, xPos, yPos, ascii, color);
		vTaskDelay(800/portTICK_PERIOD_MS);
		lcdFillScreen(dev, WHITE_SMOKE);
	}

	endTick = xTaskGetTickCount();
	diffTick = endTick - startTick;
	ESP_LOGI(__FUNCTION__, "elapsed time[ms]:%"PRIu32,diffTick*portTICK_PERIOD_MS);
	return diffTick;
}

static void guiTextAlign(int screenWidth, int screenHeight, size_t stringLen, uint8_t fontWidth, uint8_t fontHeight, e_align_t alignment, uint16_t * xPos, uint16_t * yPos)
{
	switch(alignment)
	{
		case ALIGN_CENTER:
			if(screenWidth > fontHeight){
				*xPos = ((screenWidth - fontHeight) / 2) - 1;
			}else{
				*xPos = 0;
			}
			if(screenHeight > (stringLen * fontWidth)){
				*yPos = (screenHeight - (stringLen * fontWidth)) / 2;
			}else{
				*yPos = 0;
			}
			break;
		case ALIGN_RIGHT:
			if(screenWidth > fontHeight){
				*xPos = ((screenWidth - fontHeight) / 2) - 1;
			}else{
				*xPos = 0;
			}
			if(screenHeight > (stringLen * fontWidth)){
				*yPos = (screenHeight - (stringLen * fontWidth));
			}else{
				*yPos = 0;
			}
			break;
		case ALIGN_LEFT:
			if(screenWidth > fontHeight){
				*xPos = ((screenWidth - fontHeight) / 2) - 1;
			}else{
				*xPos = 0;
			}
			*yPos = 0;
			break;
		default:
			break;
	}
}

void initGUI(void)
{
    InitFontx(fx16G,"/spiffs/ILGH16XB.FNT",""); // 8x16Dot  Gothic
	InitFontx(fx24G,"/spiffs/ILGH24XB.FNT",""); // 12x24Dot Gothic
	// InitFontx(fx32G,"/spiffs/ILGH32XB.FNT",""); // 16x32Dot Gothic

    spi_master_init(&dev, MOSI_GPIO, SCLK_GPIO, CS_GPIO, DC_GPIO, RESET_GPIO, BACKLIGHT_GPIO, 
                    XPT_MISO_GPIO, XPT_CS_GPIO, XPT_IRQ_GPIO, XPT_SCLK_GPIO, XPT_MOSI_GPIO);
    uint16_t model = 0x9341;    //Using ILI9341 driver
    lcdInit(&dev, model, SCREEN_WIDTH, SCREEN_HEIGHT, GRAM_X_OFFSET, GRAM_Y_OFFSET);

#if CONFIG_USE_RGB_COLOR
    ESP_LOGI(TAG, "Change BGR filter to RGB filter");
    lcdBGRFilter(&dev);
#endif 
	lcdSetFontDirection(&dev, DIRECTION90);

}

void GUITask(void *pvParameters)
{	
	
    initGUI();
    guiBoot(&dev, fx16G, SCREEN_WIDTH, SCREEN_HEIGHT);
    while(1){
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

