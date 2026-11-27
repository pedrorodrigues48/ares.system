#include "ares_radio.h"
#include <esp_wifi.h>

AresRadio* AresRadio::instance = nullptr;

AresRadio::AresRadio() : packetCallback(nullptr), lastRssi(0), seenIndex(0) {
    memset(seenMsgIds, 0, sizeof(seenMsgIds));
}

AresRadio& AresRadio::getInstance() {
    if (!instance) {
        instance = new AresRadio();
    }
    return *instance;
}

// [LONGO ALCANCE] Liga b/g/n + LR na interface STA e põe a potência no máximo.
// Pode ser chamada antes do WiFi.begin() do Gateway (ideal: mudar o protocolo com o
// Wi-Fi já conectado pode derrubar a conexão por um instante).
void AresRadio::applyRadioConfig() {
    if (WiFi.getMode() == WIFI_MODE_NULL) {
        WiFi.mode(WIFI_STA);
    }

    // [LONGO ALCANCE] Interface STA (a que o ESP-NOW usa) em 802.11 b/g/n + LR.
    // O b/g/n continua ligado para o Gateway conectar no hotspot ARES_BASE_WIFI.
    // O SoftAP do Router (celulares) não é tocado: continua só b/g/n.
#if ARES_RADIO_LONG_RANGE
    const uint8_t wanted = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR;
    uint8_t current = 0;
    esp_wifi_get_protocol(WIFI_IF_STA, &current);
    if (current != wanted) { // só mexe se precisar (não derruba o Wi-Fi do Gateway à toa)
        esp_err_t protoErr = esp_wifi_set_protocol(WIFI_IF_STA, wanted);
        if (protoErr != ESP_OK) {
            Serial.printf("[ARES RADIO] Falha ao ativar Long Range (erro %d)\n", protoErr);
        }
    }
#endif

    // [LONGO ALCANCE] Potência máxima (antes 8,5 dBm por suspeita de brownout, que era a alimentação USB)
    WiFi.setTxPower(ARES_RADIO_TX_POWER);
}

bool AresRadio::begin(uint8_t channel) {
    if (WiFi.getMode() == WIFI_MODE_NULL) {
        WiFi.mode(WIFI_STA);
    }

    applyRadioConfig();

    // Inicializa ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ARES RADIO] Erro ao inicializar ESP-NOW!");
        return false;
    }

    esp_now_register_send_cb(AresRadio::onDataSent);
    esp_now_register_recv_cb(AresRadio::onDataRecv);

    // Adiciona peer de broadcast padrão com canal 0 (segue o canal do rádio automaticamente)
    esp_now_peer_info_t peerInfo = {};
    uint8_t broadcastMac[] = BROADCAST_MAC;
    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = 0; // Canal 0 = Home Channel atual do rádio (evita erro 12390)
    peerInfo.encrypt = false;

    if (!esp_now_is_peer_exist(broadcastMac)) {
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("[ARES RADIO] Falha ao adicionar peer de broadcast!");
            return false;
        }
    }

#if ARES_RADIO_LONG_RANGE
    // Sem isto o ESP-NOW continua enviando a 1 Mbps normal; com isto os pacotes saem no modo LR.
    esp_err_t rateErr = esp_wifi_config_espnow_rate(WIFI_IF_STA, ARES_RADIO_LR_RATE);
    if (rateErr != ESP_OK) {
        Serial.printf("[ARES RADIO] Falha ao definir taxa LR do ESP-NOW (erro %d)\n", rateErr);
    }
#endif

    int8_t txQuarterDbm = 0;
    esp_wifi_get_max_tx_power(&txQuarterDbm);
    Serial.printf("[ARES RADIO] ESP-NOW ativo no canal %d. MAC Local: %s\n", channel, WiFi.macAddress().c_str());
    Serial.printf("[ARES RADIO] Potencia TX: %.1f dBm | Long Range: %s\n",
                  txQuarterDbm / 4.0f, ARES_RADIO_LONG_RANGE ? "LIGADO" : "desligado");
    return true;
}

bool AresRadio::sendBroadcast(const AresMessagePacket *packet) {
    uint8_t broadcastMac[] = BROADCAST_MAC;
    return sendToMac(broadcastMac, packet);
}

bool AresRadio::sendToMac(const uint8_t *targetMac, const AresMessagePacket *packet) {
    esp_err_t result = esp_now_send(targetMac, (uint8_t*)packet, sizeof(AresMessagePacket));
    if (result == ESP_OK) {
        return true;
    } else {
        Serial.printf("[ARES RADIO] Erro no envio ESP-NOW: %d\n", result);
        return false;
    }
}

void AresRadio::setPacketCallback(AresPacketReceivedCallback callback) {
    packetCallback = callback;
}

bool AresRadio::hasSeenMessage(const char *msgId) {
    if (!msgId || strlen(msgId) == 0) return false;
    for (int i = 0; i < 32; i++) {
        if (strncmp(seenMsgIds[i], msgId, 32) == 0) {
            return true;
        }
    }
    return false;
}

void AresRadio::markMessageSeen(const char *msgId) {
    if (!msgId || strlen(msgId) == 0) return;
    strncpy(seenMsgIds[seenIndex], msgId, 32);
    seenIndex = (seenIndex + 1) % 32;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void AresRadio::onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    // Status callback em ESP-IDF 5+
}

void AresRadio::onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
    if (data_len < (int)sizeof(AresMessagePacket)) {
        return;
    }

    const AresMessagePacket *packet = (const AresMessagePacket*)data;
    if (instance && instance->packetCallback && recv_info) {
        int rssi = recv_info->rx_ctrl ? recv_info->rx_ctrl->rssi : -60;
        instance->lastRssi = rssi;
        instance->packetCallback(recv_info->src_addr, packet, rssi);
    }
}
#else
void AresRadio::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // Status callback em ESP-IDF 4
}

void AresRadio::onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    if (data_len < (int)sizeof(AresMessagePacket)) {
        return;
    }

    const AresMessagePacket *packet = (const AresMessagePacket*)data;
    if (instance && instance->packetCallback) {
        instance->packetCallback(mac_addr, packet, instance->lastRssi);
    }
}
#endif
