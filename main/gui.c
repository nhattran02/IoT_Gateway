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
#include "button.h"

#define STACK_SIZE 100

typedef TickType_t (*screenSelectCallback)(FontxFile *, int8_t);

/* Define a data structure to represent a menu item */
typedef struct menuItem
{
	const char *label;
	screenSelectCallback dispFunc;
	struct menuItem *subMenus;
	int numSubMenus;
}menuItem;

menuItem* menuStack[STACK_SIZE];
int8_t top = -1;

FontxFile fx16G[2];
FontxFile fx24G[2];
// FontxFile fx32G[2];

static TFT_t dev;
static int width  = SCREEN_WIDTH;
static int height = SCREEN_HEIGHT;
static const char *TAG = "GUI";

const char * mainScreenTextOptions[] = {"Connection Config", "Cloud Config",   "Sensor Config",  "Option 4"};
const char * connScreenTextOptions[] = {"WiFi", "4G/TLE", "Bluetooth", "Exit"};
const char * wifiScreenTextOptions[] = {"Turn off", "Scan WiFi", "Add New WiFi", "Exit"};

static void guiTextAlign(size_t stringLen, uint8_t fontWidth, uint8_t fontHeight, e_align_t alignment, uint16_t * xPos, uint16_t * yPos);
static TickType_t guiBoot(FontxFile *fx);
static void initGUI(void);
static TickType_t mainScreenSelect(FontxFile *fx, int8_t mainScreenOption);
static TickType_t connScreenSelect(FontxFile *fx, int8_t connScreenOption);
static TickType_t wifiScreenSelect(FontxFile *fx, int8_t wifiScreenOption);
static void pushStack(menuItem *menuStack);
menuItem* popStack(void);


menuItem wifiScreenItems[] = {
	{"Turn off", NULL, NULL, 0},
	{"Scan WiFi", NULL, NULL, 0},
	{"Add New WiFi", NULL, NULL, 0},
	{"Exit", NULL, NULL, 0}
};

menuItem connScreenItems[] = {
	{"WiFi", wifiScreenSelect, wifiScreenItems, 4},
	{"4G/TLE", NULL, NULL, 0},
	{"Bluetooth", NULL, NULL, 0},
	{"Exit", NULL, NULL, 0}
};

menuItem mainScreenItems[] = {
	{"Connection Config", connScreenSelect, connScreenItems, 4},
	{"Cloud Config", NULL, NULL, 0},
	{"Sensor Config", NULL, NULL, 0},
	{"Option 4", NULL, NULL, 0}
};


void GUITask(void *pvParameters)
{	
    initGUI();
    guiBoot(fx16G);
	BaseType_t err_queue;
	gpio_num_t dataRcv;
	// connScreenSelect(fx16G, CONNECT_TLE);
	
    while(1){
		// connScreenSelect(fx16G, CONNECT_WIFI);
		// vTaskDelay(1000 / portTICK_PERIOD_MS);
		// connScreenSelect(fx16G, CONNECT_TLE);
		// vTaskDelay(1000 / portTICK_PERIOD_MS);
		// connScreenSelect(fx16G, CONNECT_BLE);
		// vTaskDelay(1000 / portTICK_PERIOD_MS);
		// connScreenSelect(fx16G, CONNECT_EXIT);		
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
		// connectStatus.isWifiOn = true;
		// wifiScreenSelect(fx16G, WIFI_ON_OFF);
		// vTaskDelay(1000 / portTICK_PERIOD_MS);
		// wifiScreenSelect(fx16G, WIFI_SCAN);
		// vTaskDelay(1000 / portTICK_PERIOD_MS);
		// wifiScreenSelect(fx16G, WIFI_ADD_NEW);
		// vTaskDelay(1000 / portTICK_PERIOD_MS);
		// wifiScreenSelect(fx16G, WIFI_EXIT);		
        // vTaskDelay(1000 / portTICK_PERIOD_MS);		
		err_queue = xQueueReceive (buttonMessageQueue, (void* const )&dataRcv, (TickType_t) portMAX_DELAY);
		if(err_queue == pdTRUE){
			switch (dataRcv)
			{
			case BUTTON_UP:
				ESP_LOGI(TAG, "Button Up");
				break;
			case BUTTON_DOWN:
				ESP_LOGI(TAG, "Button Down");
				break;
			case BUTTON_ENTER:
				ESP_LOGI(TAG, "Button Enter");
				break;
			default:
				break;
			}
		}
		// vTaskDelay(100/portTICK_PERIOD_MS);
    }
}

