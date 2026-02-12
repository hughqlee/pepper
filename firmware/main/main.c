#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "pepper_frames.h"

static const char *TAG = "noise_meter";

static const float DBFS_FLOOR = -96.0f;
static const float DBFS_CEIL = 0.0f;
static const float SPL_EST_MIN = 30.0f;
static const float SPL_EST_MAX = 120.0f;
static const float DISPLAY_SMOOTH_ALPHA = 0.15f;
static const uint32_t MIC_SAMPLE_RATE_HZ = 22050;
static const size_t MIC_READ_SAMPLES = 512;
static const uint32_t RMS_WINDOW_MS = 300;

/* One-point calibration */
static const float SPL_CAL_REF_DBFS = -50.9f;
static const float SPL_CAL_REF_DB = 60.0f;

static const float SPL_THRESHOLD_SLEEPY = 45.0f;
static const float SPL_THRESHOLD_DIZZY = 60.0f;
static const float SPL_HYSTERESIS_DB = 2.0f;

static const uint32_t ANIM_FRAME_INTERVAL_MS = 180;
static const uint32_t TEXT_UPDATE_INTERVAL_MS = 180;

typedef enum {
    ANIM_SET_SLEEPY = 0,
    ANIM_SET_WORK,
    ANIM_SET_DIZZY,
} anim_set_t;

static lv_obj_t *g_img = NULL;
static lv_obj_t *g_label = NULL;
static SemaphoreHandle_t g_state_mutex = NULL;
static esp_codec_dev_handle_t g_mic = NULL;
static float g_db_spl = 50.0f;
static bool g_mic_ready = false;
static int g_anim_idx = 0;
static int g_anim_phase = 0;
static anim_set_t g_anim_set = ANIM_SET_WORK;

static float clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static float rms_to_dbfs(double rms)
{
    if (rms < 1.0) {
        return DBFS_FLOOR;
    }

    const double dbfs = 20.0 * log10(rms / 32768.0);
    return clampf((float)dbfs, DBFS_FLOOR, DBFS_CEIL);
}

static size_t rms_window_samples(void)
{
    size_t samples = (size_t)((MIC_SAMPLE_RATE_HZ * RMS_WINDOW_MS) / 1000U);
    if (samples < MIC_READ_SAMPLES) {
        samples = MIC_READ_SAMPLES;
    }
    return samples;
}

static float dbfs_to_db_spl_est(float dbfs)
{
    const float offset = SPL_CAL_REF_DB - SPL_CAL_REF_DBFS;
    return clampf(dbfs + offset, SPL_EST_MIN, SPL_EST_MAX);
}

static anim_set_t choose_anim_set(float db_spl, bool mic_ready, anim_set_t current_set)
{
    if (!mic_ready) {
        return ANIM_SET_WORK;
    }

    switch (current_set) {
    case ANIM_SET_SLEEPY:
        if (db_spl >= (SPL_THRESHOLD_SLEEPY + SPL_HYSTERESIS_DB)) {
            return ANIM_SET_WORK;
        }
        return ANIM_SET_SLEEPY;
    case ANIM_SET_DIZZY:
        if (db_spl <= (SPL_THRESHOLD_DIZZY - SPL_HYSTERESIS_DB)) {
            return ANIM_SET_WORK;
        }
        return ANIM_SET_DIZZY;
    case ANIM_SET_WORK:
    default:
        if (db_spl <= (SPL_THRESHOLD_SLEEPY - SPL_HYSTERESIS_DB)) {
            return ANIM_SET_SLEEPY;
        }
        if (db_spl >= (SPL_THRESHOLD_DIZZY + SPL_HYSTERESIS_DB)) {
            return ANIM_SET_DIZZY;
        }
        return ANIM_SET_WORK;
    }
}

static void get_frames_for_set(anim_set_t set, const pepper_img_dsc_t *const **frames, size_t *count)
{
    static const pepper_img_dsc_t *const sleepy_frames[] = {
        &img_sleepy_0,
        &img_sleepy_1,
        &img_sleepy_2,
    };
    static const pepper_img_dsc_t *const work_frames[] = {
        &img_work_0,
        &img_work_1,
        &img_work_2,
    };
    static const pepper_img_dsc_t *const dizzy_frames[] = {
        &img_dizzy_0,
        &img_dizzy_1,
        &img_dizzy_2,
    };

    switch (set) {
    case ANIM_SET_SLEEPY:
        *frames = sleepy_frames;
        *count = sizeof(sleepy_frames) / sizeof(sleepy_frames[0]);
        break;
    case ANIM_SET_DIZZY:
        *frames = dizzy_frames;
        *count = sizeof(dizzy_frames) / sizeof(dizzy_frames[0]);
        break;
    case ANIM_SET_WORK:
    default:
        *frames = work_frames;
        *count = sizeof(work_frames) / sizeof(work_frames[0]);
        break;
    }
}

static void set_image_src(const pepper_img_dsc_t *dsc)
{
#if LVGL_VERSION_MAJOR >= 9
    lv_image_set_src(g_img, dsc);
#else
    lv_img_set_src(g_img, dsc);
#endif
}

static const char *anim_set_name(anim_set_t set)
{
    switch (set) {
    case ANIM_SET_SLEEPY:
        return "sleepy";
    case ANIM_SET_DIZZY:
        return "dizzy";
    case ANIM_SET_WORK:
    default:
        return "work";
    }
}

