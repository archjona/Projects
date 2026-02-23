// Programm zum lernen von I2C anhand des BME280 Sensors und zum lernen von
// ERROR handling beim ESP32

#include "driver/i2c_master.h" // esp32 = master, BME280 = Slave --> nur master treiber nötig
#include "freertos/FreeRTOS.h"
#include <stdio.h>

static const i2c_port_num_t i2c_port =
    0; // port 0 (erster I2C controller vom esp32)
static const gpio_num_t sda_io_num =
    25; // GPIO number für SDA line (pulled up internally)
static const gpio_num_t scl_io_num = 27; // GPIO number für SCL line
static const uint8_t glitch_ignore_cnt =
    7; // "If the glitch period on the line is less than this value, it can be
       // filtered out, typically value is 7" ~ Doc
// static const int intr_priority = 1; // interrupt priority
static const uint16_t device_adress =
    0x76; // BME I2C Adresse 0111011x (bei SDO -> GND = 0), sonst 0x77 (bei SDO
          // -> V_DDIO = 1)
static const uint32_t scl_speed_hz =
    100000; // I2C clock rate (100kHz hier als normal mode)
static const uint32_t scl_wait_us =
    0; // 0 ist default empfohlene länge des sensors
static const uint8_t reg_addr =
    0xF7; // in der doc "write_buffer", 0xF7 Startregister für temp, pressure,
          // humidity (data sheet BME280) hier quasi pointer auf den start des
          // bereiches, jedes Register 1 byte, 20 bits temp, 20 pressure, 16
          // humidity
static uint8_t data[8];

void app_main(void) {

  i2c_master_bus_config_t i2c_master_bus_config = {

      .i2c_port = i2c_port,
      .sda_io_num = sda_io_num,
      .scl_io_num = scl_io_num,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = glitch_ignore_cnt,
      .intr_priority = 1,
      .flags.enable_internal_pullup = 1};

  i2c_master_bus_handle_t bus_handle;

  i2c_new_master_bus(&i2c_master_bus_config, &bus_handle);

  i2c_device_config_t dev_cfg = {
      .dev_addr_length =
          I2C_ADDR_BIT_LEN_7, // länge einer Adresse vom I2C bus des BME280
      .device_address = device_adress,
      .scl_speed_hz = scl_speed_hz,
      .scl_wait_us = scl_wait_us,
      .flags.disable_ack_check = 0};

  i2c_master_dev_handle_t dev_handle;

  i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

// Register Definitionen für Sensoreinstellung
#define BME280_REG_CTRL_HUM 0xF2  // crtl_hum
#define BME280_REG_CTRL_MEAS 0xF4 // crtl_meas

  // 1. Feuchtigkeits-Oversampling einstellen
  uint8_t config_hum[] = {BME280_REG_CTRL_HUM, 0x01};
  // 001 setzt oversampling auf 1x, wenn nicht reingeschrieben wird dann
  // standardmäßig auf 0 (skip)
  i2c_master_transmit(dev_handle, config_hum, 2, -1);

  // 2. Temp/Press Oversampling + Normal Mode aktivieren
  // 0x27 bedeutet: Temp x1, Press x1, Mode: Normal
  uint8_t config_meas[] = {BME280_REG_CTRL_MEAS, 0x27};
  // 0xF4 ist register für measurement config, 001 macht oversampling auf 1x für
  // temp, nochmal 001 macht oversampling auf 1x für pressure, 11 setzt den
  // modus des sensors auf "normal", 001 001 11 = 0x27 -> 0x27 muss in register
  // 0xF4 geschrieben werden
  i2c_master_transmit(dev_handle, config_meas, 2, -1);

  printf("BME280 aufgeweckt...\n");

  while (1) {

    i2c_master_transmit(dev_handle, &reg_addr, 1, -1);
    // der write buffer muss auf die anfangsadresse zeigen, also dem register ab
    // dem gelesen werden soll
    i2c_master_receive(dev_handle, data, 8, -1);
    uint16_t bme_H = ((uint16_t)data[6] << 8) | data[7];
    printf("Feuchtigkeit (raw): %d\n", bme_H);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
