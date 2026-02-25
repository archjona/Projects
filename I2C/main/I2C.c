// Programm zum lernen von I2C anhand des BME280 Sensors

#include "driver/i2c_master.h" // esp32 = master, BME280 = Slave --> nur master treiber nötig
#include "freertos/FreeRTOS.h"
#include <stdio.h>

typedef struct {
  uint16_t dig_T1;
  int16_t dig_T2;
  int16_t dig_T3;
  uint8_t dig_H1;
  int16_t dig_H2;
  uint8_t dig_H3;
  int16_t dig_H4;
  int16_t dig_H5;
  int8_t dig_H6;
} bme280_calib_data_t;

static bme280_calib_data_t cal;
static int32_t t_fine;

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

float compensate_T(int32_t adc_T);
float compensate_H(int32_t adc_H);

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

  uint8_t reg;
  uint8_t t_cal[6], h_cal_part1, h_cal_part2[7];

  reg = 0x88; // Temp Kalibrierung
  i2c_master_transmit_receive(dev_handle, &reg, 1, t_cal, 6, -1);
  cal.dig_T1 = (t_cal[1] << 8) | t_cal[0];
  cal.dig_T2 = (int16_t)((t_cal[3] << 8) | t_cal[2]);
  cal.dig_T3 = (int16_t)((t_cal[5] << 8) | t_cal[4]);

  reg = 0xA1; // Hum H1
  i2c_master_transmit_receive(dev_handle, &reg, 1, &h_cal_part1, 1, -1);
  cal.dig_H1 = h_cal_part1;

  reg = 0xE1; // Hum H2..H6
  i2c_master_transmit_receive(dev_handle, &reg, 1, h_cal_part2, 7, -1);
  cal.dig_H2 = (int16_t)((h_cal_part2[1] << 8) | h_cal_part2[0]);
  cal.dig_H3 = h_cal_part2[2];
  cal.dig_H4 = (int16_t)((h_cal_part2[3] << 4) | (h_cal_part2[4] & 0x0F));
  // 0x0F (0000 1111) wird als bit mask verwendet, weil H4 aus 12 Bits besteht
  // und die unteren 4 bits nur die unteren vom 8 bit 0xE5 register sein sollen
  cal.dig_H5 = (int16_t)((h_cal_part2[5] << 4) | (h_cal_part2[4] >> 4));
  cal.dig_H6 = (int8_t)h_cal_part2[6];

  printf("BME280 aufgeweckt...\n");

  while (1) {
    // 8 Bytes lesen: Press(0-2), Temp(3-5), Hum(6-7)
    i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, 8, -1);

    // Rohwerte aus dem Buffer ziehen
    // Temperatur sind 20 Bit (data[3], data[4] und die oberen 4 Bit von
    // data[5])
    int32_t adc_T = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
    // Feuchtigkeit sind 16 Bit (data[6] und data[7])
    int32_t adc_H = (data[6] << 8) | data[7];

    // Kompensation anwenden
    float real_T = compensate_T(adc_T); // Berechnet auch t_fine
    float real_H = compensate_H(adc_H);

    printf("Temp: %.2f °C | Feuchte: %.2f %%\n", real_T, real_H);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// mathematische Kompensation (Formeln aus Datenblatt)
float compensate_T(int32_t adc_T) {
  int32_t var1, var2;
  var1 =
      ((((adc_T >> 3) - ((int32_t)cal.dig_T1 << 1))) * ((int32_t)cal.dig_T2)) >>
      11;
  var2 = (((((adc_T >> 4) - ((int32_t)cal.dig_T1)) *
            ((adc_T >> 4) - ((int32_t)cal.dig_T1))) >>
           12) *
          ((int32_t)cal.dig_T3)) >>
         14;
  t_fine = var1 + var2;
  return (float)((t_fine * 5 + 128) >> 8) / 100.0;
}

float compensate_H(int32_t adc_H) {
  int32_t v_x1_u32r;
  v_x1_u32r = (t_fine - ((int32_t)76800));
  v_x1_u32r =
      (((((adc_H << 14) - (((int32_t)cal.dig_H4) << 20) -
          (((int32_t)cal.dig_H5) * v_x1_u32r)) +
         ((int32_t)16384)) >>
        15) *
       (((((((v_x1_u32r * ((int32_t)cal.dig_H6)) >> 10) *
            (((v_x1_u32r * ((int32_t)cal.dig_H3)) >> 11) + ((int32_t)32768))) >>
           10) +
          ((int32_t)2097152)) *
             ((int32_t)cal.dig_H2) +
         8192) >>
        14));
  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                             ((int32_t)cal.dig_H1)) >>
                            4));
  v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
  v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
  return (float)((uint32_t)(v_x1_u32r >> 12)) / 1024.0;
}
