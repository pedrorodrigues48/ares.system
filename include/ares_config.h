#ifndef ARES_CONFIG_H
#define ARES_CONFIG_H

#include "ares_protocol.h"

// ==============================================================================
// CONFIGURAÇÕES GLOBAIS DO NÓ ARES (100% DINÂMICAS PARA N ESP32s)
// O identificador do nó é derivado automaticamente do MAC Address de hardware
// (ex: ESP32_4A1C8E). Se NODE_ID for definido em tempo de compilação, ele prevalece.
// ==============================================================================

extern char g_ares_node_id[32];
extern char g_ares_mac_str[18];
extern char g_ares_node_sig[17];
extern char g_ares_ssid[32];
extern char g_ares_lat[16];
extern char g_ares_lon[16];
extern char g_ares_altura[16];
extern uint8_t g_ares_role;
extern float g_ares_battery;

// Vizinhos diretos: nós que este nó ouviu sem intermediário (hop_count 1).
// Vão no beacon de telemetria (campo NB) para o site desenhar a rede real.
struct AresNeighbor {
    char id[20];
    int rssi;
    unsigned long lastSeen;
};
#define ARES_MAX_NEIGHBORS        8
#define ARES_NEIGHBOR_TIMEOUT_MS  20000 // some da lista se ficar 20 s sem ser ouvido
extern AresNeighbor g_ares_neighbors[ARES_MAX_NEIGHBORS];

// Identificador único real derivado do MAC de fábrica no eFuse (ex: ESP32_A00370)
#define ARES_NODE_ID (g_ares_node_id[0] != '\0' ? g_ares_node_id : "ESP32_DYNAMIC")

#define ARES_NODE_LAT (g_ares_lat[0] != '\0' ? g_ares_lat : NODE_LAT)
#define ARES_NODE_LON (g_ares_lon[0] != '\0' ? g_ares_lon : NODE_LON)
#define ARES_NODE_ALTURA (g_ares_altura[0] != '\0' ? g_ares_altura : NODE_ALTURA)

#ifndef NODE_ROLE
    #define NODE_ROLE ARES_ROLE_RELAY
#endif

// Chave mestra para derivação de assinatura criptográfica exclusiva por nó (HMAC-SHA256)
#define ARES_MASTER_SECRET "ARES-UFSM-MESH-HMAC-2026"

// Funções globais de criptografia de nó e troca dinâmica de papel em tempo de execução
void computeNodeHmacHex(const char* nodeId, const char* payload, char* outHex17);
void switchNodeRole(uint8_t newRole, bool persistToNvs);

#ifndef NODE_LAT
    #define NODE_LAT "INDEFINIDO"
#endif

#ifndef NODE_LON
    #define NODE_LON "INDEFINIDO"
#endif

#ifndef NODE_ALTURA
    #define NODE_ALTURA "INDEFINIDO"
#endif

// ==============================================================================
// CONFIGURAÇÕES DE GEOLOCALIZAÇÃO DINÂMICA VIA WI-FI (WPS)
// ==============================================================================
#ifndef GOOGLE_GEOLOCATION_API_KEY
    #define GOOGLE_GEOLOCATION_API_KEY "" // Insira sua chave ou passe via build_flags
#endif

#ifndef UNWIRED_API_KEY
    #define UNWIRED_API_KEY ""
#endif

#ifndef PROVISIONING_WIFI_SSID
    #define PROVISIONING_WIFI_SSID ""
#endif

#ifndef PROVISIONING_WIFI_PASS
    #define PROVISIONING_WIFI_PASS ""
#endif

