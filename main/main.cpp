#include "Arduino.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_touch_cst816s.h"
#include <Wire.h>
#include "Adafruit_ADS1X15.h"
#include "Adafruit_TCS34725.h"
#include "ee.c"

// 定義觸控屏的引腳配置
#define TOUCH_SDA GPIO_NUM_6
#define TOUCH_SCL GPIO_NUM_7
#define TOUCH_RST GPIO_NUM_13
#define TOUCH_IRQ GPIO_NUM_5
#define I2C_TOUCH_FREQ 50000


// 定義顯示屏配置
#define LCD_PIN_CLK   GPIO_NUM_10
#define LCD_PIN_MISO  GPIO_NUM_12  
#define LCD_PIN_MOSI  GPIO_NUM_11
#define LCD_PIN_CS    GPIO_NUM_9
#define LCD_PIN_RST   GPIO_NUM_14
#define LCD_PIN_DC    GPIO_NUM_8
#define LCD_PIN_BL    GPIO_NUM_2  

#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (40 * 1000 * 1000)
// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS           8
#define EXAMPLE_LCD_PARAM_BITS         8

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2

// Using SPI2 in the example
#define LCD_HOST  SPI2_HOST

/**********************
 *  STATIC PROTOTYPES
 **********************/
static SemaphoreHandle_t touch_mux = xSemaphoreCreateBinary();
static void btn_event_info(lv_event_t * e);
static const char *TAG = "example";

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}


static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

static void example_lvgl_touch_cb(lv_indev_drv_t * drv, lv_indev_data_t * data)
{   
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_mux, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }

    if (xSemaphoreTake(touch_mux, 0) != pdTRUE){
        return;
    }

    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    esp_lcd_touch_handle_t panel_handle = (esp_lcd_touch_handle_t) drv->user_data;

    /* Read touch controller data */
    esp_lcd_touch_read_data(panel_handle);

    /* Get coordinates */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(panel_handle, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}


static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

void gc9a01_init(void) {
    static lv_disp_draw_buf_t disp_buf;
    static lv_disp_drv_t disp_drv;

    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << LCD_PIN_BL,
        .mode = GPIO_MODE_OUTPUT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = LCD_PIN_MISO,
        .sclk_io_num = LCD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 80 * sizeof(lv_color_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = LCD_PIN_CS,
        .dc_gpio_num = LCD_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .on_color_trans_done = example_notify_lvgl_flush_ready,
        .user_ctx = &disp_drv,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };

    ESP_LOGI(TAG, "Install GC9A01 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));

    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(LCD_PIN_BL, 1);

    

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = (lv_color_t*)heap_caps_malloc(240 * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = (lv_color_t*)heap_caps_malloc(240 * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);

    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, 240 * 20);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);



    /*Initialization of the touch component*/

    // 初始化 I2C 驅動
    esp_lcd_touch_handle_t tp = NULL;
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();

    // I2C 配置結構
    i2c_config_t i2c_conf  = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TOUCH_SDA,
        .scl_io_num = TOUCH_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master ={
            .clk_speed = I2C_TOUCH_FREQ
        },
    };

    // 安裝 I2C 驅動
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));


    // 初始化 I2C 實例
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_NUM_0, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 240,
        .y_max = 240,
        .rst_gpio_num = TOUCH_RST,
        .int_gpio_num = TOUCH_IRQ,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        // .interrupt_callback = example_lvgl_touch_cb,
    };

    ESP_LOGI(TAG, "Initialize touch controller CST816S");

    // 初始化觸摸控制器
    esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp);
    // ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp));


    static lv_indev_drv_t indev_drv;    // Input device driver (Touch)
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = example_lvgl_touch_cb;
    indev_drv.user_data = tp;
    lv_indev_drv_register(&indev_drv);


    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));
}



