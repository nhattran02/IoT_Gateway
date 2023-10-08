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


/* Define a data structure to represent a menu item */
typedef struct menuScreen
{
	const char *label;
	TickType_t (*dispFunc)(FontxFile *, int8_t, struct menuScreen);
	struct menuScreen *subMenus;
	int8_t numOfSubMenus;
	int8_t curSubMenusDisp;
}menuScreen;

menuScreen menuScreenStack[STACK_SIZE] = {0};

int8_t top = 0;

FontxFile fx16G[2];
FontxFile fx24G[2];
// FontxFile fx32G[2];

static TFT_t dev;
static int width  = SCREEN_WIDTH;
static int height = SCREEN_HEIGHT;
static const char *TAG = "GUI";


static void guiTextAlign(size_t stringLen, uint8_t fontWidth, uint8_t fontHeight, e_align_t alignment, uint16_t * xPos, uint16_t * yPos);
static TickType_t guiBoot(FontxFile *fx);
static void initGUI(void);
static TickType_t dispMainScreen(FontxFile *fx, int8_t mainScreenOption, struct menuScreen curScreen);
static TickType_t dispConnScreen(FontxFile *fx, int8_t connScreenOption, struct menuScreen curScreen);
static TickType_t dispWifiScreen(FontxFile *fx, int8_t wifiScreenOption, struct menuScreen curScreen);

static void initStack(void);
static void pushStack(menuScreen screen);
static int8_t popStack(menuScreen *screen);


menuScreen wifiScreenSubMenus[] = {
	{
		.label = "Turn on/off",
		.dispFunc = NULL,
		.subMenus = NULL,
		.numOfSubMenus = 0,
		.curSubMenusDisp = 0,
	}, 
	{
		.label = "Scan WiFi",
		.dispFunc = NULL,
		.subMenus = NULL,
		.numOfSubMenus = 0,
		.curSubMenusDisp = 0,
	}, 
	{
		.label = "Add New WiFi",
		.dispFunc = NULL,
		.subMenus = NULL,
		.numOfSubMenus = 0,
		.curSubMenusDisp = 0,
	}, 
	{
		.label = "Back",
		.dispFunc = NULL,
		.subMenus = NULL,
		.numOfSubMenus = 0,
		.curSubMenusDisp = 0,
	}
};

menuScreen connScreenSubMenus[] = {
	{
		.label = "WiFi",
		.dispFunc = dispWifiScreen,
		.subMenus = wifiScreenSubMenus,
		.numOfSubMenus = 4,
		.curSubMenusDisp = 0,
	},
	{
		.label = "4G/TLE",
		.dispFunc = NULL,
		.subMenus = NULL,
		.numOfSubMenus = 0,
		.curSubMenusDisp = 0,
	},
	{
		.label = "Bluetooth",
		.dispFunc = NULL,
		.subMenus = NULL,
		.numOfSubMenus = 0,
		.curSubMenusDisp = 0,
	},
	{
		.label = "Back",
		.dispFunc = NULL,
		.subMenus = NULL,
		.numOfSubMenus = 0,
		.curSubMenusDisp = 0,
	}
};

menuScreen mainScreenSubMenus[] = {
	{
		.label = "Connection Config",
		.dispFunc = dispConnScreen,
		.subMenus = connScreenSubMenus,
		.numOfSubMenus = 4,
		.curSubMenusDisp = 0,
	}, 
	{
		.label = "Cloud Config",
		.dispFunc = NULL,
		.subMenus = NULL,
		.numOfSubMenus = 0,
		.curSubMenusDisp = 0,
	}, 
	{
		.label = "Sensor Config",
		.dispFunc = NULL,
		.subMenus = NULL,
		.numOfSubMenus = 0,
		.curSubMenusDisp = 0,
	}, 
	{
		.label = "Option 4",
		.dispFunc = NULL,
		.subMenus = NULL,
		.numOfSubMenus = 0,
		.curSubMenusDisp = 0,
	}
};

menuScreen mainScreen = {
	.label = "Main Screen", 
	.dispFunc = dispMainScreen, 
	.subMenus = mainScreenSubMenus,
	.numOfSubMenus = 4,
	.curSubMenusDisp = 0,
};