static void guiTextAlign(size_t stringLen, uint8_t fontWidth, uint8_t fontHeight, e_align_t alignment, uint16_t * xPos, uint16_t * yPos)
{
	switch(alignment)
	{
		case ALIGN_CENTER:
			if(width > fontHeight){
				*xPos = ((width - fontHeight) / 2) - 1;
			}else{
				*xPos = 0;
			}
			if(height > (stringLen * fontWidth)){
				*yPos = (height - (stringLen * fontWidth)) / 2;
			}else{
				*yPos = 0;
			}
			break;
		case ALIGN_RIGHT:
			if(width > fontHeight){
				*xPos = ((width - fontHeight) / 2) - 1;
			}else{
				*xPos = 0;
			}
			if(height > (stringLen * fontWidth)){
				*yPos = (height - (stringLen * fontWidth));
			}else{
				*yPos = 0;
			}
			break;
		case ALIGN_LEFT:
			if(width > fontHeight){
				*xPos = ((width - fontHeight) / 2) - 1;
			}else{
				*xPos = 0;
			}
			*yPos = 0;
			break;
		default:
			break;
	}
}


static TickType_t guiBoot(FontxFile *fx)
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

	lcdFillScreen(&dev, BG_COLOR);
	color = BLACK;

	const char * textLoading[] = {"Loading .", "Loading ..", "Loading ..."};
	const uint8_t loadingSize = sizeof(textLoading)/sizeof(textLoading[0]);
	for(int i = 0; i < loadingSize; i++){
		strcpy((char *)ascii, textLoading[i]);
		guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
		lcdDrawString(&dev, fx, xPos, yPos, ascii, color);
		vTaskDelay(800/portTICK_PERIOD_MS);
		lcdFillScreen(&dev, BG_COLOR);
	}

	endTick = xTaskGetTickCount();
	diffTick = endTick - startTick;
	// ESP_LOGI(__FUNCTION__, "elapsed time[ms]:%"PRIu32,diffTick*portTICK_PERIOD_MS);
	return diffTick;
}

static void initGUI(void)
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


