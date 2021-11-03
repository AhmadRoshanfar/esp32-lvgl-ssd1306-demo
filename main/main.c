/* LVGL Example project
 *
 * Basic project to test LVGL on ESP32 based projects.
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"

/* Littlevgl specific */
#include "lvgl/lvgl.h"
#include "lvgl_helpers.h"

#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
#if defined CONFIG_LV_USE_DEMO_WIDGETS
#include "lv_examples/src/lv_demo_widgets/lv_demo_widgets.h"
#elif defined CONFIG_LV_USE_DEMO_KEYPAD_AND_ENCODER
#include "lv_examples/src/lv_demo_keypad_and_encoder/lv_demo_keypad_and_encoder.h"
#elif defined CONFIG_LV_USE_DEMO_BENCHMARK
#include "lv_examples/src/lv_demo_benchmark/lv_demo_benchmark.h"
#elif defined CONFIG_LV_USE_DEMO_STRESS
#include "lv_examples/src/lv_demo_stress/lv_demo_stress.h"
#else
#error "No demo application selected."
#endif
#endif

/*********************
 *      DEFINES
 *********************/
#define TAG "demo"
#define LV_TICK_PERIOD_MS 1

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
static void create_demo_application(void);

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main()
{

    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
     * Otherwise there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */
    xTaskCreatePinnedToCore(guiTask, "gui", 4096 * 2, NULL, 0, NULL, 1);
}

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;

static void guiTask(void *pvParameter)
{

    (void)pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    lvgl_driver_init();

    static lv_color_t buf1[DISP_BUF_SIZE];

    /* Use double buffered when not working with monochrome displays */
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    static lv_color_t buf2[DISP_BUF_SIZE];
#else
    static lv_color_t *buf2 = NULL;
#endif

    static lv_disp_buf_t disp_buf;

    uint32_t size_in_px = DISP_BUF_SIZE;

#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820 || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D

    /* Actual size in pixels, not bytes. */
    size_in_px *= 8;
#endif

    /* Initialize the working buffer depending on the selected display.
     * NOTE: buf2 == NULL when using monochrome displays. */
    lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;

    /* When using a monochrome display we need to register the callbacks:
     * - rounder_cb
     * - set_px_cb */
#ifdef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    disp_drv.rounder_cb = disp_driver_rounder;
    disp_drv.set_px_cb = disp_driver_set_px;
#endif

    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Register an input device when enabled on the menuconfig */
#if CONFIG_LV_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);
#endif

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    /* Create the demo application */
    create_demo_application();

    while (1)
    {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    /* A task should NEVER return */
    vTaskDelete(NULL);
}

static void create_demo_application(void)
{
    /*Create a screen*/
    lv_obj_t *scr = lv_obj_create(NULL, NULL);
    lv_scr_load(scr); /*Load the screen*/

    static lv_style_t label_persian_style;
    lv_style_init(&label_persian_style);
    lv_style_set_text_font(&label_persian_style, LV_STATE_DEFAULT, &lv_font_dejavu_16_persian_hebrew); /*Set persian font*/

    lv_obj_t *persian_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_add_style(persian_label, LV_LABEL_PART_MAIN, &label_persian_style);
    lv_label_set_text(persian_label, "سلام");
    lv_obj_align(persian_label, NULL, LV_ALIGN_CENTER, 0, -25);

    static lv_style_t label_icon_style;
    lv_style_init(&label_icon_style);
    lv_style_set_text_font(&label_icon_style, LV_STATE_DEFAULT, &lv_font_montserrat_12_subpx); /*Set persian font*/

    lv_obj_t *settings_icon_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_add_style(settings_icon_label, LV_LABEL_PART_MAIN, &label_icon_style);
    lv_label_set_text(settings_icon_label, LV_SYMBOL_SETTINGS);
    lv_obj_align(settings_icon_label, NULL, LV_ALIGN_CENTER, -55, -23);

    lv_obj_t *battery_icon_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_add_style(battery_icon_label, LV_LABEL_PART_MAIN, &label_icon_style);
    lv_label_set_text(battery_icon_label, LV_SYMBOL_BATTERY_2);
    lv_obj_align(battery_icon_label, NULL, LV_ALIGN_CENTER, -35, -23);

    lv_obj_t *bell_icon_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_add_style(bell_icon_label, LV_LABEL_PART_MAIN, &label_icon_style);
    lv_label_set_text(bell_icon_label, LV_SYMBOL_BELL);
    lv_obj_align(bell_icon_label, NULL, LV_ALIGN_CENTER, 35, -23);

    lv_obj_t *wifi_icon_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_add_style(wifi_icon_label, LV_LABEL_PART_MAIN, &label_icon_style);
    lv_label_set_text(wifi_icon_label, LV_SYMBOL_WIFI);
    lv_obj_align(wifi_icon_label, NULL, LV_ALIGN_CENTER, 55, -23);

    lv_obj_t *label2 = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_long_mode(label2, LV_LABEL_LONG_SROLL_CIRC); /*Circular scroll*/
    lv_obj_set_width(label2, 150);
    lv_label_set_text(label2, "It is a circularly scrolling text. ");
    lv_obj_align(label2, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *led1 = lv_led_create(lv_scr_act(), NULL);
    lv_obj_set_pos(led1, 45, 50);
    lv_obj_set_size(led1, 12, 12);
    lv_led_off(led1);

    lv_obj_t *led2 = lv_led_create(lv_scr_act(), NULL);
    lv_obj_set_pos(led2, 60, 50);
    lv_obj_set_size(led2, 12, 12);
    lv_led_on(led2);
}
static void lv_tick_task(void *arg)
{
    (void)arg;

    lv_tick_inc(LV_TICK_PERIOD_MS);
}
