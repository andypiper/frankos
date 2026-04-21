/*
 * FRANK OS — TLV320DAC3100 audio DAC driver (Adafruit Fruit Jam).
 *
 * The Fruit Jam uses a TI TLV320DAC3100 stereo DAC connected via I2C
 * (for register configuration) and I2S (for audio data).  This driver
 * initialises the chip so that the existing I2S PIO audio path works
 * without changes.
 *
 * Must be called once from main() BEFORE snd_init() starts the I2S DMA.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TLV320DAC3100_H
#define TLV320DAC3100_H

#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialise the TLV320DAC3100.
 *   i2c      — I2C instance (i2c0 on Fruit Jam)
 *   sda_pin  — GPIO for I2C SDA (20 on Fruit Jam)
 *   scl_pin  — GPIO for I2C SCL (21 on Fruit Jam)
 *   i2c_addr — 7-bit address (0x18 on Fruit Jam, ADDR pin low)
 *
 * Configures: PLL for 44100 Hz from 12 MHz MCLK, I2S slave mode,
 * 16-bit stereo, headphone + class-D speaker outputs enabled.
 *
 * Returns true on success (ACK received from DAC).
 */
bool tlv320_init(i2c_inst_t *i2c, uint sda_pin, uint scl_pin, uint8_t i2c_addr);

/* Set output volume.
 *   vol — 0 (mute) to 127 (0 dB) per channel. */
void tlv320_set_volume(i2c_inst_t *i2c, uint8_t i2c_addr, uint8_t vol);

#ifdef __cplusplus
}
#endif

#endif /* TLV320DAC3100_H */