// 設置簡單的 LVGL 畫面
void lvgl_init_bcakground() {
    // 取得當前顯示的螢幕
    // lv_obj_t *scr = lv_disp_get_scr_act(NULL);  

    // // 設置螢幕背景色為白色 (RGB 顏色)
    // lv_obj_set_style_bg_color(scr, lv_color_make(171, 110, 248), LV_PART_MAIN);  // 設置背景為白色
    // lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);    // 設置背景為不透明

    // // 創建一個簡單的按鈕
    // lv_obj_t *btn = lv_btn_create(lv_scr_act());
    // lv_obj_set_size(btn, 120, 60);
    // lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    // // 設置按鈕背景顏色為紅色
    // lv_obj_set_style_bg_color(btn, lv_color_make(56, 198, 92), LV_PART_MAIN);  // LV_COLOR_RED 也可以直接使用
    // lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);  // 設置背景為不透明

    // // 為按鈕添加標籤
    // lv_obj_t *label = lv_label_create(btn);
    // lv_label_set_text(label, "Hello world!");
    // lv_obj_center(label);

    /* Create simple background */
    LV_IMG_DECLARE(ee);
    lv_obj_t *bg_img = lv_img_create(lv_scr_act());
    lv_img_set_src(bg_img, &ee); // 設置背景圖片
    lv_obj_set_size(bg_img, 240, 240); // 設置大小填滿整個屏幕
    lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0); 
}




// Tile window for introduce page, signal page, record page
static lv_obj_t * tile1;
static lv_obj_t * tile2;
static lv_obj_t * tile3;
static lv_obj_t * tile4;
static lv_obj_t * tile5;

void GUI_tileview(void) {
    lv_obj_t * tv = lv_tileview_create(lv_scr_act());
    lv_obj_set_style_bg_color(tv, lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tv, LV_OPA_TRANSP, LV_PART_MAIN);

    /*Tile1: just a label*/
    tile1 = lv_tileview_add_tile(tv, 0, 0,LV_DIR_RIGHT|LV_DIR_BOTTOM);

    /*Tile2: a button*/
    tile2 = lv_tileview_add_tile(tv, 1, 0,LV_DIR_RIGHT|LV_DIR_LEFT);


    /*Tile3: a button*/
    tile3 = lv_tileview_add_tile(tv, 2, 0,LV_DIR_LEFT);

    /*Tile4: time setting*/
    tile4 = lv_tileview_add_tile(tv, 0, 1,LV_DIR_TOP|LV_DIR_BOTTOM);
    
    /*Tile5: battery */
    tile5 = lv_tileview_add_tile(tv, 0, 2,LV_DIR_TOP);

}

/**
 * Start animation on an event
 */
static int anim_step = 0;
lv_obj_t * anim_label;

void GUI_Info_page(void){
      anim_label = lv_label_create(tile1);
      lv_label_set_text(anim_label, "Press the button!");
      lv_obj_align(anim_label, LV_ALIGN_CENTER, 0, -20);

      lv_obj_t * label = lv_label_create(tile1);
      lv_label_set_text(label, "B-WATCH");
      lv_obj_align(label, LV_ALIGN_CENTER, 0, -90);

      lv_obj_t * label2 = lv_label_create(tile1);
      lv_label_set_text(label2, "-Manual-");
      lv_obj_align(label2, LV_ALIGN_CENTER, 0, -75);

      lv_obj_t * btn = lv_btn_create(tile1);
      lv_obj_set_size(btn, 120, 60);
      lv_obj_align(btn, LV_ALIGN_CENTER, 0, 60);

      lv_obj_t *btn_label = lv_label_create(btn);
      lv_label_set_text(btn_label, "Ckick!");
      lv_obj_center(btn_label);

      lv_obj_add_event_cb(btn, btn_event_info, LV_EVENT_CLICKED, NULL);
};


static void anim_x_cb(void * var, int32_t v){
    lv_obj_set_x((lv_obj_t *)var, v);
}
static void anim_del_cb(lv_anim_t * a) {
    lv_obj_t * obj = (lv_obj_t *)a->var;  
    lv_obj_del(obj); // 刪除動畫label
}