static TickType_t wifiScreenSelect(FontxFile *fx, int8_t wifiScreenOption)
{
	wifi_screen_option_t _wifiScreenOption = (wifi_screen_option_t)wifiScreenOption;
	TickType_t startTick, endTick, diffTick;
	startTick = xTaskGetTickCount();
	uint16_t xPos;
	uint16_t yPos;
	uint8_t ascii[30] = {0};
	uint8_t buffer[FontxGlyphBufSize];
	uint8_t fontWidth;
	uint8_t fontHeight;
	GetFontx(fx, 0, buffer, &fontWidth, &fontHeight);


	if(!(connectStatus.isWifiOn)){
		lcdFillScreen(&dev, BG_COLOR);

		if(connectStatus.isWifiConnected){
			strcpy((char*)ascii, "Connected");
		}else{
			strcpy((char*)ascii, "Disconnected");
		}
		guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
		lcdDrawString(&dev, fx, X_START, yPos, ascii, BLACK);
		lcdDrawLine(&dev, X_START, Y_START, X_START, Y_END, BLACK);

		strcpy((char*)ascii, "Turn on");
		guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
		lcdDrawFillRect(&dev, X_START - 20, 10, X_START - 20 + fontHeight, Y_END - 10, WHITE_SMOKE);
		lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);
		for(int i = 0; i < 4; i++){
			lcdDrawLine(&dev, X_START - 20 - (i + 1), 15, X_START - 20 - (i + 1), Y_END - 10 + 4, GRAY);
			lcdDrawLine(&dev, X_START - 20 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 20, Y_END - 10 + (i + 1), GRAY);
		}		
	}else{
		if(connectStatus.isWifiConnected){
			strcpy((char*)ascii, "Connected");
		}else{
			strcpy((char*)ascii, "Disconnected");
		}
		guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
		lcdDrawString(&dev, fx, X_START, yPos, ascii, BLACK);
		lcdDrawLine(&dev, X_START, Y_START, X_START, Y_END, BLACK);

		switch (_wifiScreenOption)
		{
			case WIFI_ON_OFF:
			{
				strcpy((char*)ascii, wifiScreenItems[WIFI_ON_OFF].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 20, 10, X_START - 20 + fontHeight, Y_END - 10, WHITE_SMOKE);
				lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);
				for(int i = 0; i < 4; i++){
					lcdDrawLine(&dev, X_START - 20 - (i + 1), 15, X_START - 20 - (i + 1), Y_END - 10 + 4, GRAY);
					lcdDrawLine(&dev, X_START - 20 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 20, Y_END - 10 + (i + 1), GRAY);
				}

				strcpy((char*)ascii, wifiScreenItems[WIFI_SCAN].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);

				strcpy((char*)ascii, wifiScreenItems[WIFI_ADD_NEW].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);


				strcpy((char*)ascii, wifiScreenItems[WIFI_EXIT].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);
				break;
			}
			case WIFI_SCAN:
			{
				strcpy((char*)ascii, wifiScreenItems[WIFI_SCAN].label);		
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 45, 10, X_START - 45 + fontHeight, Y_END - 10, WHITE_SMOKE);
				lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);
				for(int i = 0; i < 4; i++){
					lcdDrawLine(&dev, X_START - 45 - (i + 1), 15, X_START - 45 - (i + 1), Y_END - 10 + 4, GRAY);
					lcdDrawLine(&dev, X_START - 45 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 45, Y_END - 10 + (i + 1), GRAY);
				}

				strcpy((char*)ascii, wifiScreenItems[WIFI_ON_OFF].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);

				strcpy((char*)ascii, wifiScreenItems[WIFI_ADD_NEW].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);

				strcpy((char*)ascii, wifiScreenItems[WIFI_EXIT].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);
				
				break;
			}
			case WIFI_ADD_NEW:
			{

				strcpy((char*)ascii, wifiScreenItems[WIFI_ADD_NEW].label);		
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 70, 10, X_START - 70 + fontHeight, Y_END - 10, WHITE_SMOKE);
				lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);
					for(int i = 0; i < 4; i++){
					lcdDrawLine(&dev, X_START - 70 - (i + 1), 15, X_START - 70 - (i + 1), Y_END - 10 + 4, GRAY);
					lcdDrawLine(&dev, X_START - 70 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 70, Y_END - 10 + (i + 1), GRAY);
				}

				strcpy((char*)ascii, wifiScreenItems[WIFI_EXIT].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);

				strcpy((char*)ascii, wifiScreenItems[WIFI_SCAN].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);

				strcpy((char*)ascii, wifiScreenItems[WIFI_ON_OFF].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);

				break;
			}
			case WIFI_EXIT:
			{
				strcpy((char*)ascii, wifiScreenItems[WIFI_EXIT].label);		
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, WHITE_SMOKE);
				lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);
				for(int i = 0; i < 4; i++){
					lcdDrawLine(&dev, X_START - 95 - (i + 1), 15, X_START - 95 - (i + 1), Y_END - 10 + 4, GRAY);
					lcdDrawLine(&dev, X_START - 95 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 95, Y_END - 10 + (i + 1), GRAY);
				}

				strcpy((char*)ascii, wifiScreenItems[WIFI_ADD_NEW].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);

				strcpy((char*)ascii, wifiScreenItems[WIFI_ON_OFF].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);

				strcpy((char*)ascii, wifiScreenItems[WIFI_SCAN].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);
				
				break;
			}
			default:
				break;
		}
	}
	

	endTick = xTaskGetTickCount();
	diffTick = endTick - startTick;
	// ESP_LOGI(__FUNCTION__, "elapsed time[ms]:%"PRIu32,diffTick*portTICK_PERIOD_MS);
	return diffTick;	
}

