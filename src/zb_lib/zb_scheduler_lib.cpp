#include "zb_scheduler_lib.h"
#include "../ScheduleManager.h"
#include "../HardwareControl.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" {
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_basic.h"
#include "zcl/esp_zigbee_zcl_on_off.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "ha/esp_zigbee_ha_standard.h"
}

// main.cpp - un comando entrante de encendido/apagado se trata igual que un toque manual.
extern bool estadoBombaA;
extern bool estadoBombaB;
extern bool bloqueoSeguridad;
void markPumpManualOverride(uint8_t pumpIndex); // declarado también en ScheduleManager.h

// --- Identificadores del clúster de configuración custom ---
// 0xFC00 está en el rango "Manufacturer Specific" reservado por la especificación ZCL
// (0xFC00-0xFFFF) para clústeres no estándar - esto es un hecho del estándar Zigbee, no una
// suposición sobre esta librería.
static const uint16_t CLUSTER_ID_RIEGO_CONFIG = 0xFC00;
enum : uint16_t {
    ATTR_PUMP_A_NAME  = 0x0000, // Character String
    ATTR_PUMP_B_NAME  = 0x0001, // Character String
    ATTR_PUMP_A_SCHED = 0x0002, // Long Character String (el JSON puede superar 254 bytes)
    ATTR_PUMP_B_SCHED = 0x0003, // Long Character String
    ATTR_TIMEZONE     = 0x0004, // Character String
    ATTR_EPOCH_TIME   = 0x0005, // Unsigned 32-bit Integer
};

// Buffers estáticos con el formato de cadena ZCL (length-prefixed) que exige el SDK para
// atributos de tipo string - 1 byte de longitud para Character String (máx 254 datos),
// 2 bytes de longitud (little-endian) para Long Character String (máx 65534 datos).
static uint8_t bufPumpAName[1 + MAX_PUMP_NAME_LEN];
static uint8_t bufPumpBName[1 + MAX_PUMP_NAME_LEN];
static uint8_t bufTimezone[1 + 40];
static uint8_t bufPumpASched[2 + 512];
static uint8_t bufPumpBSched[2 + 512];

static void packCharString(uint8_t* buf, size_t bufCap, const char* text) {
    size_t len = strlen(text);
    if (len > bufCap - 1) len = bufCap - 1;
    buf[0] = (uint8_t)len;
    memcpy(buf + 1, text, len);
}

static void packLongCharString(uint8_t* buf, size_t bufCap, const char* text) {
    size_t len = strlen(text);
    if (len > bufCap - 2) len = bufCap - 2;
    buf[0] = (uint8_t)(len & 0xFF);
    buf[1] = (uint8_t)((len >> 8) & 0xFF);
    memcpy(buf + 2, text, len);
}

// --- Endpoints On/Off para las bombas ---
static void onPumpCommand(uint8_t pumpIndex, bool state) {
    if (bloqueoSeguridad && state) {
        Serial.printf("Comando Zigbee ignorado: Bomba %c bloqueada por seguridad (tanque vacio)\n", 'A' + pumpIndex);
        zbReportPumpState(pumpIndex, false); // refleja el estado real, no acepta el comando
        return;
    }
    if (pumpIndex == 0) { estadoBombaA = state; setPumpA(state); }
    else { estadoBombaB = state; setPumpB(state); }
    markPumpManualOverride(pumpIndex);
}

// --- Manejador central de acciones ZCL (única fuente de despacho - ver nota de diseño en el .h) ---
static esp_err_t zbActionHandler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
    switch (callback_id) {
        case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID: {
            const esp_zb_zcl_set_attr_value_message_t *msg = (const esp_zb_zcl_set_attr_value_message_t *)message;
            if (!msg) return ESP_OK;

            if (msg->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
                bool state = msg->attribute.data.value ? *(bool *)msg->attribute.data.value : false;
                if (msg->info.dst_endpoint == ZB_EP_PUMP_A) onPumpCommand(0, state);
                else if (msg->info.dst_endpoint == ZB_EP_PUMP_B) onPumpCommand(1, state);
            } else if (msg->info.cluster == CLUSTER_ID_RIEGO_CONFIG && msg->info.dst_endpoint == ZB_EP_CONFIG) {
                // Atributos de texto llegan con el formato length-prefixed ya descrito -
                // los datos útiles empiezan después del prefijo de longitud.
                const uint8_t* raw = (const uint8_t*)msg->attribute.data.value;
                switch (msg->attribute.id) {
                    case ATTR_PUMP_A_NAME: {
                        char tmp[MAX_PUMP_NAME_LEN] = {0};
                        memcpy(tmp, raw + 1, min((int)raw[0], MAX_PUMP_NAME_LEN - 1));
                        setPumpName(0, tmp);
                        break;
                    }
                    case ATTR_PUMP_B_NAME: {
                        char tmp[MAX_PUMP_NAME_LEN] = {0};
                        memcpy(tmp, raw + 1, min((int)raw[0], MAX_PUMP_NAME_LEN - 1));
                        setPumpName(1, tmp);
                        break;
                    }
                    case ATTR_PUMP_A_SCHED: {
                        uint16_t len = raw[0] | (raw[1] << 8);
                        char tmp[513] = {0};
                        memcpy(tmp, raw + 2, min((int)len, 512));
                        if (!setPumpScheduleJson(0, tmp)) Serial.println("Horario Bomba A (Zigbee): JSON invalido");
                        break;
                    }
                    case ATTR_PUMP_B_SCHED: {
                        uint16_t len = raw[0] | (raw[1] << 8);
                        char tmp[513] = {0};
                        memcpy(tmp, raw + 2, min((int)len, 512));
                        if (!setPumpScheduleJson(1, tmp)) Serial.println("Horario Bomba B (Zigbee): JSON invalido");
                        break;
                    }
                    case ATTR_TIMEZONE: {
                        char tmp[40] = {0};
                        memcpy(tmp, raw + 1, min((int)raw[0], 39));
                        setTimezone(tmp);
                        break;
                    }
                    case ATTR_EPOCH_TIME: {
                        uint32_t epoch = *(uint32_t*)msg->attribute.data.value;
                        setEpochTime(epoch);
                        break;
                    }
                    default:
                        break;
                }
            }
            break;
        }
        default:
            break;
    }
    return ESP_OK;
}