static void btn_event_info(lv_event_t * e) {
    // Serial.println("Button clicked!");
    lv_obj_t * btn = lv_event_get_target(e); // 獲取按鈕對象
    lv_anim_t a;
    lv_anim_t b;

    // 根據當前步驟顯示不同的文本
    if (anim_step == 0) {
        // 動畫右滑消失
        lv_anim_init(&a);
        lv_anim_set_var(&a, anim_label);
        lv_anim_set_values(&a, lv_obj_get_x(anim_label), (120+lv_obj_get_width(anim_label)/2) ); // 從中間到右邊
        lv_anim_set_time(&a, 500); // 設置動畫時間
        lv_anim_set_exec_cb(&a, anim_x_cb); // 設置執行回調
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out); // 設置運動路徑
        lv_anim_set_ready_cb(&a, anim_del_cb); // 完成動畫，刪掉label
        lv_anim_start(&a);

        //建立新動畫label
        anim_label = lv_label_create(tile1);
        lv_label_set_text(anim_label, "Swipe down to\n check the time"); 
        lv_obj_set_style_text_align(anim_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(anim_label, LV_ALIGN_CENTER, 0, -20);

        // 動畫左滑消失
        lv_anim_init(&b);
        lv_anim_set_var(&b, anim_label);
        lv_anim_set_values(&b, (-120-lv_obj_get_width(anim_label)/2), 0); // 從左邊進入到中間
        lv_anim_set_time(&b, 500); // 設置動畫時間
        lv_anim_set_exec_cb(&b, anim_x_cb); // 設置執行回調
        lv_anim_set_path_cb(&b, lv_anim_path_ease_in); // 設置運動路徑

        
        lv_anim_start(&b);

        anim_step = 1;
    } 
    else if(anim_step == 1){
        // 動畫右滑消失
        lv_anim_init(&a);
        lv_anim_set_var(&a, anim_label);
        lv_anim_set_values(&a, lv_obj_get_x(anim_label), (120+lv_obj_get_width(anim_label)/2) ); // 從中間到右邊
        lv_anim_set_time(&a, 500); // 設置動畫時間
        lv_anim_set_exec_cb(&a, anim_x_cb); // 設置執行回調
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out); // 設置運動路徑
        lv_anim_set_ready_cb(&a, anim_del_cb); // 完成動畫，刪掉label
        lv_anim_start(&a);

        //建立新動畫label
        anim_label = lv_label_create(tile1);
        lv_label_set_text(anim_label, "Swipe right to\n check the signal"); 
        lv_obj_set_style_text_align(anim_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(anim_label, LV_ALIGN_CENTER, 0, -20);

        // 動畫左滑消失→
        lv_anim_init(&b);
        lv_anim_set_var(&b, anim_label);
        lv_anim_set_values(&b, (-120-lv_obj_get_width(anim_label)/2), 0); // 從左邊進入到中間
        lv_anim_set_time(&b, 500); // 設置動畫時間
        lv_anim_set_exec_cb(&b, anim_x_cb); // 設置執行回調
        lv_anim_set_path_cb(&b, lv_anim_path_ease_in); // 設置運動路徑

        lv_anim_start(&b);

        anim_step = 2;
    }

    else if(anim_step == 2){
        // 動畫右滑消失
        lv_anim_init(&a);
        lv_anim_set_var(&a, anim_label);
        lv_anim_set_values(&a, lv_obj_get_x(anim_label), (120+lv_obj_get_width(anim_label)/2) ); // 從中間到右邊
        lv_anim_set_time(&a, 500); // 設置動畫時間
        lv_anim_set_exec_cb(&a, anim_x_cb); // 設置執行回調
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out); // 設置運動路徑
        lv_anim_set_ready_cb(&a, anim_del_cb); // 完成動畫，刪掉label
        lv_anim_start(&a);

        //建立新動畫label
        anim_label = lv_label_create(tile1);
        lv_label_set_text(anim_label, "Swipe right again\n to enable the function"); 
        lv_obj_set_style_text_align(anim_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(anim_label, LV_ALIGN_CENTER, 0, -20);

        // 動畫左滑消失→
        lv_anim_init(&b);
        lv_anim_set_var(&b, anim_label);
        lv_anim_set_values(&b, (-120-lv_obj_get_width(anim_label)/2), 0); // 從左邊進入到中間
        lv_anim_set_time(&b, 500); // 設置動畫時間
        lv_anim_set_exec_cb(&b, anim_x_cb); // 設置執行回調
        lv_anim_set_path_cb(&b, lv_anim_path_ease_in); // 設置運動路徑

        lv_anim_start(&b);
        anim_step = 3;
    }
    else if(anim_step == 3){
        // 動畫右滑消失
        lv_anim_init(&a);
        lv_anim_set_var(&a, anim_label);
        lv_anim_set_values(&a, lv_obj_get_x(anim_label), (120+lv_obj_get_width(anim_label)/2) ); // 從中間到右邊
        lv_anim_set_time(&a, 500); // 設置動畫時間
        lv_anim_set_exec_cb(&a, anim_x_cb); // 設置執行回調
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out); // 設置運動路徑
        lv_anim_set_ready_cb(&a, anim_del_cb); // 完成動畫，刪掉label
        lv_anim_start(&a);

        //建立新動畫label
        anim_label = lv_label_create(tile1);
        lv_label_set_text(anim_label, "Button \"Record\"\n is only usable\n after pressing \"Start\".");
        lv_obj_set_style_text_align(anim_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(anim_label, LV_ALIGN_CENTER, 0, -20);

        // 動畫左滑消失→
        lv_anim_init(&b);
        lv_anim_set_var(&b, anim_label);
        lv_anim_set_values(&b, (-120-lv_obj_get_width(anim_label)/2), 0); // 從左邊進入到中間
        lv_anim_set_time(&b, 500); // 設置動畫時間
        lv_anim_set_exec_cb(&b, anim_x_cb); // 設置執行回調
        lv_anim_set_path_cb(&b, lv_anim_path_ease_in); // 設置運動路徑

        lv_anim_start(&b);
        anim_step = 0;
    }
    else{
        return;
    }
}

