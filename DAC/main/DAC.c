#include "driver/dac_cosine.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stdio.h>

void app_main(void) {

  dac_cosine_handle_t chan; // dac_cosine_handle_t ist pointer auf config struct
                            // und chan ist der name der pointer variable

  dac_cosine_config_t config = {
      .chan_id = DAC_CHAN_0, // GPIO 25
      .freq_hz = 500,
      .clk_src = DAC_COSINE_CLK_SRC_DEFAULT,
      .atten = DAC_COSINE_ATTEN_DEFAULT,
      .phase = DAC_COSINE_PHASE_0,
      .offset = 0,
      .flags.force_set_freq = true}; // flags ist unterstruct und force_set_freq
                                     // ist member des unterstructs

  // force_set_freq in der definition als Bit Feld (bool x : 1), als ein Bit
  // festgelegt für Speicher sparen

  dac_cosine_new_channel(&config,
                         &chan); // cos_cfg und ret_handle sind pointer auf
                                 // pointer, deswegen reference pass
  dac_cosine_start(chan);
}
