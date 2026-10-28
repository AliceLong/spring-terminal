#include "display/lcd_display.h"
#include "esp_lcd_co5300.h"
#include "wifi_board.h"

#include "application.h"
#include "axp2101.h"
#include "button.h"
#include "codecs/box_audio_codec.h"
#include "config.h"
#include "i2c_device.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "power_save_timer.h"

#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <algorithm>
#include "assets.h"
#include "day1_probe.h"
#include "display/lvgl_display/lvgl_font.h"
#include "esp_io_expander_tca9554.h"
#include "settings.h"
#include "spring_power.h"
#include "spring_subtitle.h"
#include "spring_touch_input.h"

#include <esp_lcd_touch_cst9217.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#define TAG "WaveshareEsp32s3TouchAMOLED2inch16"

class Pmic : public Axp2101 {
public:
    // Battery telemetry is optional; a transient shared-bus timeout must not reset the game.
    bool TryGetBattery(int& level, bool& charging, bool& discharging) {
        uint8_t status_reg = 0x01, level_reg = 0xA4, status = 0, percentage = 0;
        auto result = i2c_master_transmit_receive(i2c_device_, &status_reg, 1, &status, 1, 20);
        if (result == ESP_OK)
            result = i2c_master_transmit_receive(i2c_device_, &level_reg, 1, &percentage, 1, 20);
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Battery telemetry unavailable: %s", esp_err_to_name(result));
            return false;
        }
        level = percentage;
        charging = ((status >> 5) & 3) == 1;
        discharging = ((status >> 5) & 3) == 2;
        return true;
    }
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        WriteReg(0x22, 0b110);  // PWRON > OFFLEVEL as POWEROFF Source enable
        WriteReg(0x27, 0x10);   // hold 4s to power off

        // Disable All DCs but DC1
        WriteReg(0x80, 0x01);
        // Disable All LDOs
        WriteReg(0x90, 0x00);
        WriteReg(0x91, 0x00);

        // Set DC1 to 3.3V
        WriteReg(0x82, (3300 - 1500) / 100);

        // Set ALDO1 to 3.3V
        WriteReg(0x92, (3300 - 500) / 100);

        // Enable ALDO1(MIC)
        WriteReg(0x90, 0x01);

        WriteReg(0x64, 0x02);  // CV charger voltage setting to 4.1V

        WriteReg(0x61, 0x02);  // set Main battery precharge current to 50mA
        WriteReg(0x62, 0x08);  // set Main battery charger current to 400mA ( 0x08-200mA,
                               // 0x09-300mA, 0x0A-400mA )
        WriteReg(0x63, 0x01);  // set Main battery term charge current to 25mA
    }
};

#define LCD_OPCODE_WRITE_CMD (0x02ULL)
#define LCD_OPCODE_READ_CMD (0x03ULL)
#define LCD_OPCODE_WRITE_COLOR (0x32ULL)

static const co5300_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, (uint8_t[]){0x00}, 0, 600},  // Sleep out

    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1C, (uint8_t[]){0xA0}, 1, 0},

    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x36, (uint8_t[]){0xA0}, 1, 0},
    {0x29, (uint8_t[]){0x00}, 0, 600},
};

