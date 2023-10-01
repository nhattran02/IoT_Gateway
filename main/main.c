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
#include "gui.h"
#include "connect_wifi.h"
#include "spiffs.h"

static const char *TAG = "IoT Gateway";


TaskHandle_t GUITaskHandle;
TaskHandle_t wifiTaskHandle;

static void initHardware(void)
{
	gpio_set_direction(WIFI_LED_STATUS, GPIO_MODE_OUTPUT);
	
	// Initialize NVS
	ESP_LOGI(TAG, "Initialize NVS");
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	// Initialize SPIFFS
	err = initSPIFFS("/spiffs", "LCD_Font", 10);
	if (err != ESP_OK) return;
}

void app_main(void)
{
	initHardware();
	xTaskCreate(GUITask, "GUI", 1024 * 5, NULL, 2, &GUITaskHandle);
	xTaskCreate(wifiTask, "WiFi", 1024 * 5, NULL, 2, &wifiTaskHandle);
}

