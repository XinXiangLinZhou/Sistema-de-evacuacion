#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <mqtt_client.h>
#include <esp_log.h>
#include "driver/gpio.h"
#include <esp_system.h>
#include "esp_ota_ops.h"
#include <inttypes.h>
#include <esp_mac.h>
#include <stdlib.h>
#include <string.h>
#include "mender-client.h"
#include "mender-flash.h"
#include "led_strip.h"

#define TOPIC_TEMP          "sed/sensor/temp"
#define TOPIC_HUMIDITY      "sed/sensor/humidity"
#define TOPIC_ALERT         "sed/G01/status"

#define RGB_GPIO 2

mender_client_config_t mender_config;
mender_client_callbacks_t mender_callbacks;
mender_keystore_t mender_identity[2];
char mender_mac_address[18];

static const char *TAG = "FINAL_C3";
static led_strip_handle_t led_strip;

#define VERSION "1.0.1"
#define FIRE_HUMIDITY_WARN 35
#define FIRE_HUMIDITY_CRIT 50
#define FIRE_TEMP_WARN 26
#define FIRE_TEMP_CRIT 28

void rgb_led_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = RGB_GPIO,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    // off luz
    led_strip_clear(led_strip);
}

void led_alert(void) {
    led_strip_set_pixel(led_strip, 0, 255, 0, 0);
    led_strip_refresh(led_strip);
}

void led_normal(void) {
    led_strip_set_pixel(led_strip, 0, 0, 255, 0);
    led_strip_refresh(led_strip);
}

void led_warning(void) {
    // yellow
    led_strip_set_pixel(led_strip, 0, 255, 255, 0);
    led_strip_refresh(led_strip);
}



static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

// Convierte el ESP32 en cliente WiFi (modo Station) y lo conecta al router
void wifi_init_sta(void) {
    // Inicializa el sistema de red y el bucle de eventos
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default()); 
    // Muchas funciones del ESP32 funcionan por eventos, así que necesita este loop para gestionarlos

    esp_netif_create_default_wifi_sta(); 
    // Crea la interfaz de red en modo Station (cliente)

    // Configuración por defecto del driver WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); 
    // Genera una configuración básica predeterminada

    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); 
    // Inicia el controlador WiFi con esa configuración

    // Configura el nombre de la red (SSID) y la contraseña desde menuconfig
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
        },
    };

    // Establece el modo Station: ESP32 se conecta al router como cliente
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Aplica la configuración WiFi (SSID + contraseña)
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        
    // Inicia el WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
        
    // Muestra en consola a qué red intenta conectarse
    ESP_LOGI("WIFI", "Conectando a %s...", CONFIG_ESP_WIFI_SSID);

    // Intenta conectarse al router
    esp_wifi_connect();
}

// ---------------- MENDER CALLBACKS ----------------

static mender_err_t mender_network_connect_cb(void) {
    return MENDER_OK;
}

static mender_err_t mender_network_release_cb(void) {
    return MENDER_OK;
}

static mender_err_t mender_auth_failure_cb(void) {
    return MENDER_OK;
}

static mender_err_t mender_deployment_status_cb(mender_deployment_status_t status, char *desc) {
    ESP_LOGI("MENDER", "Deployment: %s", desc ? desc : "Unknown");
    return MENDER_OK;
}

static mender_err_t mender_auth_success_cb(void) {
    ESP_LOGI("MENDER", "Auth OK");
    return mender_flash_confirm_image();
}

static mender_err_t mender_restart_cb(void) {
    ESP_LOGI("MENDER", "Restarting OTA...");
    esp_restart();
    return MENDER_OK;
}

