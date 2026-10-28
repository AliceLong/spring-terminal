#pragma once
#include <esp_lcd_touch.h>
#include <lvgl.h>
namespace desk_gap {
bool StartBufferedTouch(esp_lcd_touch_handle_t touch, lv_indev_t* input, int width, int height);
void ReadBufferedTouch(lv_indev_t* input, lv_indev_data_t* data);
void ConsumeClosingTouch();
}  // namespace desk_gap
