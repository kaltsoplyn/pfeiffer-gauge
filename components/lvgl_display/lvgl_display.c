// #include "lvgl_display.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "lvgl_display.h"

#define LCD_PIXEL_CLOCK_HZ    (80 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL 1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL


// Pin configuration for Waveshare ESP32-C6-LCD-1.47
#define LCD_PIN_NUM_SCLK      7
#define LCD_PIN_NUM_MOSI      6
#define LCD_PIN_NUM_DC        15
#define LCD_PIN_NUM_RST       21
#define LCD_PIN_NUM_CS        14
#define LCD_PIN_NUM_BK_LIGHT  22

// Display resolution
#define LCD_H_RES           320
#define LCD_V_RES           172

static const char *TAG = "LVGL";

static lv_color_t green;
static lv_color_t magenta;

static lv_obj_t *screen1 = NULL;
static lv_obj_t *screen2 = NULL;
static lv_obj_t *pressure_label = NULL;
static lv_obj_t *ipaddr_label = NULL;
static lv_obj_t *int_temp_label = NULL;
static lv_obj_t *buffer_full_label = NULL;
static bool is_backlight_on = false;

void lvgl_display_backlight(bool on) {
    gpio_set_level(LCD_PIN_NUM_BK_LIGHT, on ? LCD_BK_LIGHT_ON_LEVEL : LCD_BK_LIGHT_OFF_LEVEL);
    is_backlight_on = on;
}