// --- Construcción de clústeres/endpoints ---

static esp_zb_cluster_list_t* buildPumpClusterList(const char* label) {
    esp_zb_basic_cluster_cfg_t basicCfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = 0x03, // Battery
    };
    esp_zb_attribute_list_t *basicCluster = esp_zb_basic_cluster_create(&basicCfg);
    esp_zb_basic_cluster_add_attr(basicCluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void*)"\x03""FSD");
    esp_zb_basic_cluster_add_attr(basicCluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void*)label);

    esp_zb_identify_cluster_cfg_t identifyCfg = { .identify_time = 0 };
    esp_zb_attribute_list_t *identifyCluster = esp_zb_identify_cluster_create(&identifyCfg);

    esp_zb_on_off_cluster_cfg_t onOffCfg = { .on_off = false };
    esp_zb_attribute_list_t *onOffCluster = esp_zb_on_off_cluster_create(&onOffCfg);

    esp_zb_cluster_list_t *clusters = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(clusters, basicCluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(clusters, identifyCluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(clusters, onOffCluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    return clusters;
}

static esp_zb_cluster_list_t* buildConfigClusterList() {
    packCharString(bufPumpAName, sizeof(bufPumpAName), getPumpName(0));
    packCharString(bufPumpBName, sizeof(bufPumpBName), getPumpName(1));
    packCharString(bufTimezone, sizeof(bufTimezone), getTimezone().c_str());
    packLongCharString(bufPumpASched, sizeof(bufPumpASched), getPumpScheduleJson(0).c_str());
    packLongCharString(bufPumpBSched, sizeof(bufPumpBSched), getPumpScheduleJson(1).c_str());

    esp_zb_attribute_list_t *cfgCluster = esp_zb_zcl_attr_list_create(CLUSTER_ID_RIEGO_CONFIG);
    static uint32_t epochPlaceholder = 0;

    esp_zb_custom_cluster_add_custom_attr(cfgCluster, ATTR_PUMP_A_NAME, ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, bufPumpAName);
    esp_zb_custom_cluster_add_custom_attr(cfgCluster, ATTR_PUMP_B_NAME, ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, bufPumpBName);
    esp_zb_custom_cluster_add_custom_attr(cfgCluster, ATTR_PUMP_A_SCHED, ESP_ZB_ZCL_ATTR_TYPE_LONG_CHAR_STRING,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, bufPumpASched);
    esp_zb_custom_cluster_add_custom_attr(cfgCluster, ATTR_PUMP_B_SCHED, ESP_ZB_ZCL_ATTR_TYPE_LONG_CHAR_STRING,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, bufPumpBSched);
    esp_zb_custom_cluster_add_custom_attr(cfgCluster, ATTR_TIMEZONE, ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, bufTimezone);
    esp_zb_custom_cluster_add_custom_attr(cfgCluster, ATTR_EPOCH_TIME, ESP_ZB_ZCL_ATTR_TYPE_U32,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, &epochPlaceholder);

    esp_zb_cluster_list_t *clusters = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_custom_cluster(clusters, cfgCluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    return clusters;
}

bool zbSchedulerInit() {
    esp_zb_platform_config_t platformCfg = {};
    platformCfg.radio_config.radio_mode = ZB_RADIO_MODE_NATIVE;
    platformCfg.host_config.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE;
    esp_zb_platform_config(&platformCfg);

    esp_zb_cfg_t zbCfg = {};
    zbCfg.esp_zb_role = ESP_ZB_DEVICE_TYPE_ED;
    zbCfg.install_code_policy = false;
    zbCfg.nwk_cfg.zed_cfg.ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN;
    zbCfg.nwk_cfg.zed_cfg.keep_alive = 3000;
    esp_zb_init(&zbCfg);

    esp_zb_ep_list_t *epList = esp_zb_ep_list_create();

    esp_zb_endpoint_config_t epPumpA = {};
    epPumpA.endpoint = ZB_EP_PUMP_A;
    epPumpA.app_profile_id = ESP_ZB_AF_HA_PROFILE_ID;
    epPumpA.app_device_id = ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID;
    epPumpA.app_device_version = 0;
    esp_zb_ep_list_add_ep(epList, buildPumpClusterList("Bomba-Riego-Goteo-Z2M-A"), epPumpA);

    esp_zb_endpoint_config_t epPumpB = {};
    epPumpB.endpoint = ZB_EP_PUMP_B;
    epPumpB.app_profile_id = ESP_ZB_AF_HA_PROFILE_ID;
    epPumpB.app_device_id = ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID;
    epPumpB.app_device_version = 0;
    esp_zb_ep_list_add_ep(epList, buildPumpClusterList("Bomba-Riego-Goteo-Z2M-B"), epPumpB);

    esp_zb_endpoint_config_t epCfg = {};
    epCfg.endpoint = ZB_EP_CONFIG;
    epCfg.app_profile_id = ESP_ZB_AF_HA_PROFILE_ID;
    epCfg.app_device_id = ESP_ZB_HA_CONFIGURATION_TOOL_DEVICE_ID;
    epCfg.app_device_version = 0;
    esp_zb_ep_list_add_ep(epList, buildConfigClusterList(), epCfg);

    esp_zb_device_register(epList);
    esp_zb_core_action_handler_register(zbActionHandler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);

    // esp_zb_start() arranca el stack pero el procesamiento continuo de la pila Zigbee
    // (radio, temporizadores, cola ZCL) debe correr en su propia tarea FreeRTOS dedicada -
    // NO se puede llamar cooperativamente desde el loop() de Arduino (que además entra en
    // light-sleep entre iteraciones, ver main.cpp), o se pierden eventos/timing de radio.
    // Esto coincide con lo que ya advertía el comentario original de este proyecto sobre
    // Zigbee.h: "corre en su propia tarea de FreeRTOS". zbSchedulerLoop() por eso queda
    // como no-op - se mantiene solo por estabilidad de la API pública.
    esp_zb_start(false);
    xTaskCreate(
        [](void*) { esp_zb_stack_main_loop(); /* bloquea para siempre procesando la pila */ },
        "zb_main", 4096, nullptr, 5, nullptr);

    return true;
}

void zbSchedulerLoop() {
    // No-op deliberado - ver comentario en zbSchedulerInit(). El procesamiento real corre
    // en la tarea "zb_main" creada allí. Se mantiene esta función para no romper la firma
    // pública ni el flujo de main.cpp/ZigbeeHandler.cpp.
}

void zbReportPumpState(uint8_t pumpIndex, bool state) {
    uint8_t endpoint = (pumpIndex == 0) ? ZB_EP_PUMP_A : ZB_EP_PUMP_B;
    esp_zb_zcl_set_attribute_val(endpoint, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &state, false);
}

void zbReportConfigAll() {
    packCharString(bufPumpAName, sizeof(bufPumpAName), getPumpName(0));
    packCharString(bufPumpBName, sizeof(bufPumpBName), getPumpName(1));
    packCharString(bufTimezone, sizeof(bufTimezone), getTimezone().c_str());
    packLongCharString(bufPumpASched, sizeof(bufPumpASched), getPumpScheduleJson(0).c_str());
    packLongCharString(bufPumpBSched, sizeof(bufPumpBSched), getPumpScheduleJson(1).c_str());

    esp_zb_zcl_set_attribute_val(ZB_EP_CONFIG, CLUSTER_ID_RIEGO_CONFIG, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_PUMP_A_NAME, bufPumpAName, false);
    esp_zb_zcl_set_attribute_val(ZB_EP_CONFIG, CLUSTER_ID_RIEGO_CONFIG, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_PUMP_B_NAME, bufPumpBName, false);
    esp_zb_zcl_set_attribute_val(ZB_EP_CONFIG, CLUSTER_ID_RIEGO_CONFIG, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_PUMP_A_SCHED, bufPumpASched, false);
    esp_zb_zcl_set_attribute_val(ZB_EP_CONFIG, CLUSTER_ID_RIEGO_CONFIG, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_PUMP_B_SCHED, bufPumpBSched, false);
    esp_zb_zcl_set_attribute_val(ZB_EP_CONFIG, CLUSTER_ID_RIEGO_CONFIG, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ATTR_TIMEZONE, bufTimezone, false);
}
