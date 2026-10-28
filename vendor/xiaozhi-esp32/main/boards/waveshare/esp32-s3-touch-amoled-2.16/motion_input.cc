#include <driver/i2c_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>
#include "application.h"
#include "day1_probe.h"
#include "game/pickup_detector.h"
#include "game/shake_detector.h"
#include "spring_power.h"
#include "spring_tilt.h"
namespace desk_gap {
namespace {
i2c_master_dev_handle_t imu = nullptr;
std::function<void()> pickup_wake;
void MotionLoop(void*) {
    ShakeDetector detector;
    PickupDetector pickup;
    unsigned errors = 0;
    uint64_t last_log = 0, stable_since = 0;
    float smooth_x = 0, smooth_y = 0, smooth_z = 1, last_x = 0, last_y = 0, last_z = 1;
    // Give board/audio setup time to settle, then keep input alive even offline.
    vTaskDelay(pdMS_TO_TICKS(5000));
    while (true) {
        uint8_t reg = 0x35, bytes[6]{};
        const auto result = i2c_master_transmit_receive(imu, &reg, 1, bytes, sizeof(bytes), 20);
        if (result == ESP_OK) {
            errors = 0;
            float axis[3]{};
            for (int i = 0; i < 3; ++i)
                axis[i] = static_cast<int16_t>(static_cast<uint16_t>(bytes[i * 2]) |
                                               (static_cast<uint16_t>(bytes[i * 2 + 1]) << 8)) /
                          4096.0f;
            const uint64_t now = esp_timer_get_time() / 1000;
            const float norm = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
            const float jerk = std::abs(axis[0] - last_x) + std::abs(axis[1] - last_y) +
                               std::abs(axis[2] - last_z);
            last_x = axis[0];
            last_y = axis[1];
            last_z = axis[2];
            smooth_x += .12f * (axis[0] - smooth_x);
            smooth_y += .12f * (axis[1] - smooth_y);
            smooth_z += .12f * (axis[2] - smooth_z);
            if (norm < .85f || norm > 1.15f || jerk > .18f)
                stable_since = now;
            const float reference = std::sqrt(smooth_x * smooth_x + smooth_z * smooth_z);
            const float angle = spring_ui::LateralTilt(smooth_x, smooth_y, smooth_z);
            MotionSample(angle, now - stable_since >= 500 && reference > .5f);
            if (pickup.Sample(now, axis[0], axis[1], axis[2]) && pickup_wake) {
                PowerActivity("pickup");
                Application::GetInstance().Schedule([] {
                    pickup_wake();
                    PickupWakeFeedback();
                    ESP_LOGI("DeskGapMotion", "PICKUP_WAKE main-task callback; no currency reward");
                });
            }
            if (detector.Sample(now, axis[0], axis[1], axis[2]))
                MotionShakeDetected();
            if (now - last_log >= 5000) {
                ESP_LOGI("DeskGapMotion", "ACCEL x=%.2f y=%.2f z=%.2f g lateral=%.1f stable=%d",
                         axis[0], axis[1], axis[2], angle,
                         now - stable_since >= 500 && reference > .5f);
                last_log = now;
            }
        } else if (++errors == 10) {
            SetMotionAvailable(false);
            ESP_LOGW("DeskGapMotion", "IMU unavailable; game continues without shake input");
            vTaskDelete(nullptr);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
}  // namespace
void SetPickupWakeCallback(std::function<void()> callback) { pickup_wake = std::move(callback); }
void StartMotionInput(i2c_master_bus_handle_t bus) {
    // Register addresses/ranges verified against the board vendor's SensorQMI8658.hpp.
    // Existing shared I2C bus and pins are retained. No interrupt line is required.
    for (uint16_t address : {0x6Bu, 0x6Au}) {
        if (i2c_master_probe(bus, address, 30) != ESP_OK)
            continue;
        i2c_device_config_t config{};
        config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        config.device_address = address;
        config.scl_speed_hz = 400000;
        if (i2c_master_bus_add_device(bus, &config, &imu) != ESP_OK)
            continue;
        uint8_t reg = 0, who = 0;
        if (i2c_master_transmit_receive(imu, &reg, 1, &who, 1, 30) != ESP_OK || who != 0x05) {
            i2c_master_bus_rm_device(imu);
            imu = nullptr;
            continue;
        }
        const uint8_t setup[][2] = {{0x08, 0x00},
                                    {0x02, 0x40},
                                    {0x03, 0x27},
                                    {0x08, 0x01}};  // increment, ±8g, 62.5Hz, accel only
        bool ok = true;
        for (const auto& command : setup)
            if (i2c_master_transmit(imu, command, 2, 30) != ESP_OK) {
                ok = false;
                break;
            }
        if (ok && xTaskCreate(MotionLoop, "desk_gap_imu", 3072, nullptr, 1, nullptr) == pdPASS) {
            SetMotionAvailable(true);
            ESP_LOGI(
                "DeskGapMotion",
                "QMI8658 address=0x%02x initialized; shake threshold requires hardware calibration",
                address);
            return;
        }
        i2c_master_bus_rm_device(imu);
        imu = nullptr;
    }
    ESP_LOGW("DeskGapMotion", "No usable QMI8658; shake input disabled");
}
}  // namespace desk_gap