static TickType_t connScreenSelect(FontxFile *fx, int8_t connScreenOption)
{
	connect_screen_option_t _connScreenOption = (connect_screen_option_t)connScreenOption;

	TickType_t startTick, endTick, diffTick;
	startTick = xTaskGetTickCount();
	uint16_t xPos;
	uint16_t yPos;
	uint8_t ascii[30] = {0};
	uint8_t buffer[FontxGlyphBufSize];
	uint8_t fontWidth;
	uint8_t fontHeight;
	GetFontx(fx, 0, buffer, &fontWidth, &fontHeight);

	strcpy((char*)ascii, "Connect Screen");  
	guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
	lcdDrawString(&dev, fx, X_START, yPos, ascii, BLACK);
	lcdDrawLine(&dev, X_START, Y_START, X_START, Y_END, BLACK);
	
	switch (_connScreenOption)
	{
		case CONNECT_WIFI:
		{
			strcpy((char*)ascii, connScreenItems[CONNECT_WIFI].label);
			if(connectStatus.isWifiConnected){
				strcat((char*)ascii, " <ON>");
			}else{
				strcat((char*)ascii, " <OFF>");
			}
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20, 10, X_START - 20 + fontHeight, Y_END - 10, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);
			for(int i = 0; i < 4; i++){
				lcdDrawLine(&dev, X_START - 20 - (i + 1), 15, X_START - 20 - (i + 1), Y_END - 10 + 4, GRAY);
				lcdDrawLine(&dev, X_START - 20 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 20, Y_END - 10 + (i + 1), GRAY);
			}

			strcpy((char*)ascii, connScreenItems[CONNECT_TLE].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);

			strcpy((char*)ascii, connScreenItems[CONNECT_BLE].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);


			strcpy((char*)ascii, connScreenItems[CONNECT_EXIT].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);
			break;
		}
		case CONNECT_TLE:
		{
			strcpy((char*)ascii, connScreenItems[CONNECT_TLE].label);
			if(connectStatus.isLTEConnected){
				strcat((char*)ascii, " <ON>");
			}else{
				strcat((char*)ascii, " <OFF>");
			}			
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45, 10, X_START - 45 + fontHeight, Y_END - 10, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);
			for(int i = 0; i < 4; i++){
				lcdDrawLine(&dev, X_START - 45 - (i + 1), 15, X_START - 45 - (i + 1), Y_END - 10 + 4, GRAY);
				lcdDrawLine(&dev, X_START - 45 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 45, Y_END - 10 + (i + 1), GRAY);
			}

			strcpy((char*)ascii, connScreenItems[CONNECT_WIFI].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);

			strcpy((char*)ascii, connScreenItems[CONNECT_BLE].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);

			strcpy((char*)ascii, connScreenItems[CONNECT_EXIT].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);
			
			break;
		}
		case CONNECT_BLE:
		{

			strcpy((char*)ascii, connScreenItems[CONNECT_BLE].label);
			if(connectStatus.isBLEConnected){
				strcat((char*)ascii, " <ON>");
			}else{
				strcat((char*)ascii, " <OFF>");
			}			
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70, 10, X_START - 70 + fontHeight, Y_END - 10, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);
				for(int i = 0; i < 4; i++){
				lcdDrawLine(&dev, X_START - 70 - (i + 1), 15, X_START - 70 - (i + 1), Y_END - 10 + 4, GRAY);
				lcdDrawLine(&dev, X_START - 70 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 70, Y_END - 10 + (i + 1), GRAY);
			}

			strcpy((char*)ascii, connScreenItems[CONNECT_EXIT].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);

			strcpy((char*)ascii, connScreenItems[CONNECT_TLE].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);

			strcpy((char*)ascii, connScreenItems[CONNECT_WIFI].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);

			break;
		}
		case CONNECT_EXIT:
		{
			strcpy((char*)ascii, connScreenItems[CONNECT_EXIT].label);		
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);
			for(int i = 0; i < 4; i++){
				lcdDrawLine(&dev, X_START - 95 - (i + 1), 15, X_START - 95 - (i + 1), Y_END - 10 + 4, GRAY);
				lcdDrawLine(&dev, X_START - 95 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 95, Y_END - 10 + (i + 1), GRAY);
			}

			strcpy((char*)ascii, connScreenItems[CONNECT_BLE].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);

			strcpy((char*)ascii, connScreenItems[CONNECT_WIFI].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);

			strcpy((char*)ascii, connScreenItems[CONNECT_TLE].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);
			
			break;
		}
		default:
			break;
	}

	endTick = xTaskGetTickCount();
	diffTick = endTick - startTick;
	// ESP_LOGI(__FUNCTION__, "elapsed time[ms]:%"PRIu32,diffTick*portTICK_PERIOD_MS);
	return diffTick;
}


// static TickType_t wifiScreenSelect()
// {

// }

