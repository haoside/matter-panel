#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_matter.h>

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;

static const char *TAG = "matter_panel";

#define MATTER_PANEL_CHANNEL_COUNT 4
static const int panel_gpios[MATTER_PANEL_CHANNEL_COUNT] = {GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_8};
static uint16_t panel_endpoint_ids[MATTER_PANEL_CHANNEL_COUNT] = {0};

static const gpio_num_t PANEL_STATUS_LED_GPIO = GPIO_NUM_2;

typedef enum {
    PANEL_STATUS_UNPAIRED,
    PANEL_STATUS_PAIRING,
    PANEL_STATUS_ONLINE,
    PANEL_STATUS_OFFLINE,
    PANEL_STATUS_ERROR
} panel_status_enum_t;

static panel_status_enum_t g_panel_status = PANEL_STATUS_UNPAIRED;

typedef enum {
    CHANNEL_STATE_OFF,
    CHANNEL_STATE_ON,
    CHANNEL_STATE_PENDING,
    CHANNEL_STATE_NO_RESPONSE
} channel_state_enum_t;

static channel_state_enum_t g_channel_states[MATTER_PANEL_CHANNEL_COUNT];

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

static void status_led_task(void *pv) {
    while (1) {
        switch (g_panel_status) {
            case PANEL_STATUS_UNPAIRED:
                led_set(PANEL_STATUS_LED_GPIO, true);
                vTaskDelay(pdMS_TO_TICKS(500));
                led_set(PANEL_STATUS_LED_GPIO, false);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            case PANEL_STATUS_PAIRING:
                led_set(PANEL_STATUS_LED_GPIO, true);
                vTaskDelay(pdMS_TO_TICKS(100));
                led_set(PANEL_STATUS_LED_GPIO, false);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
            case PANEL_STATUS_ONLINE:
                led_set(PANEL_STATUS_LED_GPIO, true);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            case PANEL_STATUS_OFFLINE:
                led_set(PANEL_STATUS_LED_GPIO, false);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            case PANEL_STATUS_ERROR:
                led_set(PANEL_STATUS_LED_GPIO, true);
                vTaskDelay(pdMS_TO_TICKS(800));
                led_set(PANEL_STATUS_LED_GPIO, false);
                vTaskDelay(pdMS_TO_TICKS(800));
                break;
        }
    }
}

static void update_channel_led(int ch) {
    switch (g_channel_states[ch]) {
        case CHANNEL_STATE_OFF:
            ESP_LOGD(TAG, "CH%d LED: off", ch + 1);
            break;
        case CHANNEL_STATE_ON:
            ESP_LOGD(TAG, "CH%d LED: on (warm)", ch + 1);
            break;
        case CHANNEL_STATE_PENDING:
            ESP_LOGD(TAG, "CH%d LED: pending blink", ch + 1);
            break;
        case CHANNEL_STATE_NO_RESPONSE:
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
    if (cluster_id != chip::app::Clusters::OnOff::Id) return ESP_OK;
    if (attribute_id != chip::app::Clusters::OnOff::Attributes::OnOff::Id) return ESP_OK;

    for (int i = 0; i < MATTER_PANEL_CHANNEL_COUNT; i++) {
        if (endpoint_id == panel_endpoint_ids[i]) {
            bool state = val->val.b;
            g_channel_states[i] = state ? CHANNEL_STATE_ON : CHANNEL_STATE_OFF;
            gpio_set_level((gpio_num_t)panel_gpios[i], state ? 1 : 0);
            update_channel_led(i);
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

static void app_event_cb(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
        case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
            ESP_LOGI(TAG, "Interface IP Address changed");
            break;
        case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
            ESP_LOGI(TAG, "Commissioning complete");
            g_panel_status = PANEL_STATUS_ONLINE;
            break;
        case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
            ESP_LOGI(TAG, "Commissioning session started");
            g_panel_status = PANEL_STATUS_PAIRING;
            break;
        case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
            ESP_LOGI(TAG, "Commissioning session stopped");
            break;
        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
            ESP_LOGI(TAG, "Commissioning window opened");
            break;
        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
            ESP_LOGI(TAG, "Commissioning window closed");
            break;
        case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
            ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
            g_panel_status = PANEL_STATUS_ERROR;
            break;
        default:
            break;
    }
}

extern "C" void app_main()
{
    esp_err_t err = ESP_OK;

    gpio_config_t io_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    for (int i = 0; i < MATTER_PANEL_CHANNEL_COUNT; i++) {
        io_conf.pin_bit_mask |= (1ULL << panel_gpios[i]);
        gpio_set_level((gpio_num_t)panel_gpios[i], 0);
        g_channel_states[i] = CHANNEL_STATE_OFF;
    }
    io_conf.pin_bit_mask |= (1ULL << PANEL_STATUS_LED_GPIO);
    gpio_config(&io_conf);
    gpio_set_level(PANEL_STATUS_LED_GPIO, 0);

    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    if (!node) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        g_panel_status = PANEL_STATUS_ERROR;
        return;
    }

    for (int i = 0; i < MATTER_PANEL_CHANNEL_COUNT; i++) {
        on_off_light::config_t light_config;
        endpoint_t *ep = on_off_light::create(node, &light_config, ENDPOINT_FLAG_NONE, NULL);
        if (!ep) {
            ESP_LOGE(TAG, "Failed to create endpoint for CH%d", i + 1);
            continue;
        }
        panel_endpoint_ids[i] = endpoint::get_id(ep);
        ESP_LOGI(TAG, "CH%d endpoint: %d", i + 1, panel_endpoint_ids[i]);
    }

    err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Matter: %d", err);
        g_panel_status = PANEL_STATUS_ERROR;
        return;
    }

    g_panel_status = PANEL_STATUS_UNPAIRED;
    ESP_LOGI(TAG, "Matter panel started (%d channels), waiting for commissioning...", MATTER_PANEL_CHANNEL_COUNT);

    xTaskCreate(status_led_task, "status_led", 2048, NULL, 1, NULL);
}