// 在waveshare_amoled_1_75类之前添加新的显示类
class CustomLcdDisplay : public SpiLcdDisplay {
    bool game_screen_sleep_ = false;

public:
    static void rounder_event_cb(lv_event_t* e) {
        lv_area_t* area = (lv_area_t*)lv_event_get_param(e);
        uint16_t x1 = area->x1;
        uint16_t x2 = area->x2;

        uint16_t y1 = area->y1;
        uint16_t y2 = area->y2;

        // round the start of coordinate down to the nearest 2M number
        area->x1 = (x1 >> 1) << 1;
        area->y1 = (y1 >> 1) << 1;
        // round the end of coordinate up to the nearest 2N+1 number
        area->x2 = ((x2 >> 1) << 1) + 1;
        area->y2 = ((y2 >> 1) << 1) + 1;
    }

    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t panel_handle,
                     int width, int height, int offset_x, int offset_y, bool mirror_x,
                     bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x,
                        mirror_y, swap_xy) {
        // Note: UI customization should be done in SetupUI(), not in constructor
        // to ensure lvgl objects are created before accessing them
    }

    virtual void SetupUI() override {
        // Call parent SetupUI() first to create all lvgl objects
        SpiLcdDisplay::SetupUI();

        // Game labels must work before Wi-Fi/AI activation. Load only the local font,
        // not speech models or downloads; normal activation still applies full assets.
        auto& assets = Assets::GetInstance();
        void* index_data = nullptr;
        size_t index_size = 0;
        if (assets.GetAssetData("index.json", index_data, index_size)) {
            std::unique_ptr<cJSON, decltype(&cJSON_Delete)> index(
                cJSON_ParseWithLength(static_cast<const char*>(index_data), index_size),
                cJSON_Delete);
            auto* font = index ? cJSON_GetObjectItem(index.get(), "text_font") : nullptr;
            auto* version = index ? cJSON_GetObjectItem(index.get(), "version") : nullptr;
            void* font_data = nullptr;
            size_t font_size = 0;
            if (cJSON_IsString(font) && (!cJSON_IsNumber(version) || version->valuedouble <= 1) &&
                assets.GetAssetData(font->valuestring, font_data, font_size)) {
                auto local_font = std::make_shared<LvglCBinFont>(font_data);
                if (desk_gap::SetGameTextFont(local_font) && SetTextFont(local_font))
                    ESP_LOGI(TAG, "LOCAL_GAME_FONT CJK verified; line_height=%u bytes=%u",
                             local_font->font()->line_height, static_cast<unsigned>(font_size));
                else
                    ESP_LOGE(TAG, "LOCAL_GAME_FONT missing Chinese glyphs");
            }
        }

        DisplayLockGuard lock(this);
        lv_obj_add_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
        lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        desk_gap::StartDay1Probe(container_);
        ConfigureBottomText();
    }

    void ConfigureBottomText() {
        if (top_bar_)
            lv_obj_add_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
        if (status_bar_)
            lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
        // Listening/speaking are shown by the gardener animation; keep the
        // native text state internally and in debug logs, without a scene badge.
        if (status_label_)
            lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        if (!bottom_bar_ || !chat_message_label_)
            return;
        // Keep real voice subtitles above the game HUD, in their own bottom row.
        lv_obj_set_parent(bottom_bar_, lv_layer_top());
        lv_obj_move_foreground(bottom_bar_);
        lv_obj_set_size(bottom_bar_, 480, 32);
        lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_pad_all(bottom_bar_, 0, 0);
        lv_obj_set_style_pad_left(bottom_bar_, 12, 0);
        lv_obj_set_style_bg_color(bottom_bar_, lv_color_hex(0x18362B), 0);
        lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_80, 0);
        lv_obj_remove_flag(bottom_bar_, LV_OBJ_FLAG_CLICKABLE);
        // Subtitles belong to the game overlay, independent of light/dark themes.
        auto text_font = desk_gap::GetGameTextFont();
        if (!text_font) text_font = GetTheme() ? GetTheme()->GetTextFont() : nullptr;
        spring_ui::BindSubtitleFont(chat_message_label_, text_font ? text_font->font() : nullptr);
        lv_obj_remove_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
        hide_subtitle_ = false;
        const char* subtitle = lv_label_get_text(chat_message_label_);
        if (subtitle && subtitle[0])
            lv_obj_remove_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        const auto* font = lv_obj_get_style_text_font(chat_message_label_, LV_PART_MAIN);
        const int scale =
            std::min(256, 26 * 256 / std::max(1, static_cast<int>(font->line_height)));
        lv_obj_set_style_transform_scale_x(chat_message_label_, scale, 0);
        lv_obj_set_style_transform_scale_y(chat_message_label_, scale, 0);
        lv_obj_set_style_transform_pivot_x(chat_message_label_, 0, 0);
        lv_obj_set_style_transform_pivot_y(chat_message_label_, 0, 0);
        lv_obj_set_width(chat_message_label_, 456 * 256 / scale);
        lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(chat_message_label_, LV_ALIGN_TOP_LEFT, 0, 3);
        lv_obj_remove_flag(chat_message_label_, LV_OBJ_FLAG_CLICKABLE);
    }

    void SetGameScreenPower(bool sleep) {
        DisplayLockGuard lock(this);
        if (sleep == game_screen_sleep_)
            return;
        game_screen_sleep_ = sleep;
        if (sleep) {
            lv_display_enable_invalidation(display_, false);
            esp_lcd_panel_disp_on_off(panel_, false);
        } else {
            esp_lcd_panel_disp_on_off(panel_, true);
            lv_display_enable_invalidation(display_, true);
            lv_obj_invalidate(lv_screen_active());
            lv_obj_invalidate(lv_layer_top());
        }
    }

    void SetStatus(const char* status) override {
        // Idle clock ticks must not replace the debugging voice state.
        const bool clock = status && status[0] >= '0' && status[0] <= '9' &&
                           std::string(status).find(':') != std::string::npos;
        SpiLcdDisplay::SetStatus(clock ? "待机" : status);
        DisplayLockGuard lock(this);
        ConfigureBottomText();
    }

    void SetChatMessage(const char* role, const char* content) override {
        desk_gap::GardenerTranscript(role, content);
        SpiLcdDisplay::SetChatMessage(role, content);
        if (role && content && content[0] &&
            (std::string(role) == "user" || std::string(role) == "assistant"))
            desk_gap::PowerVoiceProgress();
        if (role && std::string(role) == "user" && content && content[0] &&
            Application::GetInstance().GetDeviceState() == kDeviceStateListening)
            SetStatus("等待回复");
        DisplayLockGuard lock(this);
        ConfigureBottomText();
    }

    void SetTheme(Theme* theme) override {
        SpiLcdDisplay::SetTheme(theme);
        DisplayLockGuard lock(this);
        ConfigureBottomText();
    }

    void SetHideSubtitle(bool /*hide*/) override {
        // This game's ASR/TTS row stays available even if an asset pack hides it.
        SpiLcdDisplay::SetHideSubtitle(false);
        DisplayLockGuard lock(this);
        ConfigureBottomText();
    }
};

