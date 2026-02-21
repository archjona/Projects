#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

// Wir nehmen UART_NUM_0, weil das der USB-Port ist!
#define UART_PORT UART_NUM_0
#define BUF_SIZE (1024)
#define LED_EXTERNAL 25

static QueueHandle_t uart_queue;

void app_main(void) {

  gpio_reset_pin(LED_EXTERNAL);
  gpio_set_direction(LED_EXTERNAL, GPIO_MODE_OUTPUT);

  // 2. UART Konfiguration (Standard für USB-Verbindung)
  uart_config_t uart_config = {
      .baud_rate = 115200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };

  // Installieren auf UART_NUM_0 (USB)
  uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 10, &uart_queue, 0);
  uart_param_config(UART_PORT, &uart_config);

  printf("\n--- ESP32 UART BEREIT ---\n");
  printf("Druecke '1' fuer AN, '0' fuer AUS\n");

  uint8_t data[BUF_SIZE];
  uart_event_t event; // wird automatisch ald DATA event aufgerufen ohne
                      // voreinstellung wenn ein byte ankommt

  while (1) {
    if (xQueueReceive(uart_queue, (void *)&event, portMAX_DELAY)) {
      if (event.type == UART_DATA) {
        int len = uart_read_bytes(UART_PORT, data, event.size,
                                  20 / portTICK_PERIOD_MS);
        if (len > 0) {
          // Wir schauen uns das Zeichen an
          char key = data[0];

          if (key == '1') {
            gpio_set_level(LED_EXTERNAL, 1);
            printf("Empfangen: %c -> LED AN\n", key);
          } else if (key == '0') {
            gpio_set_level(LED_EXTERNAL, 0);
            printf("Empfangen: %c -> LED AUS\n", key);
          }
        }
      }
    }
  }
}
