/*
 * SSD1306 OLED Driver for ESP-IDF
 * Minimal I2C driver for 128x64 OLED displays
 */

#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64

/**
 * Initialize the SSD1306 display
 * @param sda_pin GPIO pin for I2C SDA
 * @param scl_pin GPIO pin for I2C SCL
 * @param i2c_addr I2C address (usually 0x3C or 0x3D)
 * @return ESP_OK on success
 */
esp_err_t ssd1306_init(int sda_pin, int scl_pin, uint8_t i2c_addr);

/**
 * Clear the display (all pixels off)
 */
void ssd1306_clear(void);

/**
 * Fill the display (all pixels on)
 */
void ssd1306_fill(void);

/**
 * Set a single pixel
 * @param x X coordinate (0-127)
 * @param y Y coordinate (0-63)
 * @param on true = pixel on, false = pixel off
 */
void ssd1306_set_pixel(int x, int y, bool on);

/**
 * Get pixel state from buffer
 * @param x X coordinate (0-127)
 * @param y Y coordinate (0-63)
 * @return true if pixel is on
 */
bool ssd1306_get_pixel(int x, int y);

/**
 * Draw a filled rectangle
 */
void ssd1306_fill_rect(int x, int y, int w, int h, bool on);

/**
 * Draw a filled circle
 */
void ssd1306_fill_circle(int cx, int cy, int r, bool on);

/**
 * Send the frame buffer to the display
 * Call this after drawing operations to update the screen
 */
void ssd1306_update(void);

/**
 * Set display contrast (brightness)
 * @param contrast 0-255
 */
void ssd1306_set_contrast(uint8_t contrast);

/**
 * Invert display colors
 * @param invert true = inverted, false = normal
 */
void ssd1306_invert(bool invert);

#endif // SSD1306_H

