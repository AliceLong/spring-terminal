#include "spring_touch_input.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "day1_probe.h"
#include "game/touch_buffer.h"
#include "spring_power.h"
namespace desk_gap {
namespace {
spring::TouchBuffer reports;
spring::TouchTransitionGuard transition;
portMUX_TYPE report_lock = portMUX_INITIALIZER_UNLOCKED;
esp_lcd_touch_handle_t touch_handle;
lv_indev_t* touch_input;
int screen_width, screen_height;
void SampleTouch(void*) {
    spring::TouchSample sample;
    spring::TouchFrameFilter filter;
    int64_t last_warning_us = 0;
    while (true) {
        const auto start = esp_timer_get_time();
        const auto read = esp_lcd_touch_read_data(touch_handle);
        const uint8_t ack[] = {0x00, 0xAB};
        const auto acknowledged =
            esp_lcd_panel_io_tx_param(touch_handle->io, 0xD0, ack, sizeof(ack));
        esp_lcd_touch_point_data_t point{};
        uint8_t count = 0;
        bool valid = read == ESP_OK && acknowledged == ESP_OK;
        if (valid)
            valid = esp_lcd_touch_get_data(touch_handle, &point, &count, 1) == ESP_OK;
        if (valid && count && (point.x >= screen_width || point.y >= screen_height))
            valid = false;
        bool publish = filter.Update(valid, count > 0, point.x, point.y, start, sample);
        if (publish && PowerTouch(sample.pressed))
            publish = false;
        if (!valid && read != ESP_ERR_INVALID_RESPONSE && start - last_warning_us > 5000000) {
            ESP_LOGW("SpringTouch", "read=%s ack=%s", esp_err_to_name(read),
                     esp_err_to_name(acknowledged));
            last_warning_us = start;
        }
        bool wake = false;
        {
            sample.sampled_us = start;
            portENTER_CRITICAL(&report_lock);
            if (!transition.Blocked(valid, count > 0, start) && publish)
                wake = reports.Push(sample);
            else
                publish = false;
            portEXIT_CRITICAL(&report_lock);
        }
        RecordTouchPoll(start, sample.pressed);
        if (wake || (publish && sample.pressed))
            lvgl_port_task_wake(LVGL_PORT_EVENT_TOUCH, touch_input);
        vTaskDelay(pdMS_TO_TICKS(GamePowerMode() == spring_power::Mode::Sleep ? 40 : 10));
    }
}
}  // namespace
void ConsumeClosingTouch() {
    const auto now = esp_timer_get_time();
    portENTER_CRITICAL(&report_lock);
    transition.Begin(now);
    reports.Cancel(now);
    portEXIT_CRITICAL(&report_lock);
    if (touch_input) lvgl_port_task_wake(LVGL_PORT_EVENT_TOUCH, touch_input);
}
bool StartBufferedTouch(esp_lcd_touch_handle_t touch, lv_indev_t* input, int width, int height) {
    touch_handle = touch;
    touch_input = input;
    screen_width = width;
    screen_height = height;
    if (xTaskCreate(SampleTouch, "spring_touch", 4096, nullptr, 2, nullptr) != pdPASS) {
        ESP_LOGE("SpringTouch", "Sampler allocation failed; retaining original synchronous input");
        return false;
    }
    ESP_LOGI("SpringTouch",
             "Independent sampler ready; bounded edge queue, event-driven LVGL input");
    return true;
}
void ReadBufferedTouch(lv_indev_t* input, lv_indev_data_t* data) {
    spring::TouchSample sample;
    bool fresh;
    size_t pending;
    unsigned overflows;
    portENTER_CRITICAL(&report_lock);
    fresh = reports.Pop(sample);
    pending = reports.Pending();
    overflows = reports.Overflows();
    portEXIT_CRITICAL(&report_lock);
    if (sample.reset) {
        lv_indev_reset(input, nullptr);
        ESP_LOGW("SpringTouch", "Cancelled interrupted touch; waiting for physical release");
    }
    data->point.x = sample.x;
    data->point.y = sample.y;
    data->state = sample.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->continue_reading = pending > 0;
    if (fresh && sample.edge)
        ESP_LOGI("SpringTouch", "DELIVER state=%s lag_us=%llu pending=%u overflows=%u",
                 sample.pressed ? "down" : "up",
                 static_cast<unsigned long long>(esp_timer_get_time() - sample.sampled_us),
                 static_cast<unsigned>(pending), overflows);
}
}  // namespace desk_gap