esp_err_t lvgl_display_init(void)
{
    // Backlight configuration
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_NUM_BK_LIGHT
    };

    // SPI bus configuration
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_NUM_SCLK,
        .mosi_io_num = LCD_PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * 2
    };

    // LCD IO configuration
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_NUM_DC,
        .cs_gpio_num = LCD_PIN_NUM_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    
    // ST7789 panel configuration with proper initialization commands
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_NUM_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
        .vendor_config = NULL
    };

    // Error checks
    esp_err_t ret = gpio_config(&bk_gpio_config);
    ret = ret == ESP_OK ? spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO) : ret;
    ret = ret == ESP_OK ? esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle) : ret;
    ret = ret == ESP_OK ? esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle) : ret;
    if ( ret != ESP_OK ) {
        ESP_LOGE(TAG, "Failed to configure display!\n%s", esp_err_to_name(ret));
        return ret;
    }
    
    // Reset the panel
    ret = esp_lcd_panel_reset(panel_handle);
    
    // Initialize the panel with custom commands
    ret = ret == ESP_OK ? esp_lcd_panel_init(panel_handle) : ret;

    if ( ret != ESP_OK ) {
        ESP_LOGE(TAG, "Failed to intitialize display!\n%s", esp_err_to_name(ret));
        return ret;
    }
    
    // Specific configuration for Waveshare 1.47" display
    ret = esp_lcd_panel_invert_color(panel_handle, true);
    ret = ret == ESP_OK ? esp_lcd_panel_swap_xy(panel_handle, true) : ret;
    ret = ret == ESP_OK ? esp_lcd_panel_mirror(panel_handle, false, true) : ret;
    ret = ret == ESP_OK ? esp_lcd_panel_set_gap(panel_handle, 0, 34) : ret; // Y offset for 172px height
    if ( ret != ESP_OK ) {
        ESP_LOGE(TAG, "Failed to apply specific config to ST7789 display!\n%s", esp_err_to_name(ret));
        return ret;
    }

    green = lv_color_make(255,0,255);
    magenta = lv_color_make(255,255,0);
    
    // Turn on display
    ret = esp_lcd_panel_disp_on_off(panel_handle, true);
    if ( ret != ESP_OK ) {
        ESP_LOGE(TAG, "Failed to turn on display!\n%s", esp_err_to_name(ret));
        return ret;
    }

    // LVGL initialization
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,
        .task_stack = 12288,
        .task_affinity = -1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5
    };
    ret = lvgl_port_init(&lvgl_cfg);
    if ( ret != ESP_OK ) {
        ESP_LOGE(TAG, "Failed initialize LVGL portation!\n%s", esp_err_to_name(ret));
        return ret;
    }

    // LVGL display configuration
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * 40, // Larger buffer
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = true,    // Match panel settings
            .mirror_x = false,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,  // Enable DMA
        }
    };
    
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    
    // Set default theme and colors
    lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE), 
                         lv_palette_main(LV_PALETTE_RED), 
                         LV_THEME_DEFAULT_DARK, LV_FONT_DEFAULT);
    
    // Create screen1
    screen1 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen1, lv_color_black(), 0);

    // pressure label
    pressure_label = lv_label_create(screen1);
    lv_obj_set_style_text_font(pressure_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(pressure_label, green, 0);
    lv_obj_align(pressure_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(pressure_label, PRESSURE_GAUGE_NAME);

    // ip address label
    ipaddr_label = lv_label_create(screen1);
    lv_obj_set_style_text_font(ipaddr_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ipaddr_label, lv_color_white(), 0);
    lv_obj_set_style_bg_color(ipaddr_label, lv_color_make(200,200,200), 0);
    lv_obj_set_style_bg_opa(ipaddr_label, LV_OPA_COVER, 0);
    lv_obj_set_size(ipaddr_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(ipaddr_label, 5, 0);
    lv_obj_align(ipaddr_label, LV_ALIGN_BOTTOM_RIGHT, -10, -6);
    lv_label_set_text(ipaddr_label, "IP: 0.0.0.0");
    
    // internal temperature label
    int_temp_label = lv_label_create(screen1);
    lv_obj_set_style_text_font(int_temp_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(int_temp_label, magenta, 0);
    lv_obj_set_size(int_temp_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(int_temp_label, 5, 0);
    lv_obj_align(int_temp_label, LV_ALIGN_BOTTOM_LEFT, 10, -6);
    lv_label_set_text(int_temp_label, "[SoC Temp]");

    // buffer full percentage label
    buffer_full_label = lv_label_create(screen1);
    lv_obj_set_style_text_font(buffer_full_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(buffer_full_label, green, 0);
    lv_obj_set_size(buffer_full_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(buffer_full_label, 5, 0);
    lv_obj_align(buffer_full_label, LV_ALIGN_TOP_RIGHT, -10, 6);
    lv_label_set_text(buffer_full_label, "[data buffer %%]");

    lv_scr_load(screen1);
    
    // Enable backlight
    lvgl_display_backlight(true);

    return ret;
}


void lvgl_display_label_text(lv_obj_t* element , const char* text) {
    if (lvgl_port_lock(0)) {
        if(lv_obj_is_valid(element) && lv_obj_check_type(element, &lv_label_class)) {
            lv_label_set_text(element, text);
        }
        lvgl_port_unlock();
    }
}

void lvgl_set_text_color(lv_obj_t* element , lv_color_t color) {
    if (lvgl_port_lock(0)) {
        if(lv_obj_is_valid(element)) {
            lv_obj_set_style_text_color(element, color, 0);
        }
        lvgl_port_unlock();
    }
}

void lvgl_display_pressure(float pressure, float FS) {
    if (!pressure_label) return;

    static char buffer[50];
    if (pressure > 0) {
        snprintf(buffer, sizeof(buffer), "%.2f mbar", pressure);
    } else {
        snprintf(buffer, sizeof(buffer), " -- ");
    }

    if (pressure < 0.1 * FS || pressure > 0.9 * FS) {
        lvgl_set_text_color(pressure_label, magenta);
    } else {
        lvgl_set_text_color(pressure_label, green);
    }
    
    lvgl_display_label_text(pressure_label, buffer);
}

void lvgl_display_ipaddr(const char* ipaddr) {
    if (!ipaddr_label) return;
    ipaddr ? lvgl_display_label_text(ipaddr_label, ipaddr) : lvgl_display_label_text(ipaddr_label, "IP: --");
    lv_obj_set_size(ipaddr_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    //lv_obj_align(ipaddr_label, LV_ALIGN_BOTTOM_RIGHT, -10, -6);
}

void lvgl_display_internal_temp(const char* temp) {
    if (!int_temp_label) return;
    temp ? lvgl_display_label_text(int_temp_label, temp) : lvgl_display_label_text(int_temp_label, "T: -- °C");
    lv_obj_set_size(int_temp_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    //lv_obj_align(int_temp_label, LV_ALIGN_BOTTOM_LEFT, 10, -6);
}

void lvgl_display_buffer_pc(int buf_pc) {
    if (!buffer_full_label) return;
    static char text[20];
    snprintf(text, sizeof(text), "buffer: %d %%", buf_pc);
    buf_pc < 0 || buf_pc > 100 ? lvgl_display_label_text(buffer_full_label, "buffer: -- %%") : lvgl_display_label_text(buffer_full_label, text);
    lv_obj_set_size(buffer_full_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    if (buf_pc > 80) {
        lvgl_set_text_color(buffer_full_label, magenta);
    } else {
        lvgl_set_text_color(buffer_full_label, green);
    }
    //lv_obj_align(int_temp_label, LV_ALIGN_BOTTOM_LEFT, -10, 6);
}


// void lvgl_display_update(void) {
//     lv_timer_handler();
// }