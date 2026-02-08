#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define BLINK_LED 4 // Pin define

void app_main(void)
{
	char* ourTaskName = pcTaskGetName(NULL); // getName returned namen einer task, bei NULL macht er automatisch die task die hier läuft

	ESP_LOGI(ourTaskName, "Hello, starting up!\n"); // zeigt namen des prozesses (1. argument) und startnachricht im monitor/log

	gpio_reset_pin(BLINK_LED); // reset falls pin vorher besetzt war
	gpio_set_direction(BLINK_LED, GPIO_MODE_OUTPUT);

	while(1)
	{
		gpio_set_level(BLINK_LED, 1); // LED wird auf HIGH gesetzt
		vTaskDelay(1000 / portTICK_PERIOD_MS); // Sleep für Anzahl an systicks -> durch conversion rate teilen zu millisekunden 
		gpio_set_level(BLINK_LED, 0); // LED wir auf LOW gesetzt
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

}