void mender_init(void) {
    uint8_t mac[6];

    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    sprintf(mender_mac_address,
            "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2],
            mac[3], mac[4], mac[5]);

    mender_identity[0].name = "mac";
    mender_identity[0].value = mender_mac_address;
    mender_identity[1].name = NULL;
    mender_identity[1].value = NULL;

    mender_config.identity = mender_identity;
    mender_config.artifact_name = VERSION;
    mender_config.device_type = "esp32";
    mender_config.host = CONFIG_MENDER_SERVER_HOST;
    mender_config.tenant_token = NULL;
    mender_config.authentication_poll_interval = 60;
    mender_config.update_poll_interval = 60;
    mender_config.recommissioning = false;

    mender_callbacks.network_connect = mender_network_connect_cb;
    mender_callbacks.network_release = mender_network_release_cb;
    mender_callbacks.authentication_success = mender_auth_success_cb;
    mender_callbacks.authentication_failure = mender_auth_failure_cb;
    mender_callbacks.deployment_status = mender_deployment_status_cb;
    mender_callbacks.restart = mender_restart_cb;

    ESP_LOGI("MENDER", "Starting Mender: %s", mender_mac_address);

    if (mender_client_init(&mender_config, &mender_callbacks) == MENDER_OK) {
        mender_client_activate();
    } else {
        ESP_LOGE("MENDER", "Mender init failed");
    }
}

// Esta función se encarga de:
// Indicar a qué broker MQTT se conecta el ESP32
// Configurar el Last Will (LWT)
// Configurar el keepalive
// Registrar el manejador de eventos MQTT
// Iniciar el cliente MQTT
static esp_mqtt_client_handle_t mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {

        // URL del broker configurada desde menuconfig
        .broker.address.uri = CONFIG_BROKER_URL, 
        // Ejemplo: mqtt://IP_del_router_ASUS

        // Configuración del Last Will and Testament (LWT)
        .session.last_will = {

            // Si el dispositivo se desconecta de forma inesperada,
            // el broker enviará este mensaje automáticamente a este topic
            .topic = TOPIC_ALERT, 
            // Ejemplo: "sed/G02/status"

            // Mensaje que se enviará si el ESP32 falla o pierde conexión
            .msg = "Offline",

            // Longitud del mensaje
            .msg_len = strlen("Offline"),

            // QoS 1 = el mensaje debe llegar al menos una vez
            .qos = 1,

            // Retain = el broker guarda este último estado
            // Así, cualquier nuevo suscriptor sabrá si el dispositivo está offline
            .retain = 1,
        },

        // Keepalive:
        // Cada 10 segundos el cliente y el broker verifican que siguen conectados
        .session.keepalive = 10,
    };

    // Crea el cliente MQTT usando toda la configuración anterior
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);

    // Registra la función que manejará eventos MQTT
    // (conexión, mensajes recibidos, desconexión, etc.)
    esp_mqtt_client_register_event(
        client, 
        ESP_EVENT_ANY_ID, 
        mqtt_event_handler, 
        NULL
    );

    // Inicia la conexión MQTT
    esp_mqtt_client_start(client);

    // Devuelve el cliente para poder usarlo después
    return client;
}