class CustomBacklight : public Backlight {
public:
    CustomBacklight(esp_lcd_panel_io_handle_t panel_io) : Backlight(), panel_io_(panel_io) {}

protected:
    esp_lcd_panel_io_handle_t panel_io_;

    virtual void SetBrightnessImpl(uint8_t brightness) override {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        uint8_t data[1] = {((uint8_t)((255 * brightness) / 100))};
        int lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
        esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
    }
};

class WaveshareEsp32s3TouchAMOLED2inch16 : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Pmic* pmic_ = nullptr;
    Button boot_button_;
    CustomLcdDisplay* display_;
    CustomBacklight* backlight_;
    esp_io_expander_handle_t io_expander = NULL;
    void InitializePowerSaveTimer() {
        // Board-local policy replaces the old USB-dependent idle timer. Keep
        // wake-word audio and the game worker running in every mode.
        esp_pm_config_t pm = {.max_freq_mhz = 240, .min_freq_mhz = 80, .light_sleep_enable = false};
        ESP_ERROR_CHECK(esp_pm_configure(&pm));
        desk_gap::ConfigureGamePower([this](spring_power::Mode mode) {
            const bool sleep = mode == spring_power::Mode::Sleep;
            display_->SetGameScreenPower(sleep);
            backlight_->SetBrightness(sleep ? 0 : mode == spring_power::Mode::Quiet ? 15 : 40);
            WifiBoard::SetPowerSaveLevel(mode == spring_power::Mode::Play
                                             ? PowerSaveLevel::BALANCED
                                             : PowerSaveLevel::LOW_POWER);
            ESP_LOGI(TAG, "GAME_POWER mode=%s brightness=%u USB-independent; AFE retained",
                     spring_power::Name(mode),
                     sleep                               ? 0
                     : mode == spring_power::Mode::Quiet ? 15
                                                         : 40);
        });
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeTca9554(void) {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_bus_, I2C_ADDRESS, &io_expander);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "TCA9554 create returned error");
        ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_4, IO_EXPANDER_INPUT);
        ESP_ERROR_CHECK(ret);
    }

    void InitializeAxp2101() {
        ESP_LOGI(TAG, "Init AXP2101");
        pmic_ = new Pmic(i2c_bus_, 0x34);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = EXAMPLE_PIN_NUM_LCD_PCLK;
        buscfg.data0_io_num = EXAMPLE_PIN_NUM_LCD_DATA0;
        buscfg.data1_io_num = EXAMPLE_PIN_NUM_LCD_DATA1;
        buscfg.data2_io_num = EXAMPLE_PIN_NUM_LCD_DATA2;
        buscfg.data3_io_num = EXAMPLE_PIN_NUM_LCD_DATA3;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            const bool wake_only = desk_gap::GamePowerMode() == spring_power::Mode::Sleep;
            desk_gap::PowerActivity("button");
            if (!wake_only)
                app.ToggleChatState();
        });