static TickType_t mainScreenSelect(FontxFile *fx, int8_t mainScreenOption)
{
	main_screen_option_t _mainScreenOption = (main_screen_option_t)mainScreenOption;
	TickType_t startTick, endTick, diffTick;
	startTick = xTaskGetTickCount();
	uint16_t xPos;
	uint16_t yPos;
	uint8_t ascii[30] = {0};
	uint8_t buffer[FontxGlyphBufSize];
	uint8_t fontWidth;
	uint8_t fontHeight;
	GetFontx(fx, 0, buffer, &fontWidth, &fontHeight);
	
    strcpy((char*)ascii, "Main Screen");
	guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
	lcdDrawString(&dev, fx, X_START, yPos, ascii, BLACK);
	lcdDrawLine(&dev, X_START, Y_START, X_START, Y_END, BLACK);

	switch (_mainScreenOption)
	{
		case CONNECT_CONFIG:
		{
			strcpy((char*)ascii, mainScreenTextOptions[0]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20, 10, X_START - 20 + fontHeight, Y_END - 10, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);
			for(int i = 0; i < 4; i++){
				lcdDrawLine(&dev, X_START - 20 - (i + 1), 15, X_START - 20 - (i + 1), Y_END - 10 + 4, GRAY);
				lcdDrawLine(&dev, X_START - 20 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 20, Y_END - 10 + (i + 1), GRAY);
			}

			strcpy((char*)ascii, mainScreenTextOptions[1]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);

			strcpy((char*)ascii, mainScreenTextOptions[2]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);


			strcpy((char*)ascii, mainScreenTextOptions[3]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);
			break;
		}
		case CLOUD_CONFIG:
		{
			strcpy((char*)ascii, mainScreenTextOptions[1]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45, 10, X_START - 45 + fontHeight, Y_END - 10, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);
			for(int i = 0; i < 4; i++){
				lcdDrawLine(&dev, X_START - 45 - (i + 1), 15, X_START - 45 - (i + 1), Y_END - 10 + 4, GRAY);
				lcdDrawLine(&dev, X_START - 45 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 45, Y_END - 10 + (i + 1), GRAY);
			}

			strcpy((char*)ascii, mainScreenTextOptions[0]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);

			strcpy((char*)ascii, mainScreenTextOptions[2]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);

			strcpy((char*)ascii, mainScreenTextOptions[3]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);
			
			break;
		}
		case SENSOR_CONFIG:
		{

			strcpy((char*)ascii, mainScreenTextOptions[2]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70, 10, X_START - 70 + fontHeight, Y_END - 10, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);
				for(int i = 0; i < 4; i++){
				lcdDrawLine(&dev, X_START - 70 - (i + 1), 15, X_START - 70 - (i + 1), Y_END - 10 + 4, GRAY);
				lcdDrawLine(&dev, X_START - 70 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 70, Y_END - 10 + (i + 1), GRAY);
			}

			strcpy((char*)ascii, mainScreenTextOptions[3]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);

			strcpy((char*)ascii, mainScreenTextOptions[1]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);

			strcpy((char*)ascii, mainScreenTextOptions[0]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);

			break;
		}
		case OPTION4:
		{
			strcpy((char*)ascii, mainScreenTextOptions[3]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);
			for(int i = 0; i < 4; i++){
				lcdDrawLine(&dev, X_START - 95 - (i + 1), 15, X_START - 95 - (i + 1), Y_END - 10 + 4, GRAY);
				lcdDrawLine(&dev, X_START - 95 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 95, Y_END - 10 + (i + 1), GRAY);
			}

			strcpy((char*)ascii, mainScreenTextOptions[2]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);

			strcpy((char*)ascii, mainScreenTextOptions[0]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);

			strcpy((char*)ascii, mainScreenTextOptions[1]);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);
			
			break;
		}
		default:
			break;
	}

	endTick = xTaskGetTickCount();
	diffTick = endTick - startTick;
	// ESP_LOGI(__FUNCTION__, "elapsed time[ms]:%"PRIu32,diffTick*portTICK_PERIOD_MS);
	return diffTick;	
}

static void pushStack(menuItem *menu_item)
{
	if(top < STACK_SIZE - 1){
		menuStack[++top] = menu_item;
	}else{
		ESP_LOGE(TAG, "Menu stack is overflow");
	}
}

menuItem* popStack(){
	if(top >= 0){
		return menuStack[top --];
	}else{
		return NULL;
	}
}