// Esta función se ejecuta automáticamente cada vez que ocurre un evento MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    
    // Guarda la información del evento recibido
    esp_mqtt_event_handle_t event = event_data;

    // Obtiene el cliente MQTT para poder suscribirse o publicar
    esp_mqtt_client_handle_t client = event->client;

    // Según el tipo de evento, ejecuta diferentes acciones
    switch ((esp_mqtt_event_id_t)event_id) {

    // Cuando el ESP32 se conecta correctamente al broker
    case MQTT_EVENT_CONNECTED:

        ESP_LOGI(TAG, "Conectado al Broker");

        // Se suscribe solo a los topics de temperatura y humedad
        esp_mqtt_client_subscribe(client, TOPIC_TEMP, 1);
        esp_mqtt_client_subscribe(client, TOPIC_HUMIDITY, 1);

        break;

    // Cuando llega un mensaje nuevo desde algún topic suscrito
    case MQTT_EVENT_DATA:

        // Copia el nombre del topic recibido
        char topic[64];
        memcpy(topic, event->topic, event->topic_len);
        topic[event->topic_len] = '\0';

        // Copia el mensaje recibido
        char msg[16];

        // Evita desbordamiento del buffer
        int len = event->data_len < sizeof(msg)-1 ? event->data_len : sizeof(msg)-1;

        memcpy(msg, event->data, len);
        msg[len] = '\0';

        // Convierte el mensaje de texto a número entero
        int value = atoi(msg);

        // Muestra en consola el topic y el valor recibido
        ESP_LOGI(TAG, "TOPIC: %s | DATA: %d", topic, value);

        // Si el mensaje viene del sensor de temperatura
        if (strcmp(topic, TOPIC_TEMP) == 0) {

            ESP_LOGI(TAG, "Temperatura: %d", value);

            // Nivel crítico → rojo
            if (value >= FIRE_TEMP_CRIT) {
                ESP_LOGI(TAG, "TEMP CRITICA!");
                led_alert();
            } 
            // Nivel de advertencia → amarillo
            else if (value >= FIRE_TEMP_WARN) {
                ESP_LOGI(TAG, "TEMP WARNING!");
                led_warning();
            } 
            // Nivel normal → verde
            else {
                ESP_LOGI(TAG, "TEMP NORMAL");
                led_normal();
            }

        } 
        // Si el mensaje viene del sensor de humedad
        else if (strcmp(topic, TOPIC_HUMIDITY) == 0) {

            ESP_LOGI(TAG, "Humedad: %d", value);

            // Nivel crítico → rojo
            if (value >= FIRE_HUMIDITY_CRIT) {
                ESP_LOGI(TAG, "HUMEDAD CRITICA!");
                led_alert();
            } 
            // Nivel de advertencia → amarillo
            else if (value >= FIRE_HUMIDITY_WARN) {
                ESP_LOGI(TAG, "HUMEDAD WARNING!");
                led_warning();
            } 
            // Nivel normal → verde
            else {
                ESP_LOGI(TAG, "HUMEDAD NORMAL");
                led_normal();
            }
        }

        break;

    // Si ocurre un error en MQTT
    case MQTT_EVENT_ERROR:

        ESP_LOGE(TAG, "Error en el stack MQTT");

        break;

    // Otros eventos no se usan
    default:
        break;
    }
}

void app_main(void) {

    // Inicializa el LED RGB
    rgb_led_init();

    // Enciende el LED en modo normal (verde al iniciar)
    led_normal();

    // Obtiene la partición actual desde la que está ejecutándose el firmware
    const esp_partition_t *running = esp_ota_get_running_partition();

    // Muestra información básica del sistema al arrancar
    printf("\n--- Inicio del sistema ---\n");

    // Versión actual del firmware
    printf("Versión del firmware: %s\n", VERSION);

    // Nombre de la partición activa (por ejemplo: ota_0 o ota_1)
    printf("Partición actual: %s\n", running->label);

    // Dirección de memoria donde está esa partición
    printf("Dirección de partición: 0x%08" PRIx32 "\n", running->address);

    // Inicializa la memoria NVS (Non-Volatile Storage)
    // Se usa para guardar configuraciones como WiFi, OTA, etc.
    esp_err_t ret = nvs_flash_init();

    // Si hay error por falta de espacio o versión antigua,
    // borra la NVS y la reinicia
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());

        ret = nvs_flash_init();
    }

    // Verifica que NVS se inicializó correctamente
    ESP_ERROR_CHECK(ret);

    // Conecta el ESP32 al WiFi como cliente
    wifi_init_sta();

    // Espera 10 segundos para asegurar conexión estable
    // antes de iniciar servicios online
    vTaskDelay(pdMS_TO_TICKS(10000));

    // Inicializa Mender para OTA updates
    // Permite recibir nuevas versiones del firmware remotamente
    mender_init();

    // Inicia MQTT para comunicación con sensores y alertas
    mqtt_app_start();

    // Mensaje final de confirmación
    ESP_LOGI(TAG, "WiFi, Mender y MQTT iniciados correctamente");
}