#if CONFIG_USE_DEVICE_AEC
        boot_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
            }
        });
#endif
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS;
        io_config.dc_gpio_num = GPIO_NUM_NC;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 32;
        io_config.lcd_param_bits = 8;
        io_config.flags.quad_mode = true;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        const co5300_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(co5300_lcd_init_cmd_t),
            .flags = {
                .use_qspi_interface = 1,
            }};

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void*)&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(panel_io, &panel_config, &panel));
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        display_ = new CustomLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                        DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                        DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        backlight_ = new CustomBacklight(panel_io);
        backlight_->SetBrightness(40);
    }

    void InitializeTouch() {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = PIN_NUM_TOUCH_RST,
            .int_gpio_num = PIN_NUM_TOUCH_INT,
            .levels =
                {
                    .reset = 0,
                    .interrupt = 0,
                },
            .flags =
                {
                    .swap_xy = 0,
                    .mirror_x = 0,
                    .mirror_y = 0,
                },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();
        tp_io_config.scl_speed_hz = 400 * 1000;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_LOGI(TAG, "Initialize touch controller");
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst9217(tp_io_handle, &tp_cfg, &tp));
        // The bundled driver reads chip information in command mode but does not
        // return to normal mode or ACK consumed reports. Match the board vendor's
        // SensorLib CST92xx normal-mode/read-ACK sequence, without changing pins.
        const uint8_t normal_mode = 0x09;
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(tp_io_handle, 0xD1, &normal_mode, 1));
        gpio_intr_disable(PIN_NUM_TOUCH_INT);
        lvgl_port_lock(0);
        auto* touch_input = lv_indev_create();
        ESP_ERROR_CHECK(touch_input ? ESP_OK : ESP_ERR_NO_MEM);
        lv_indev_set_type(touch_input, LV_INDEV_TYPE_POINTER);
        lv_indev_set_disp(touch_input, lv_display_get_default());
        lv_indev_set_user_data(touch_input, tp);
        lv_indev_set_read_cb(touch_input, [](lv_indev_t* input, lv_indev_data_t* data) {
            const auto started_us = esp_timer_get_time();
            struct PollTiming {
                int64_t start;
                lv_indev_data_t* data;
                ~PollTiming() {
                    desk_gap::RecordTouchPoll(start, data->state == LV_INDEV_STATE_PRESSED);
                }
            } timing{started_us, data};
            auto handle = static_cast<esp_lcd_touch_handle_t>(lv_indev_get_user_data(input));
            data->state = LV_INDEV_STATE_RELEASED;
            const auto read_result = esp_lcd_touch_read_data(handle);
            const uint8_t ack[] = {0x00, 0xAB};
            const auto ack_result = esp_lcd_panel_io_tx_param(handle->io, 0xD0, ack, sizeof(ack));
            // Idle/invalid reports are not fatal; never reuse stale coordinates.
            if (read_result != ESP_OK || ack_result != ESP_OK) {
                static int64_t last_warning_us = 0;
                const auto now = esp_timer_get_time();
                if (read_result != ESP_ERR_INVALID_RESPONSE && now - last_warning_us > 5000000) {
                    ESP_LOGW(TAG, "Touch read=%s ack=%s", esp_err_to_name(read_result),
                             esp_err_to_name(ack_result));
                    last_warning_us = now;
                }
                return;
            }
            esp_lcd_touch_point_data_t point{};
            uint8_t count = 0;
            if (esp_lcd_touch_get_data(handle, &point, &count, 1) == ESP_OK && count &&
                point.x < DISPLAY_WIDTH && point.y < DISPLAY_HEIGHT) {
                data->point.x = point.x;
                data->point.y = point.y;
                data->state = LV_INDEV_STATE_PRESSED;
            }
        });
        lv_timer_set_period(lv_indev_get_read_timer(touch_input), 20);
        if (desk_gap::StartBufferedTouch(tp, touch_input, DISPLAY_WIDTH, DISPLAY_HEIGHT)) {
            lv_indev_set_read_cb(touch_input, desk_gap::ReadBufferedTouch);
            lv_indev_set_mode(touch_input, LV_INDEV_MODE_EVENT);
        }
        lv_indev_add_event_cb(
            touch_input,
            [](lv_event_t* event) {
                auto* input = static_cast<lv_indev_t*>(lv_event_get_target(event));
                lv_point_t point;
                lv_indev_get_point(input, &point);
                ESP_LOGI(TAG, "TOUCH_INPUT x=%ld y=%ld", static_cast<long>(point.x),
                         static_cast<long>(point.y));
                auto* self =
                    static_cast<WaveshareEsp32s3TouchAMOLED2inch16*>(lv_event_get_user_data(event));
                (void)self;  // Buffered sampler owns idle timing and wake-only touches.
            },
            LV_EVENT_PRESSED, this);
        lv_indev_add_event_cb(
            touch_input,
            [](lv_event_t* event) {
                auto* input = static_cast<lv_indev_t*>(lv_event_get_target(event));
                lv_point_t point;
                lv_indev_get_point(input, &point);
                ESP_LOGI(TAG, "TOUCH_RELEASE x=%ld y=%ld", static_cast<long>(point.x),
                         static_cast<long>(point.y));
            },
            LV_EVENT_RELEASED, nullptr);
        lvgl_port_unlock();
        ESP_LOGI(TAG, "Touch panel initialized successfully");
    }

    // 初始化工具
    void InitializeTools() {
        desk_gap::RegisterGameTools();
        Application::GetInstance().SetVoiceCompletionCallback(desk_gap::VoiceInteractionCompleted);
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.system.reconfigure_wifi",
                           "End this conversation and enter WiFi configuration mode.\n"
                           "**CAUTION** You must ask the user to confirm this action.",
                           PropertyList(), [this](const PropertyList& properties) {
                               EnterWifiConfigMode();
                               return true;
                           });
    }

public:
    WaveshareEsp32s3TouchAMOLED2inch16() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
#if CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_75
        InitializeTca9554();
#endif
        InitializeAxp2101();
        InitializeSpi();
        InitializeDisplay();
        InitializePowerSaveTimer();
        InitializeTouch();
        InitializeButtons();
        InitializeTools();
        desk_gap::SetPickupWakeCallback([] {});
        desk_gap::StartMotionInput(i2c_bus_);
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override { return backlight_; }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (!pmic_->TryGetBattery(level, charging, discharging))
            return false;
        return true;
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        const auto state = Application::GetInstance().GetDeviceState();
        if (level == PowerSaveLevel::PERFORMANCE &&
            (state == kDeviceStateConnecting || state == kDeviceStateListening ||
             state == kDeviceStateSpeaking))
            desk_gap::PowerActivity("voice_connect");
        WifiBoard::SetPowerSaveLevel(level);
    }
};

DECLARE_BOARD(WaveshareEsp32s3TouchAMOLED2inch16);
