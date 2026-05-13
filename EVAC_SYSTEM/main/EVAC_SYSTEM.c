#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <esp_rom_sys.h>

#include <driver/gpio.h>

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>

#include <mqtt_client.h>

#include <i2cdev.h>
#include <si7021.h>

/* ================= PIN ================= */
#define I2C_SDA_GPIO 21
#define I2C_SCL_GPIO 22

#define TRIG 18
#define ECHO 19

/* ================= MQTT ================= */
#define TOPIC_TEMP       "sed/sensor/temp"
#define TOPIC_HUMIDITY   "sed/sensor/humidity"
#define TOPIC_DISTANCE   "sed/sensor/distance"
#define TOPIC_SI7021_STATUS      "sed/sensor/si7021/status"
#define TOPIC_DISTANCE_STATUS      "sed/sensor/distance/status"

static const char *TAG = "EVAC";

/* ================= DEV ================= */
static i2c_dev_t si_dev;

/* ================= WIFI ================= */
// Configura y conecta a la red Wi-Fi
void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

/* ================= MQTT ================= */
static void mqtt_handler(void *args, esp_event_base_t base, int32_t id, void *data)
{
    if (id == MQTT_EVENT_CONNECTED)
        ESP_LOGI(TAG, "MQTT connected");
}

// Configura e inicia el cliente MQTT
static esp_mqtt_client_handle_t mqtt_start(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
    };
    // Se inicializa el cliente MQTT
    esp_mqtt_client_handle_t c = esp_mqtt_client_init(&cfg);
    // Se registra un manejador de eventos para cualquier evento MQTT
    esp_mqtt_client_register_event(c, ESP_EVENT_ANY_ID, mqtt_handler, NULL);
    esp_mqtt_client_start(c);
    return c;
}

/* ================= DISTANCE ================= */
// Realiza una medición de distancia utilizando sensor HC-SR04
float read_distance()
{
    //Mantener el pin TRIG en estado bajo para asegurar estabilidad.
    gpio_set_level(TRIG, 0);
    esp_rom_delay_us(2);
    // Enviar un pulso de disparo (TRIG) de 10 microsegundos, poniendo el pin en alto
    gpio_set_level(TRIG, 1);
    esp_rom_delay_us(10);
    // Luego, se pone el pin de disparo (TRIG) en bajo nuevamente
    gpio_set_level(TRIG, 0);

    // Espera a que el pin de eco (ECHO) se active (nivel alto)
    while (!gpio_get_level(ECHO));
    // Guarda el tiempo de inicio del pulso ECHO
    int64_t start = esp_timer_get_time();

    // Espera a que el pin de eco (ECHO) se desactive (nivel bajo)
    while (gpio_get_level(ECHO));
    // Guarda el tiempo de fin del pulso ECHO
    int64_t end = esp_timer_get_time();

    // Calcula la distancia en centímetros
    // distancia = (tiempo de ida y vuelta * velocidad del sonido) / 2
    return (end - start) * 0.0343 / 2;
}

/* ================= TASK ================= */
static void task(void *arg)
{
    esp_mqtt_client_handle_t client = arg;

    char msg[32];
    float temp, dist, hum;

    while (1)
    {


        /* SI7021 */
        if (si7021_measure_temperature(&si_dev, &temp) == ESP_OK &&
            si7021_measure_humidity(&si_dev, &hum) == ESP_OK)
        {
            sprintf(msg, "%.2f", temp);
            esp_mqtt_client_publish(client, TOPIC_TEMP, msg, 0, 0, 0);
            sprintf(msg, "%.2f", hum);
            esp_mqtt_client_publish(client, TOPIC_HUMIDITY, msg, 0, 0, 0);
            esp_mqtt_client_publish(client, TOPIC_SI7021_STATUS, "online", 0, 0, 0);
            ESP_LOGI(TAG, "SI7021 ONLINE");
        }
        else
        {
            esp_mqtt_client_publish(client, TOPIC_SI7021_STATUS, "offline", 0, 0, 0);
            ESP_LOGW(TAG, "SI7021 OFFLINE");
        }

        /* distance */
        dist = read_distance();
        if(dist < 0 || dist > 700)
        {
            esp_mqtt_client_publish(client, TOPIC_DISTANCE_STATUS, "offline", 0, 0, 0);
            ESP_LOGI(TAG, "HC_SR04 OFFLINE");
        }
        else{
            sprintf(msg, "%.2f", dist);
            esp_mqtt_client_publish(client, TOPIC_DISTANCE, msg, 0, 0, 0);
            esp_mqtt_client_publish(client, TOPIC_DISTANCE_STATUS, "online", 0, 0, 0);
            ESP_LOGI(TAG, "HC_SR04 ONLINE");
        }
        // En terminal aparece algo como: T=25.3 H=60.0 D=120.5
        ESP_LOGI(TAG, "T=%.1f H=%.1f D=%.1f",
                 temp, hum, dist);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* ================= MAIN ================= */
void app_main()
{
    nvs_flash_init();
    i2cdev_init();
    // Inicializa el descriptor del sensor de temperatura SI7021
    si7021_init_desc(&si_dev, 0, I2C_SDA_GPIO, I2C_SCL_GPIO);


    // Configura los pines para el sensor de distancia
    gpio_set_direction(TRIG, GPIO_MODE_OUTPUT);
    gpio_set_direction(ECHO, GPIO_MODE_INPUT);

    // Conecta a la red Wi-Fi
    wifi_init_sta();

    // Espera para asegurar estabilidad de la conexión Wi-Fi
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Inicia el cliente MQTT y obtiene su manejador
    esp_mqtt_client_handle_t client = mqtt_start();

    // Crea una tarea de FreeRTOS para leer los sensores y publicar los datos en MQTT
    xTaskCreate(task, "task", 4096, client, 5, NULL);
}
