/*
 * SmartPark-RED :: Firmware ESP32 (Wokwi)
 * Estacionamento inteligente de 3 andares / 15 vagas (A01-C05)
 *
 * - 15 botões digitais simulam os sensores de ocupação de cada vaga.
 * - 1 fita NeoPixel (WS2812, 15 LEDs) no pino DIN dá feedback visual
 *   individual por vaga (verde=livre, vermelho=ocupada comum,
 *   azul=ocupada preferencial) sem estourar o orçamento de pinos do ESP32.
 * - Publica no broker MQTT da VPS a cada mudança de estado (com debounce)
 *   e faz uma varredura completa no boot para sincronizar o backend.
 *
 * Bibliotecas (Library Manager / Wokwi libraries.txt):
 *   - PubSubClient (Nick O'Leary)
 *   - ArduinoJson (Benoit Blanchon) v6+
 *   - Adafruit NeoPixel
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

// ---------------------------------------------------------------------
// CONFIGURAÇÃO DE REDE
// ---------------------------------------------------------------------
const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASSWORD = "";

const char *MQTT_HOST = "smartpark-red.sytes.net";
const int   MQTT_PORT = 1884; // ver docker-compose.yml / mosquitto (host port)
const char *MQTT_CLIENT_ID = "esp32-smartpark-red";

const char *TOPIC_STATUS = "estacionamento/vagas/status";

// ---------------------------------------------------------------------
// MAPEAMENTO DE VAGAS -> PINO FÍSICO (deve espelhar postgres/init/02_seed.sql)
// ---------------------------------------------------------------------
#define NUM_VAGAS 15
#define NEOPIXEL_PIN 4

struct Vaga {
    const char *id;
    const char *andar;
    uint8_t pino;
    bool preferencial;
    bool ocupada;        // estado atual conhecido
    bool ultimaLeitura;  // última leitura bruta do pino (para debounce)
    unsigned long ultimaMudancaMs;
};

Vaga vagas[NUM_VAGAS] = {
    {"A01", "Andar 1", 13, false, false, false, 0},
    {"A02", "Andar 1", 12, false, false, false, 0},
    {"A03", "Andar 1", 14, false, false, false, 0},
    {"A04", "Andar 1", 27, false, false, false, 0},
    {"A05", "Andar 1", 26, true,  false, false, 0},

    {"B01", "Andar 2", 25, false, false, false, 0},
    {"B02", "Andar 2", 33, false, false, false, 0},
    {"B03", "Andar 2", 32, false, false, false, 0},
    {"B04", "Andar 2", 35, false, false, false, 0},
    {"B05", "Andar 2", 34, true,  false, false, 0},

    {"C01", "Andar 3", 23, false, false, false, 0},
    {"C02", "Andar 3", 22, false, false, false, 0},
    {"C03", "Andar 3", 21, false, false, false, 0},
    {"C04", "Andar 3", 19, false, false, false, 0},
    {"C05", "Andar 3", 18, true,  false, false, 0},
};

const unsigned long DEBOUNCE_MS = 50;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
Adafruit_NeoPixel pixels(NUM_VAGAS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ---------------------------------------------------------------------
// WI-FI
// ---------------------------------------------------------------------
void conectarWifi() {
    Serial.printf("Conectando ao WiFi \"%s\"...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }
    Serial.printf("\nWiFi conectado. IP: %s\n", WiFi.localIP().toString().c_str());
}

// ---------------------------------------------------------------------
// LEDs (NeoPixel) — 1 pixel por vaga
// ---------------------------------------------------------------------
void atualizarLed(int idx) {
    uint32_t cor;
    if (!vagas[idx].ocupada) {
        cor = pixels.Color(0, 255, 0);        // Verde = livre
    } else if (vagas[idx].preferencial) {
        cor = pixels.Color(0, 0, 255);        // Azul = ocupada preferencial
    } else {
        cor = pixels.Color(255, 0, 0);        // Vermelho = ocupada comum
    }
    pixels.setPixelColor(idx, cor);
    pixels.show();
}

void atualizarTodosLeds() {
    for (int i = 0; i < NUM_VAGAS; i++) atualizarLed(i);
}

// ---------------------------------------------------------------------
// MQTT
// ---------------------------------------------------------------------
void conectarMqtt() {
    while (!mqttClient.connected()) {
        Serial.printf("Conectando ao broker MQTT %s:%d...\n", MQTT_HOST, MQTT_PORT);
        if (mqttClient.connect(MQTT_CLIENT_ID)) {
            Serial.println("MQTT conectado.");
        } else {
            Serial.printf("Falha (rc=%d). Nova tentativa em 2s.\n", mqttClient.state());
            delay(2000);
        }
    }
}

void publicarStatus(int idx) {
    StaticJsonDocument<256> doc;
    doc["vaga_id"] = vagas[idx].id;
    doc["ocupada"] = vagas[idx].ocupada;
    doc["preferencial"] = vagas[idx].preferencial;
    doc["andar"] = vagas[idx].andar;
    doc["origem"] = "wokwi"; // identifica a via física para o Cenário 2 (ver Flow 1 no Node-RED)

    char payload[256];
    size_t n = serializeJson(doc, payload);

    mqttClient.publish(TOPIC_STATUS, (const uint8_t *)payload, n, false);
    Serial.printf("-> %s: %s\n", TOPIC_STATUS, payload);
}

// ---------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);

    pixels.begin();
    pixels.setBrightness(60);
    pixels.clear();
    pixels.show();

    for (int i = 0; i < NUM_VAGAS; i++) {
        pinMode(vagas[i].pino, INPUT_PULLUP); // botão fecha para GND = ocupada
    }

    conectarWifi();
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    conectarMqtt();

    // Varredura inicial: sincroniza a VPS com o estado real de todos os pinos
    Serial.println("Varredura inicial das 15 vagas...");
    for (int i = 0; i < NUM_VAGAS; i++) {
        bool leitura = (digitalRead(vagas[i].pino) == LOW); // LOW = pressionado = ocupado
        vagas[i].ocupada = leitura;
        vagas[i].ultimaLeitura = leitura;
        vagas[i].ultimaMudancaMs = millis();
        atualizarLed(i);
        publicarStatus(i);
        delay(50); // evita rajada excessiva no broker
    }
    Serial.println("Sincronização inicial concluída.");
}

// ---------------------------------------------------------------------
// LOOP
// ---------------------------------------------------------------------
void loop() {
    if (WiFi.status() != WL_CONNECTED) conectarWifi();
    if (!mqttClient.connected()) conectarMqtt();
    mqttClient.loop();

    unsigned long agora = millis();

    for (int i = 0; i < NUM_VAGAS; i++) {
        bool leituraAtual = (digitalRead(vagas[i].pino) == LOW);

        if (leituraAtual != vagas[i].ultimaLeitura) {
            vagas[i].ultimaMudancaMs = agora; // reinicia contagem de debounce
            vagas[i].ultimaLeitura = leituraAtual;
        }

        if ((agora - vagas[i].ultimaMudancaMs) > DEBOUNCE_MS && leituraAtual != vagas[i].ocupada) {
            vagas[i].ocupada = leituraAtual;
            atualizarLed(i);
            publicarStatus(i);
        }
    }
}
