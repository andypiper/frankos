/*
 * FRANK OS — TLV320DAC3100 I2S Audio DAC driver (Fruit Jam)
 * Copyright (c) 2025 Mikhail Matveev <xtreme@rh1.tech>
 * https://rh1.tech
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * TLV320DAC3100 stereo audio DAC initialisation for the Adafruit Fruit Jam.
 *
 * The DAC is configured over I2C (GPIO 20/21) before the PIO I2S engine
 * starts sending audio data on GPIO 24-27.
 *
 * Pinout (Fruit Jam):
 *   I2C SDA : GPIO 20  (DAC_I2C_SDA_PIN)
 *   I2C SCL : GPIO 21  (DAC_I2C_SCL_PIN)
 *   I2S DIN : GPIO 24  (I2S_DATA_PIN)
 *   I2S MCLK: GPIO 25  (I2S_MCLK_PIN  — PWM-generated master clock)
 *   I2S BCLK: GPIO 26  (I2S_CLOCK_PIN_BASE)
 *   I2S WS  : GPIO 27  (I2S_CLOCK_PIN_BASE + 1)
 *
 * Call tlv320_init() once before i2s_init() / snd_init().
 */

#ifndef TLV320DAC3100_H
#define TLV320DAC3100_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialise the TLV320DAC3100:
 *   1. Set up I2C0 on SDA/SCL pins.
 *   2. Generate MCLK via PWM on I2S_MCLK_PIN (12.288 MHz for 48 kHz, or
 *      11.289 MHz for 44.1 kHz — selected by sample_rate).
 *   3. Reset and configure the DAC registers for I2S slave mode,
 *      headphone + speaker outputs enabled, volume at unity gain.
 *
 * @param sample_rate  Audio sample rate in Hz (typically 44100 or 48000).
 * @return true on success, false if I2C communication fails.
 */
bool tlv320_init(uint32_t sample_rate);

/*
 * Set DAC output volume (0 = max, 127 = muted).
 * Applied to both headphone (HP) and speaker (SPK) outputs.
 */
void tlv320_set_volume(uint8_t vol);

#ifdef __cplusplus
}
#endif

#endif /* TLV320DAC3100_H */
