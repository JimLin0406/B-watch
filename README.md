# B-watch


## Debug
```
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
```
rgb_ele_order應為RBG。
LVGL - lv_color_make(R,G,B)
從LVGL庫更改。