void GUITask(void *pvParameters)
{	
    initGUI();
    guiBoot(fx16G);
	BaseType_t err_queue;
	gpio_num_t btnSignal;
	menuScreen curScreen = mainScreen;
	pushStack(curScreen);
	// curScreen.dispFunc(fx16G, curScreen.curSubMenusDisp, curScreen);		
    while(1){
		if(curScreen.subMenus != NULL){
			curScreen.dispFunc(fx16G, curScreen.curSubMenusDisp, curScreen);
		}
		err_queue = xQueueReceive (buttonMessageQueue, (void* const )&btnSignal, (TickType_t) portMAX_DELAY);
		if(err_queue == pdTRUE){
			switch (btnSignal)
			{
				case BUTTON_UP:
					curScreen.curSubMenusDisp = ((curScreen.curSubMenusDisp - 1) < 0) ? 0 : (curScreen.curSubMenusDisp - 1);
					ESP_LOGI(TAG, "|Button Up | Screen: %s | Index: %d|", curScreen.label, curScreen.curSubMenusDisp);	
					break;
				case BUTTON_DOWN:
					if(!connectStatus.isWifiOn && strcmp((const char *)curScreen.label, (const char *)"WiFi") == 0){
						curScreen.numOfSubMenus = 2;
					}
					curScreen.curSubMenusDisp = (curScreen.curSubMenusDisp + 1) > (curScreen.numOfSubMenus - 1) ? (curScreen.numOfSubMenus - 1) : (curScreen.curSubMenusDisp + 1);
					ESP_LOGI(TAG, "|Button Down | Screen: %s | Index: %d|", curScreen.label, curScreen.curSubMenusDisp);
					break;
				case BUTTON_ENTER:
					if((!connectStatus.isWifiOn) && strcmp((const char *)curScreen.label, (const char *)"WiFi") != 0){
						if((curScreen.subMenus[curScreen.curSubMenusDisp].subMenus != NULL) && (strcmp(curScreen.subMenus[curScreen.curSubMenusDisp].label, "Back") != 0)){
							pushStack(curScreen);
							curScreen = curScreen.subMenus[curScreen.curSubMenusDisp];
						}else if(strcmp(curScreen.subMenus[curScreen.curSubMenusDisp].label, "Back") == 0){
							if(popStack(&curScreen) != 0){
								ESP_LOGE(TAG, "Failed to pop stack");
							}
						}						
					}else{
						if((curScreen.subMenus[curScreen.curSubMenusDisp].subMenus != NULL)){
							pushStack(curScreen);
							curScreen = curScreen.subMenus[curScreen.curSubMenusDisp];
						}else if(curScreen.curSubMenusDisp == (curScreen.numOfSubMenus - 1)){
							ESP_LOGI(TAG, "DEBUG");
							if(popStack(&curScreen) != 0){
								ESP_LOGE(TAG, "Failed to pop stack");
							}
						}						
					}

					ESP_LOGI(TAG, "|Button Enter | Screen: %s | Index: %d|", curScreen.label, curScreen.curSubMenusDisp);
					lcdFillScreen(&dev, BG_COLOR);
					break;
				default:
					break;
			}
		}
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


static TickType_t dispWifiScreen(FontxFile *fx, int8_t wifiScreenOption, menuScreen curScreen)
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

		switch (_wifiScreenOption)
		{
			case WIFI_ON_OFF:
			{
				strcpy((char*)ascii, "Turn on");
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 20, 10, X_START - 20 + fontHeight, Y_END - 10, WHITE_SMOKE);
				lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);
				// for(int i = 0; i < 4; i++){
				// 	lcdDrawLine(&dev, X_START - 20 - (i + 1), 15, X_START - 20 - (i + 1), Y_END - 10 + 4, GRAY);
				// 	lcdDrawLine(&dev, X_START - 20 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 20, Y_END - 10 + (i + 1), GRAY);
				// }

				strcpy((char*)ascii, "Back");
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);
				break;
			}
			case 1:
			{
				strcpy((char*)ascii, "Back");		
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 45, 10, X_START - 45 + fontHeight, Y_END - 10, WHITE_SMOKE);
				lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);
				// for(int i = 0; i < 4; i++){
				// 	lcdDrawLine(&dev, X_START - 45 - (i + 1), 15, X_START - 45 - (i + 1), Y_END - 10 + 4, GRAY);
				// 	lcdDrawLine(&dev, X_START - 45 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 45, Y_END - 10 + (i + 1), GRAY);
				// }

				strcpy((char*)ascii, "Turn on");
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);				
				break;
			}
			default:
				break;
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
				strcpy((char*)ascii, curScreen.subMenus[WIFI_ON_OFF].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 20, 10, X_START - 20 + fontHeight, Y_END - 10, WHITE_SMOKE);
				lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);
				// for(int i = 0; i < 4; i++){
				// 	lcdDrawLine(&dev, X_START - 20 - (i + 1), 15, X_START - 20 - (i + 1), Y_END - 10 + 4, GRAY);
				// 	lcdDrawLine(&dev, X_START - 20 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 20, Y_END - 10 + (i + 1), GRAY);
				// }

				strcpy((char*)ascii, curScreen.subMenus[WIFI_SCAN].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);

				strcpy((char*)ascii, curScreen.subMenus[WIFI_ADD_NEW].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);


				strcpy((char*)ascii, curScreen.subMenus[WIFI_EXIT].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);
				break;
			}
			case WIFI_SCAN:
			{
				strcpy((char*)ascii, curScreen.subMenus[WIFI_SCAN].label);		
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 45, 10, X_START - 45 + fontHeight, Y_END - 10, WHITE_SMOKE);
				lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);
				// for(int i = 0; i < 4; i++){
				// 	lcdDrawLine(&dev, X_START - 45 - (i + 1), 15, X_START - 45 - (i + 1), Y_END - 10 + 4, GRAY);
				// 	lcdDrawLine(&dev, X_START - 45 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 45, Y_END - 10 + (i + 1), GRAY);
				// }

				strcpy((char*)ascii, curScreen.subMenus[WIFI_ON_OFF].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);

				strcpy((char*)ascii, curScreen.subMenus[WIFI_ADD_NEW].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);

				strcpy((char*)ascii, curScreen.subMenus[WIFI_EXIT].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);
				
				break;
			}
			case WIFI_ADD_NEW:
			{

				strcpy((char*)ascii, curScreen.subMenus[WIFI_ADD_NEW].label);		
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 70, 10, X_START - 70 + fontHeight, Y_END - 10, WHITE_SMOKE);
				lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);
				// 	for(int i = 0; i < 4; i++){
				// 	lcdDrawLine(&dev, X_START - 70 - (i + 1), 15, X_START - 70 - (i + 1), Y_END - 10 + 4, GRAY);
				// 	lcdDrawLine(&dev, X_START - 70 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 70, Y_END - 10 + (i + 1), GRAY);
				// }

				strcpy((char*)ascii, curScreen.subMenus[WIFI_EXIT].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);

				strcpy((char*)ascii, curScreen.subMenus[WIFI_SCAN].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, BLACK);

				strcpy((char*)ascii, curScreen.subMenus[WIFI_ON_OFF].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);

				break;
			}
			case WIFI_EXIT:
			{
				strcpy((char*)ascii, curScreen.subMenus[WIFI_EXIT].label);		
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, WHITE_SMOKE);
				lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, BLACK);
				// for(int i = 0; i < 4; i++){
				// 	lcdDrawLine(&dev, X_START - 95 - (i + 1), 15, X_START - 95 - (i + 1), Y_END - 10 + 4, GRAY);
				// 	lcdDrawLine(&dev, X_START - 95 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 95, Y_END - 10 + (i + 1), GRAY);
				// }

				strcpy((char*)ascii, curScreen.subMenus[WIFI_ADD_NEW].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, BLACK);

				strcpy((char*)ascii, curScreen.subMenus[WIFI_ON_OFF].label);
				guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
				lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
				lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, BLACK);

				strcpy((char*)ascii, curScreen.subMenus[WIFI_SCAN].label);
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

