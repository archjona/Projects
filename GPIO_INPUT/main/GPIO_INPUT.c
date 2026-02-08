#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED 4
#define Button 5

void app_main(void)
{
    // Pin als Input, Pull-Up aktivieren
    gpio_reset_pin(Button);
    gpio_reset_pin(LED); 
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(Button, GPIO_PULLUP_ONLY);
    gpio_set_direction(Button, GPIO_MODE_INPUT);

    while(1) 
    {
        int state = gpio_get_level(Button);

        if(state == 0) {
            gpio_set_level(LED, 1);
            printf("Button gedrueckt!\n");
        } else {
            gpio_set_level(LED, 0);
            printf("Button losgelassen!\n");
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);  // alle 0.5 Sekunden
    }
}