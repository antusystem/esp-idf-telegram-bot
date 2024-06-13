/* esp-idf-telegram-bot
 *
 * Author: antusystem
 * e-mail: aleantunes95@gmail.com
 * Date: 11-01-2020
 * MIT License
 * As it is described in the readme file
 *
*/

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "driver/gpio.h"
#include "freertos/event_groups.h"
#include "network_handler.h"

//Pin connected to a led
#define LED (GPIO_NUM_13)

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //Change it the pin that has a led
	gpio_pad_select_gpio(LED);
	gpio_set_direction(LED, GPIO_MODE_OUTPUT);
	gpio_set_level(LED, 1);

    wifi_init_sta();

    xTaskCreatePinnedToCore(&http_test_task, "http_test_task", 8192*4, NULL, 5, NULL,1);

}