// ==============================================================================
// CONFIGURAÇÕES DE RÁDIO ESP-NOW E WI-FI
// ==============================================================================
// Canal da malha: PRECISA ser o mesmo canal do hotspot ARES_BASE_WIFI, porque o Gateway
// fica no canal do Wi-Fi. O Gateway avisa no serial se os canais forem diferentes.
// (20/11 o hotspot estava no 11; 27/11 estava no 6.)
#define ARES_ESP_NOW_CHANNEL 6
// [LONGO ALCANCE] Potência máxima + modo Long Range (LR) do ESP-NOW.
// TODAS as placas precisam ter o mesmo valor de ARES_RADIO_LONG_RANGE, senão uma não ouve a outra.
// Para voltar ao comportamento antigo: ARES_RADIO_LONG_RANGE 0.
#define ARES_RADIO_LONG_RANGE 1
// Potência de transmissão: usamos o máximo do chip (20 dBm, via esp_wifi_set_max_tx_power(80)). Se alguma placa reiniciar sozinha
// ao ligar o rádio, o problema é alimentação (cabo/porta USB). Plano B: WIFI_POWER_8_5dBm.
#define ARES_RADIO_TX_POWER WIFI_POWER_19_5dBm
// Velocidade dos pacotes ESP-NOW no modo LR: WIFI_PHY_RATE_LORA_250K (alcance máximo)
// ou WIFI_PHY_RATE_LORA_500K (um pouco menos de alcance, metade do tempo no ar).
#define ARES_RADIO_LR_RATE WIFI_PHY_RATE_LORA_250K
#define FORCE_SOFTWARE_DEBUG true
#define BROADCAST_MAC {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

// ==============================================================================
// CONFIGURAÇÕES DO PORTAL CATIVO (SSID Dinâmico por ESP32)
// ==============================================================================
#define CAPTIVE_PORTAL_IP         IPAddress(192, 168, 4, 1)
#define CAPTIVE_PORTAL_NETMASK    IPAddress(255, 255, 255, 0)
#define DNS_PORT                  53

// ==============================================================================
// CONFIGURAÇÕES DE GATEWAY (Conexão WAN com o Servidor ARES)
// ==============================================================================
#define GATEWAY_WIFI_SSID         "ARES_BASE_WIFI"
#define GATEWAY_WIFI_PASS         "aresresiliencia2026"
// Backend oficial na internet (HTTPS). Para testar com backend local, use o
// comando serial: backend http://192.168.137.1:5026/api/ares/ingress
#define BACKEND_INGRESS_URL       "https://api-ares.dorneles.dev/api/ares/ingress"
#define M2M_API_KEY               "ares-m2m-secret-dev-change-in-production"
#define GATEWAY_HEARTBEAT_MS      20000 // Heartbeat HTTP a cada 20s (10s fazia o servidor responder 429)
#define GATEWAY_RATE_LIMIT_PAUSE_MS 60000 // Pausa em todos os envios HTTP quando o servidor responde 429 (sem Retry-After)
#define GATEWAY_INGRESS_RETRY_MS  5000  // Espera entre tentativas de reenviar um SOS que falhou
#define CUSTODY_RESEND_MS         4000  // Router reenvia pela malha os SOS ainda sem confirmação a cada 4s

// ==============================================================================
// PINAGEM DE LEDS E PULSO FÍSICO PARA GATEWAY INICIAL
// ==============================================================================
#define PIN_LED_SYSTEM_ALIVE     2   // GPIO 2: LED Onboard / Indicador de que o ESP está funcionando
#define PIN_LED_PACKET_ACTIVITY  4   // GPIO 4: LED Indicador de que uma mensagem passou pelo ESP
#define PIN_GATEWAY_BOOT_BTN     0   // GPIO 0: Botão BOOT nativo da placa (segurar >= 500ms promove a GATEWAY)
#define PIN_GATEWAY_PULSE        13  // GPIO 13: Entrada de pulso externo (LOW promove o nó a GATEWAY inicial)
#define PIN_BATTERY_ADC          34  // GPIO 34: Entrada ADC opcional com divisor 2x 100kΩ para leitura LiPo

// ==============================================================================
// CONFIGURAÇÕES DO STORE-AND-FORWARD (DTN)
// ==============================================================================
#define MAX_STORED_PACKETS        50
#define PACKET_RETENTION_MS       (3600 * 1000) // 1 hora de retenção em memória
#define BEACON_INTERVAL_MS        5000          // 5 segundos entre beacons

#endif // ARES_CONFIG_H