static void text_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);

    float db_spl = 50.0f;
    bool mic_ready = false;

    if (xSemaphoreTake(g_state_mutex, 0) == pdTRUE) {
        db_spl = g_db_spl;
        mic_ready = g_mic_ready;
        xSemaphoreGive(g_state_mutex);
    }

    if (!mic_ready) {
        lv_label_set_text(g_label, "MIC starting...");
        return;
    }

    lv_label_set_text_fmt(g_label, "%.1f dB SPL (%s)", db_spl, anim_set_name(g_anim_set));
}

static void anim_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);

    float db_spl = 50.0f;
    bool mic_ready = false;

    if (xSemaphoreTake(g_state_mutex, 0) == pdTRUE) {
        db_spl = g_db_spl;
        mic_ready = g_mic_ready;
        xSemaphoreGive(g_state_mutex);
    }

    static const uint8_t frame_seq[] = {0, 1, 2, 1};
    const anim_set_t desired = choose_anim_set(db_spl, mic_ready, g_anim_set);
    const pepper_img_dsc_t *const *frames = NULL;
    size_t frame_count = 0;

    if (desired != g_anim_set) {
        g_anim_set = desired;
        g_anim_idx = 0;
        g_anim_phase = 0;
        get_frames_for_set(g_anim_set, &frames, &frame_count);
        set_image_src(frames[g_anim_idx]);
        return;
    }

    get_frames_for_set(g_anim_set, &frames, &frame_count);
    g_anim_phase = (g_anim_phase + 1) % (int)(sizeof(frame_seq) / sizeof(frame_seq[0]));
    g_anim_idx = frame_seq[g_anim_phase];
    if (g_anim_idx >= (int)frame_count) {
        g_anim_idx = (int)frame_count - 1;
    }
    set_image_src(frames[g_anim_idx]);
}

static void ui_create(void)
{
    lv_obj_clean(lv_screen_active());
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

#if LVGL_VERSION_MAJOR >= 9
    g_img = lv_image_create(lv_screen_active());
#else
    g_img = lv_img_create(lv_screen_active());
#endif
    set_image_src(&img_work_0);
    lv_obj_center(g_img);

    g_label = lv_label_create(g_img);
    lv_label_set_text(g_label, "MIC starting...");
    lv_obj_set_style_text_color(g_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(g_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_label, LV_OPA_60, 0);
    lv_obj_set_style_pad_left(g_label, 6, 0);
    lv_obj_set_style_pad_right(g_label, 6, 0);
    lv_obj_set_style_pad_top(g_label, 3, 0);
    lv_obj_set_style_pad_bottom(g_label, 3, 0);
    lv_obj_align(g_label, LV_ALIGN_TOP_MID, 0, 10);

    lv_timer_create(anim_timer_cb, ANIM_FRAME_INTERVAL_MS, NULL);
    lv_timer_create(text_timer_cb, TEXT_UPDATE_INTERVAL_MS, NULL);
}

static void mic_task(void *arg)
{
    LV_UNUSED(arg);

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = MIC_SAMPLE_RATE_HZ,
        .mclk_multiple = 0,
    };
    int16_t samples[MIC_READ_SAMPLES];
    const size_t target_rms_samples = rms_window_samples();
    double rms_sum_sq = 0.0;
    size_t rms_count = 0;

    if (bsp_audio_init(NULL) != ESP_OK) {
        ESP_LOGE(TAG, "bsp_audio_init failed");
        vTaskDelete(NULL);
    }

    g_mic = bsp_audio_codec_microphone_init();
    if (g_mic == NULL) {
        ESP_LOGE(TAG, "bsp_audio_codec_microphone_init failed");
        vTaskDelete(NULL);
    }

    if (esp_codec_dev_open(g_mic, &fs) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "esp_codec_dev_open failed");
        vTaskDelete(NULL);
    }

    esp_codec_dev_set_in_gain(g_mic, 24.0f);
    ESP_LOGI(TAG, "SPL calibration: %.1f dBFS -> %.1f dB SPL", SPL_CAL_REF_DBFS, SPL_CAL_REF_DB);
    ESP_LOGI(TAG, "RMS window: %lu ms (%u Hz, %u samples/read, target=%u samples)",
             (unsigned long)RMS_WINDOW_MS,
             (unsigned int)MIC_SAMPLE_RATE_HZ,
             (unsigned int)MIC_READ_SAMPLES,
             (unsigned int)target_rms_samples);

    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_mic_ready = true;
        xSemaphoreGive(g_state_mutex);
    }

    while (1) {
        const int ret = esp_codec_dev_read(g_mic, samples, sizeof(samples));
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "esp_codec_dev_read failed: %d", ret);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        for (size_t i = 0; i < MIC_READ_SAMPLES; i++) {
            const double s = (double)samples[i];
            rms_sum_sq += s * s;
        }
        rms_count += MIC_READ_SAMPLES;

        if (rms_count < target_rms_samples) {
            continue;
        }

        const double rms = sqrt(rms_sum_sq / (double)rms_count);
        const float dbfs = rms_to_dbfs(rms);
        const float db_spl = dbfs_to_db_spl_est(dbfs);

        rms_sum_sq = 0.0;
        rms_count = 0;

        if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
            g_db_spl += DISPLAY_SMOOTH_ALPHA * (db_spl - g_db_spl);
            xSemaphoreGive(g_state_mutex);
        }
    }
}

void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        }
    };

    g_state_mutex = xSemaphoreCreateMutex();
    if (g_state_mutex == NULL) {
        ESP_LOGE(TAG, "state mutex allocation failed");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
    bsp_display_brightness_set(50);

    bsp_display_lock(0);
    ui_create();
    bsp_display_unlock();

    if (xTaskCreate(mic_task, "mic_task", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "failed to create mic task");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
