#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/semphr.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "nvs.h"


#include "discord.h"
#include "discord/session.h"
#include "discord/message.h"
#include "discord_ota.h"

static const char* TAG = "kasia_bot";
nvs_handle_t nvs_handle;
ledc_channel_t channels[3] = {LEDC_CHANNEL_0,LEDC_CHANNEL_1,LEDC_CHANNEL_2};

#define CONFIG_LED_FREQUENCY 30000
#define WIFI_SSID_KEY "wifi_ssid_key"
#define WIFI_PASS_KEY "wifi_pass_key"
#define NVS_NAMESPACE "wifi_namespace"
#define CONFIG_TIMER_RESOLUTION LEDC_TIMER_8_BIT
#define LED_TIMER LEDC_TIMER_0

ledc_channel_t channels[3] = {LEDC_CHANNEL_0,LEDC_CHANNEL_1,LEDC_CHANNEL_2};

esp_err_t LedPinStratup(){
    gpio_num_t RGB_PINS[] = {CONFIG_RED_PIN,CONFIG_GREEN_PIN,CONFIG_BLUE_PIN};
    ledc_timer_t led_timers[3] = {LEDC_TIMER_0,LEDC_TIMER_1,LEDC_TIMER_2};

    for(uint8_t i = 0;i<3;i++){
        ledc_timer_config_t timer_config = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num  = led_timers[i],
            .duty_resolution = CONFIG_TIMER_RESOLUTION,
            .freq_hz = CONFIG_LED_FREQUENCY,
            .clk_cfg = LEDC_AUTO_CLK
        };
        ledc_channel_config_t channel_config = {
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = channels[i],
            .timer_sel      = led_timers[i],
            .intr_type      = LEDC_INTR_DISABLE,
            .gpio_num       = RGB_PINS[i],
            .duty           = 255,
            .hpoint         = 0
        };

        ESP_ERROR_CHECK(ledc_timer_config(&timer_config));
        ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
    };
    return ESP_OK;
}

/**
static discord_handle_t bot;

static void bot_event_handler(void* handler_arg, esp_event_base_t base, int32_t event_id, void* event_data) {
    discord_event_data_t* data = (discord_event_data_t*) event_data;

    switch(event_id) {
        case DISCORD_EVENT_CONNECTED: {
                discord_session_t* session = (discord_session_t*) data->ptr;
                discord_session_dump_log(ESP_LOGI, TAG, session);
            }
            break;
        
        case DISCORD_EVENT_MESSAGE_RECEIVED: {
                discord_message_t* msg = (discord_message_t*) data->ptr;
                discord_message_dump_log(ESP_LOGI, TAG, msg);
            }
            break;
        
        case DISCORD_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Bot logged out");
            break;
    }
}
*/

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "Connected to AP");
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from AP");
        esp_wifi_connect();
    }
    printf("%d",event_id);
}

static void setRGB(uint8_t RGB[3]){
    for(int i = 0; i < 3; i++){
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channels[i], 255-(RGB[i])));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channels[i]));
    }
}

esp_err_t init_wifi(){

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // READ VALUES FROM NVS

    char ssid[64]; // Adjust the size as needed
    size_t required_size;
    esp_err_t nvs_get_result = nvs_get_str(nvs_handle, WIFI_SSID_KEY, NULL, &required_size);
    if (nvs_get_result == ESP_OK) {
        if (required_size > sizeof(value)) {
            printf("Error: Value size exceeds buffer size\n");
            nvs_close(nvs_handle);
            return;
        }
        nvs_get_result = nvs_get_str(nvs_handle, WIFI_SSID_KEY, value, &required_size);
        ESP_ERROR_CHECK(nvs_get_result);
    } else if (nvs_get_result == ESP_ERR_NVS_NOT_FOUND) {
        strcpy(value, "SAMPLE"); // Variable not found, initialize with a default value
    } else {
        ESP_ERROR_CHECK(nvs_get_result);
    }

    char pass[64]; // Adjust the size as needed
    size_t required_size;
    esp_err_t nvs_get_result = nvs_get_str(nvs_handle, WIFI_PASS_KEY, NULL, &required_size);
    if (nvs_get_result == ESP_OK) {
        if (required_size > sizeof(value)) {
            printf("Error: Value size exceeds buffer size\n");
            nvs_close(nvs_handle);
            return;
        }
        nvs_get_result = nvs_get_str(nvs_handle, WIFI_PASS_KEY, value, &required_size);
        ESP_ERROR_CHECK(nvs_get_result);
    } else if (nvs_get_result == ESP_ERR_NVS_NOT_FOUND) {
        strcpy(value, "SAMPLE"); // Variable not found, initialize with a default value
    } else {
        ESP_ERROR_CHECK(nvs_get_result);
    }

    //WIFI CONFIG

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ssid,
            .password = pass
        },
    };

    ESP_LOGI(TAG, "Connecting to WiFi...");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());


    return ESP_OK;
}

void app_main(void) {

    ESP_ERROR_CHECK(LedPinStratup());
    ESP_LOGD(TAG,"leds initiated.");
    
    //discord_ota_keep(true);

    //NVS INIT
    esp_err_t nvs_init_result = nvs_flash_init();
    if (nvs_init_result == ESP_ERR_NVS_NO_FREE_PAGES || nvs_init_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_init_result = nvs_flash_init();
    }

    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(init_wifi());

    /*
    discord_config_t cfg = {
        .intents = DISCORD_INTENT_GUILD_MESSAGES | DISCORD_INTENT_MESSAGE_CONTENT
    };

    bot = discord_create(&cfg);
    ESP_ERROR_CHECK(discord_ota_init(bot, NULL));
    ESP_ERROR_CHECK(discord_register_events(bot, DISCORD_EVENT_ANY, bot_event_handler, NULL));
    ESP_ERROR_CHECK(discord_login(bot));
    */
   
    setRGB((uint8_t []){128,128,128});
}