void I2CTask(void *parameter) {
    while (true) {
        vTaskDelay(300 / portTICK_PERIOD_MS); 
    }   
}



void UITask(void *parameter) {
  /***********Build GUI below***********/
    lvgl_init_bcakground();
    GUI_tileview();
    GUI_Info_page();
  /***********Build GUI above***********/

    while (true) {
        lv_timer_handler(); /* 執行 LVGL 的 GUI 任務 */
        vTaskDelay(5 / portTICK_PERIOD_MS); 
    }
}

// 主應用程式入口
extern "C" void app_main() {
    initArduino();  // 初始化 Arduino 库

    // 初始化 LVGL 和顯示器
    gc9a01_init();

    vTaskDelay(5 / portTICK_PERIOD_MS); 
    

        // // 創建 I2C 任務並將其固定在 Core 0
    xTaskCreatePinnedToCore(
      I2CTask,       // 任務函數
      "I2CTask",     // 任務名稱
      4096,         // 堆疊大小
      NULL,         // 傳遞給任務的參數
      1,            // 任務優先級
      NULL,         // 任務句柄
      0             // Core 0
    );
    ESP_LOGI(TAG, "I2CTask setup done!");
    delay(5);
     // 創建 UI 任務並將其固定在 Core 1
    xTaskCreatePinnedToCore(
      UITask,       // 任務函數
      "UITask",     // 任務名稱
      4096,         // 堆疊大小
      NULL,         // 傳遞給任務的參數
      1,            // 任務優先級
      NULL,         // 任務句柄
      1             // Core 1
    );
    ESP_LOGI(TAG, "UITask setup done!");
}
