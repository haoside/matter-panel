#include <esp_log.h>
#include <esp_matter.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;

static const char *TAG = "matter_panel";

// === 配置 ===
#define CHANNEL_COUNT 4
// XIAO ESP32-C6 可用引脚: D3=GPIO5, D4=GPIO6, D5=GPIO7, D6=GPIO8
static const int channel_gpios[CHANNEL_COUNT] = {GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_8};
static uint16_t channel_endpoint_ids[CHANNEL_COUNT] = {0};

// 状态 LED GPIO（XIAO D0=GPIO2）
static const gpio_num_t STATUS_LED_GPIO = GPIO_NUM_2;

// === 全局状态枚举 ===
typedef enum {
    PANEL_UNPAIRED,
    PANEL_PAIRING,
    PANEL_ONLINE,
    PANEL_OFFLINE,
    PANEL_ERROR
} panel_status_t;

static panel_status_t g_panel_status = PANEL_UNPAIRED;

// === 单路状态 ===
typedef enum {
    CH_OFF,
    CH_ON,
    CH_PENDING,
    CH_NO_RESPONSE
} channel_power_state_t;

static channel_power_state_t g_channel_states[CHANNEL_COUNT];

// === 日志宏（统一字段）===
#define LOG_ACTION(ch, action, result, err, lat) \
    ESP_LOGI(TAG, "ACTION deviceId=%s fw=0.1.0 ch=%d action=%s result=%s err=%s latencyMs=%d", \
             esp_matter::node::get_id_string(), (ch), (action), (result), (err), (lat))

// === LED 控制 ===
static void led_set(gpio_num_t gpio, bool on) {
    gpio_set_level(gpio, on ? 1 : 0);
}

static void led_blink(gpio_num_t gpio, int times, int period_ms) {
    for (int i = 0; i < times; i++) {
        led_set(gpio, true);
        vTaskDelay(pdMS_TO_TICKS(period_ms / 2));
        led_set(gpio, false);
        vTaskDelay(pdMS_TO_TICKS(period_ms / 2));
    }
}

static void led_blink_slow(gpio_num_t gpio) {
    led_blink(gpio, 1, 1000);
}

static void led_blink_fast(gpio_num_t gpio) {
    led_blink(gpio, 2, 300);
}

// === 状态 LED 任务 ===
static void status_led_task(void *pv) {
    while (1) {
        switch (g_panel_status) {
            case PANEL_UNPAIRED:
                // 蓝色慢闪
                led_set(STATUS_LED_GPIO, true);
                vTaskDelay(pdMS_TO_TICKS(500));
                led_set(STATUS_LED_GPIO, false);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            case PANEL_PAIRING:
                // 蓝色快闪
                led_set(STATUS_LED_GPIO, true);
                vTaskDelay(pdMS_TO_TICKS(100));
                led_set(STATUS_LED_GPIO, false);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
            case PANEL_ONLINE:
                // 蓝色常亮（或根据硬件支持不亮，仅屏显）
                led_set(STATUS_LED_GPIO, true);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            case PANEL_OFFLINE:
                // 灰态/熄灭
                led_set(STATUS_LED_GPIO, false);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            case PANEL_ERROR:
                // 橙色慢闪（单色 LED 用不同闪烁节奏区分）
                led_set(STATUS_LED_GPIO, true);
                vTaskDelay(pdMS_TO_TICKS(800));
                led_set(STATUS_LED_GPIO, false);
                vTaskDelay(pdMS_TO_TICKS(800));
                break;
        }
    }
}

// === 单路 LED 更新 ===
static void update_channel_led(int ch) {
    // 若硬件支持每路独立 LED，此处映射；
    // MVP 阶段：通过日志 + 全局状态灯表达
    switch (g_channel_states[ch]) {
        case CH_OFF:
            ESP_LOGD(TAG, "CH%d LED: off", ch + 1);
            break;
        case CH_ON:
            ESP_LOGD(TAG, "CH%d LED: on (warm)", ch + 1);
            break;
        case CH_PENDING:
            ESP_LOGD(TAG, "CH%d LED: pending blink", ch + 1);
            break;
        case CH_NO_RESPONSE:
            ESP_LOGD(TAG, "CH%d LED: no_response blink", ch + 1);
            break;
    }
}

static esp_err_t app_attribute_update_cb(
    attribute::callback_type_t type,
    uint16_t endpoint_id,
    uint32_t cluster_id,
    uint32_t attribute_id,
    esp_matter_attr_val_t *val,
    void *priv_data)
{
    if (type != PRE_UPDATE) return ESP_OK;
    if (cluster_id != OnOff::Id) return ESP_OK;
    if (attribute_id != OnOff::Attributes::OnOff::Id) return ESP_OK;

    for (int i = 0; i < CHANNEL_COUNT; i++) {
        if (endpoint_id == channel_endpoint_ids[i]) {
            bool state = val->val.b;
            g_channel_states[i] = state ? CH_ON : CH_OFF;
            gpio_set_level((gpio_num_t)channel_gpios[i], state ? 1 : 0);
            update_channel_led(i);
            LOG_ACTION(i + 1, state ? "turn_on" : "turn_off", "success", "none", 0);
            ESP_LOGI(TAG, "CH%d (endpoint %d) -> %s", i + 1, endpoint_id, state ? "ON" : "OFF");
            break;
        }
    }
    return ESP_OK;
}

static esp_err_t app_identification_cb(
    identification::callback_type_t type,
    uint16_t endpoint_id,
    uint8_t effect_id,
    uint8_t effect_variant,
    void *priv_data)
{
    ESP_LOGI(TAG, "Identify endpoint %d effect %d", endpoint_id, effect_id);
    return ESP_OK;
}

extern "C" void app_main()
{
    esp_err_t err = ESP_OK;

    // 初始化 GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        io_conf.pin_bit_mask |= (1ULL << channel_gpios[i]);
        gpio_set_level((gpio_num_t)channel_gpios[i], 0);
        g_channel_states[i] = CH_OFF;
    }
    io_conf.pin_bit_mask |= (1ULL << STATUS_LED_GPIO);
    gpio_config(&io_conf);
    gpio_set_level(STATUS_LED_GPIO, 0);

    // 创建 Matter Node
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    if (!node) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        g_panel_status = PANEL_ERROR;
        return;
    }

    // 创建 4 路 On/Off Light endpoint
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        on_off_light::config_t light_config;
        endpoint_t *ep = on_off_light::create(node, &light_config, ENDPOINT_FLAG_NONE, NULL);
        if (!ep) {
            ESP_LOGE(TAG, "Failed to create endpoint for CH%d", i + 1);
            continue;
        }
        channel_endpoint_ids[i] = endpoint::get_id(ep);
        ESP_LOGI(TAG, "CH%d endpoint: %d", i + 1, channel_endpoint_ids[i]);
    }

    // 启动 Matter
    err = esp_matter::start(app_attribute_update_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Matter: %d", err);
        g_panel_status = PANEL_ERROR;
        return;
    }

    g_panel_status = PANEL_UNPAIRED;
    ESP_LOGI(TAG, "Matter panel started (%d channels), waiting for commissioning...", CHANNEL_COUNT);

    // 启动状态 LED 任务
    xTaskCreate(status_led_task, "status_led", 2048, NULL, 1, NULL);
}
