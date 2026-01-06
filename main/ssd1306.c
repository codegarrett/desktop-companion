/*
 * SSD1306 OLED Driver for ESP-IDF
 * Minimal I2C driver for 128x64 OLED displays
 */

#include "ssd1306.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ssd1306";

// Frame buffer: 128x64 pixels, 1 bit per pixel = 1024 bytes
// Organized as 8 horizontal pages of 128 bytes each
static uint8_t frame_buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

static i2c_master_dev_handle_t dev_handle = NULL;
static uint8_t display_addr = 0x3C;

// SSD1306 commands
#define SSD1306_CMD_DISPLAY_OFF         0xAE
#define SSD1306_CMD_DISPLAY_ON          0xAF
#define SSD1306_CMD_SET_CONTRAST        0x81
#define SSD1306_CMD_NORMAL_DISPLAY      0xA6
#define SSD1306_CMD_INVERT_DISPLAY      0xA7
#define SSD1306_CMD_SET_MUX_RATIO       0xA8
#define SSD1306_CMD_SET_DISPLAY_OFFSET  0xD3
#define SSD1306_CMD_SET_START_LINE      0x40
#define SSD1306_CMD_SET_SEG_REMAP       0xA0
#define SSD1306_CMD_SET_COM_SCAN_DIR    0xC0
#define SSD1306_CMD_SET_COM_PINS        0xDA
#define SSD1306_CMD_SET_CLOCK_DIV       0xD5
#define SSD1306_CMD_SET_PRECHARGE       0xD9
#define SSD1306_CMD_SET_VCOM_DESELECT   0xDB
#define SSD1306_CMD_CHARGE_PUMP         0x8D
#define SSD1306_CMD_MEMORY_MODE         0x20
#define SSD1306_CMD_SET_COL_ADDR        0x21
#define SSD1306_CMD_SET_PAGE_ADDR       0x22

// Send a single command byte
static esp_err_t ssd1306_send_cmd(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};  // 0x00 = command mode
    return i2c_master_transmit(dev_handle, data, 2, 100);
}

// Send multiple command bytes
static esp_err_t ssd1306_send_cmds(const uint8_t *cmds, size_t len) {
    for (size_t i = 0; i < len; i++) {
        esp_err_t ret = ssd1306_send_cmd(cmds[i]);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

esp_err_t ssd1306_init(int sda_pin, int scl_pin, uint8_t i2c_addr) {
    display_addr = i2c_addr;
    
    ESP_LOGI(TAG, "Initializing I2C bus (SDA=%d, SCL=%d)", sda_pin, scl_pin);
    
    // Initialize I2C bus
    i2c_master_bus_handle_t bus_handle;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = scl_pin,
        .sda_io_num = sda_pin,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Add SSD1306 device
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        .scl_speed_hz = 400000,
    };
    
    ret = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Initializing SSD1306 display at address 0x%02X", i2c_addr);
    
    // Initialization sequence for 128x64 SSD1306
    const uint8_t init_cmds[] = {
        SSD1306_CMD_DISPLAY_OFF,        // Display off
        SSD1306_CMD_SET_CLOCK_DIV, 0x80,// Set clock divide ratio
        SSD1306_CMD_SET_MUX_RATIO, 0x3F,// Set multiplex ratio (64-1)
        SSD1306_CMD_SET_DISPLAY_OFFSET, 0x00, // No display offset
        SSD1306_CMD_SET_START_LINE | 0x00,    // Start line 0
        SSD1306_CMD_CHARGE_PUMP, 0x14,  // Enable charge pump
        SSD1306_CMD_MEMORY_MODE, 0x00,  // Horizontal addressing mode
        SSD1306_CMD_SET_SEG_REMAP | 0x01,     // Segment remap (flip horizontal)
        SSD1306_CMD_SET_COM_SCAN_DIR | 0x08,  // COM scan direction (flip vertical)
        SSD1306_CMD_SET_COM_PINS, 0x12, // COM pins hardware config
        SSD1306_CMD_SET_CONTRAST, 0xCF, // Contrast
        SSD1306_CMD_SET_PRECHARGE, 0xF1,// Pre-charge period
        SSD1306_CMD_SET_VCOM_DESELECT, 0x40,  // VCOMH deselect level
        0xA4,                           // Output follows RAM
        SSD1306_CMD_NORMAL_DISPLAY,     // Normal display (not inverted)
        SSD1306_CMD_DISPLAY_ON,         // Display on
    };
    
    ret = ssd1306_send_cmds(init_cmds, sizeof(init_cmds));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Clear frame buffer and display
    ssd1306_clear();
    ssd1306_update();
    
    ESP_LOGI(TAG, "SSD1306 initialized successfully");
    return ESP_OK;
}

void ssd1306_clear(void) {
    memset(frame_buffer, 0, sizeof(frame_buffer));
}

void ssd1306_fill(void) {
    memset(frame_buffer, 0xFF, sizeof(frame_buffer));
}

void ssd1306_set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) {
        return;
    }
    
    // Calculate byte index and bit position
    // Display is organized in 8 horizontal pages (8 rows of pixels each)
    int page = y / 8;
    int bit = y % 8;
    int idx = page * SSD1306_WIDTH + x;
    
    if (on) {
        frame_buffer[idx] |= (1 << bit);
    } else {
        frame_buffer[idx] &= ~(1 << bit);
    }
}

bool ssd1306_get_pixel(int x, int y) {
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) {
        return false;
    }
    
    int page = y / 8;
    int bit = y % 8;
    int idx = page * SSD1306_WIDTH + x;
    
    return (frame_buffer[idx] & (1 << bit)) != 0;
}

void ssd1306_fill_rect(int x, int y, int w, int h, bool on) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            ssd1306_set_pixel(i, j, on);
        }
    }
}

void ssd1306_fill_circle(int cx, int cy, int r, bool on) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                ssd1306_set_pixel(cx + x, cy + y, on);
            }
        }
    }
}

void ssd1306_update(void) {
    // Set column address range (0-127)
    ssd1306_send_cmd(SSD1306_CMD_SET_COL_ADDR);
    ssd1306_send_cmd(0);    // Start column
    ssd1306_send_cmd(127);  // End column
    
    // Set page address range (0-7)
    ssd1306_send_cmd(SSD1306_CMD_SET_PAGE_ADDR);
    ssd1306_send_cmd(0);    // Start page
    ssd1306_send_cmd(7);    // End page
    
    // Send frame buffer data
    // We need to send in chunks because I2C has buffer limits
    // Format: 0x40 (data mode) followed by pixel data
    uint8_t chunk[129];  // 1 control byte + 128 data bytes
    chunk[0] = 0x40;     // Data mode
    
    for (int page = 0; page < 8; page++) {
        memcpy(&chunk[1], &frame_buffer[page * SSD1306_WIDTH], SSD1306_WIDTH);
        i2c_master_transmit(dev_handle, chunk, 129, 100);
    }
}

void ssd1306_set_contrast(uint8_t contrast) {
    ssd1306_send_cmd(SSD1306_CMD_SET_CONTRAST);
    ssd1306_send_cmd(contrast);
}

void ssd1306_invert(bool invert) {
    ssd1306_send_cmd(invert ? SSD1306_CMD_INVERT_DISPLAY : SSD1306_CMD_NORMAL_DISPLAY);
}