static TickType_t dispConnScreen(FontxFile *fx, int8_t connScreenOption, menuScreen curScreen)
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
	lcdDrawLine(&dev, X_START, Y_START, X_START, Y_END, TEXT_COLOR);
	
	switch (_connScreenOption)
	{
		case CONNECT_WIFI:
		{
			strcpy((char*)ascii, curScreen.subMenus[CONNECT_WIFI].label);
			if(connectStatus.isWifiConnected){
				strcat((char*)ascii, " <ON>");
			}else{
				strcat((char*)ascii, " <OFF>");
			}
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20, 10, X_START - 20 + fontHeight, Y_END - 10, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, TEXT_COLOR);
			// for(int i = 0; i < 4; i++){
			// 	lcdDrawLine(&dev, X_START - 20 - (i + 1), 15, X_START - 20 - (i + 1), Y_END - 10 + 4, GRAY);
			// 	lcdDrawLine(&dev, X_START - 20 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 20, Y_END - 10 + (i + 1), GRAY);
			// }

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_TLE].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_BLE].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, TEXT_COLOR);


			strcpy((char*)ascii, curScreen.subMenus[CONNECT_EXIT].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, TEXT_COLOR);
			break;
		}
		case CONNECT_TLE:
		{
			strcpy((char*)ascii, curScreen.subMenus[CONNECT_TLE].label);
			if(connectStatus.isLTEConnected){
				strcat((char*)ascii, " <ON>");
			}else{
				strcat((char*)ascii, " <OFF>");
			}			
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45, 10, X_START - 45 + fontHeight, Y_END - 10, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, TEXT_COLOR);
			// for(int i = 0; i < 4; i++){
			// 	lcdDrawLine(&dev, X_START - 45 - (i + 1), 15, X_START - 45 - (i + 1), Y_END - 10 + 4, GRAY);
			// 	lcdDrawLine(&dev, X_START - 45 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 45, Y_END - 10 + (i + 1), GRAY);
			// }

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_WIFI].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_BLE].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_EXIT].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, TEXT_COLOR);
			
			break;
		}
		case CONNECT_BLE:
		{

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_BLE].label);
			if(connectStatus.isBLEConnected){
				strcat((char*)ascii, " <ON>");
			}else{
				strcat((char*)ascii, " <OFF>");
			}			
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70, 10, X_START - 70 + fontHeight, Y_END - 10, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, TEXT_COLOR);
			// 	for(int i = 0; i < 4; i++){
			// 	lcdDrawLine(&dev, X_START - 70 - (i + 1), 15, X_START - 70 - (i + 1), Y_END - 10 + 4, GRAY);
			// 	lcdDrawLine(&dev, X_START - 70 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 70, Y_END - 10 + (i + 1), GRAY);
			// }

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_EXIT].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_TLE].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_WIFI].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, TEXT_COLOR);

			break;
		}
		case CONNECT_EXIT:
		{
			strcpy((char*)ascii, curScreen.subMenus[CONNECT_EXIT].label);		
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, TEXT_COLOR);
			// for(int i = 0; i < 4; i++){
			// 	lcdDrawLine(&dev, X_START - 95 - (i + 1), 15, X_START - 95 - (i + 1), Y_END - 10 + 4, GRAY);
			// 	lcdDrawLine(&dev, X_START - 95 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 95, Y_END - 10 + (i + 1), GRAY);
			// }

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_BLE].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_WIFI].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_TLE].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, TEXT_COLOR);
			
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

