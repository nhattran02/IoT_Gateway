#include <esp_system.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "button.h"

static const char *TAG = "Button";

QueueHandle_t buttonMessageQueue;

void initButton(void)
{
    gpio_set_direction(BUTTON_UP, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_DOWN, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_ENTER, GPIO_MODE_INPUT);

    gpio_pullup_en(BUTTON_UP);
    gpio_pullup_en(BUTTON_DOWN);
    gpio_pullup_en(BUTTON_ENTER);

    gpio_pulldown_dis(BUTTON_UP);
    gpio_pulldown_dis(BUTTON_DOWN);
    gpio_pulldown_dis(BUTTON_ENTER);
}

gpio_num_t detectButton(void)
{
    gpio_num_t buttons[] = {BUTTON_UP, BUTTON_DOWN, BUTTON_ENTER};
    for(int i = 0; i < sizeof(buttons) / sizeof(gpio_num_t); i++){
        if(gpio_get_level(buttons[i]) == 0){
            vTaskDelay(DEBOUNCE/portTICK_PERIOD_MS);
            if(gpio_get_level(buttons[i]) == 0){
                return buttons[i];
            }
        }
    }
    return GPIO_NUM_NC;
}

void buttonTask(void * pvParameters)
{
	BaseType_t err_queue;
    gpio_num_t buttonMessage = GPIO_NUM_NC;
    buttonMessageQueue = xQueueCreate(10, sizeof(gpio_num_t));
    while(1){
        buttonMessage = detectButton();
        if(buttonMessage == GPIO_NUM_NC){
            continue;
        }else{
            err_queue = xQueueSend(buttonMessageQueue, (const void *)&buttonMessage, (TickType_t)(NO_WAIT_TIME));
            if(err_queue != pdTRUE){
                ESP_LOGE(TAG, "Button Message Queue Send Error");
            }
        }
        // switch (button)
        // {
        //     case BUTTON_UP:
        //     {
        //         ESP_LOGI(TAG, "Button Up");
        //         buttonMessage = BUTTON_UP_SIGNAL;
        //         break;
        //     }
        //     case BUTTON_DOWN:
        //     {
        //         ESP_LOGI(TAG, "Button Down");
        //         buttonMessage = BUTTON_DOWN_SIGNAL;
        //         break;
        //     }
        //     case BUTTON_ENTER:
        //     {
        //         ESP_LOGI(TAG, "Button Enter");
        //         buttonMessage = BUTTON_ENTER_SIGNAL;
        //         break;
        //     }
        //     default:
        //         break;
        // }
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}