static TickType_t dispMainScreen(FontxFile *fx, int8_t mainScreenOption, menuScreen curScreen)
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
	
    strcpy((char*)ascii, mainScreen.label);
	guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
	lcdDrawString(&dev, fx, X_START, yPos, ascii, TEXT_COLOR);
	lcdDrawLine(&dev, X_START, Y_START, X_START, Y_END, TEXT_COLOR);

	switch (_mainScreenOption)
	{
		case CONNECT_CONFIG:
		{
			strcpy((char*)ascii, curScreen.subMenus[CONNECT_CONFIG].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20, 10, X_START - 20 + fontHeight, Y_END - 10, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, TEXT_COLOR);
			// for(int i = 0; i < 4; i++){
			// 	lcdDrawLine(&dev, X_START - 20 - (i + 1), 15, X_START - 20 - (i + 1), Y_END - 10 + 4, GRAY);
			// 	lcdDrawLine(&dev, X_START - 20 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 20, Y_END - 10 + (i + 1), GRAY);
			// }

			strcpy((char*)ascii, curScreen.subMenus[CLOUD_CONFIG].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[SENSOR_CONFIG].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, TEXT_COLOR);


			strcpy((char*)ascii, curScreen.subMenus[OPTION4].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, TEXT_COLOR);
			break;
		}
		case CLOUD_CONFIG:
		{
			strcpy((char*)ascii, curScreen.subMenus[CLOUD_CONFIG].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45, 10, X_START - 45 + fontHeight, Y_END - 10, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, TEXT_COLOR);
			// for(int i = 0; i < 4; i++){
			// 	lcdDrawLine(&dev, X_START - 45 - (i + 1), 15, X_START - 45 - (i + 1), Y_END - 10 + 4, GRAY);
			// 	lcdDrawLine(&dev, X_START - 45 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 45, Y_END - 10 + (i + 1), GRAY);
			// }

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_CONFIG].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[SENSOR_CONFIG].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[OPTION4].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, TEXT_COLOR);
			
			break;
		}
		case SENSOR_CONFIG:
		{

			strcpy((char*)ascii, curScreen.subMenus[SENSOR_CONFIG].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70, 10, X_START - 70 + fontHeight, Y_END - 10, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, TEXT_COLOR);
			// for(int i = 0; i < 4; i++){
			// 	lcdDrawLine(&dev, X_START - 70 - (i + 1), 15, X_START - 70 - (i + 1), Y_END - 10 + 4, GRAY);
			// 	lcdDrawLine(&dev, X_START - 70 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 70, Y_END - 10 + (i + 1), GRAY);
			// }

			strcpy((char*)ascii, curScreen.subMenus[OPTION4].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[CLOUD_CONFIG].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_CONFIG].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, TEXT_COLOR);

			break;
		}
		case OPTION4:
		{
			strcpy((char*)ascii, curScreen.subMenus[OPTION4].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 95 - 4, 10, X_START - 95 + fontHeight, Y_END - 10 + 4, WHITE_SMOKE);
			lcdDrawString(&dev, fx, X_START - 95, yPos, ascii, TEXT_COLOR);
			// for(int i = 0; i < 4; i++){
			// 	lcdDrawLine(&dev, X_START - 95 - (i + 1), 15, X_START - 95 - (i + 1), Y_END - 10 + 4, GRAY);
			// 	lcdDrawLine(&dev, X_START - 95 + fontHeight - 4, Y_END - 10 + (i + 1), X_START - 95, Y_END - 10 + (i + 1), GRAY);
			// }

			strcpy((char*)ascii, curScreen.subMenus[SENSOR_CONFIG].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 70 - 4, 10, X_START - 70 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 70, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[CONNECT_CONFIG].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 20 - 4, 10, X_START - 20 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 20, yPos, ascii, TEXT_COLOR);

			strcpy((char*)ascii, curScreen.subMenus[CLOUD_CONFIG].label);
			guiTextAlign(strlen((char *)ascii), fontWidth, fontHeight, ALIGN_CENTER, &xPos, &yPos);
			lcdDrawFillRect(&dev, X_START - 45 - 4, 10, X_START - 45 + fontHeight, Y_END - 10 + 4, BG_COLOR);
			lcdDrawString(&dev, fx, X_START - 45, yPos, ascii, TEXT_COLOR);
			
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

static void pushStack(menuScreen screen)
{
	if(top < STACK_SIZE - 1){
		menuScreenStack[top] = screen;
		top ++;
	}else{
		ESP_LOGE(TAG, "Menu stack is overflow");
	}
}

static int8_t popStack(menuScreen *screen){
	if(top > 0){
		top --;
		*screen = menuScreenStack[top];
		return 0;
	}else{
		return -1;
	}
}
