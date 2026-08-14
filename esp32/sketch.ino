#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

const char* SSID = "Wokwi-GUEST";
const char* PASSWORD = "";

// Configuração apontando diretamente para o domínio da sua VPS
const char* MQTT_SERVER = "smartpark-red.sytes.net";
const int MQTT_PORT = 1884; // broker do SmartPark-RED (1883 é de outro projeto na mesma VPS!)
const char* MQTT_TOPIC_STATUS = "estacionamento/vagas/status";

#define NEOPIXEL_PIN 4
#define NUM_VAGAS 15

struct Vaga {
  const char* id;
  const char* andar;
  int pino;
  bool preferencial;
  bool ocupada;
};

Vaga vagas[NUM_VAGAS] = {
  // Andar 1 (A)
  {"A01", "Andar 1", 13, false, false},
  {"A02", "Andar 1", 12, false, false},
  {"A03", "Andar 1", 14, false, false},
  {"A04", "Andar 1", 27, false, false},
  {"A05", "Andar 1", 26, true,  false},

  // Andar 2 (B)
  {"B01", "Andar 2", 25, false, false},
  {"B02", "Andar 2", 33, false, false},
  {"B03", "Andar 2", 32, false, false},
  {"B04", "Andar 2", 35, false, false},
  {"B05", "Andar 2", 34, true,  false},

  // Andar 3 (C)
  {"C01", "Andar 3", 23, false, false},
  {"C02", "Andar 3", 22, false, false},
  {"C03", "Andar 3", 21, false, false},
  {"C04", "Andar 3", 19, false, false},
  {"C05", "Andar 3", 18, true,  false}
};

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_NeoPixel pixels(NUM_VAGAS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setupWiFi() {
  Serial.print("Conectando ao Wi-Fi...");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[Wi-Fi] Conectado com sucesso!");
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Tentando conectar ao Broker MQTT (VPS: ");
    Serial.print(MQTT_SERVER);
    Serial.println(")...");

    String clientId = "SmartParkRed_ESP32_";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("[MQTT] Conectado a VPS!");
    } else {
      Serial.print("[MQTT] Falha na conexao. Codigo de erro: ");
      Serial.print(client.state());
      Serial.println(" -> Tentando novamente em 5 segundos...");
      delay(5000);
    }
  }
}

// LED individual por vaga: verde = livre, vermelho = ocupada comum,
// azul = ocupada preferencial (feedback visual local exigido pelo projeto)
void atualizarLed(int idx) {
  uint32_t cor;
  if (!vagas[idx].ocupada) {
    cor = pixels.Color(0, 255, 0);          // Verde
  } else if (vagas[idx].preferencial) {
    cor = pixels.Color(0, 0, 255);          // Azul
  } else {
    cor = pixels.Color(255, 0, 0);          // Vermelho
  }
  pixels.setPixelColor(idx, cor);
  pixels.show();
}

void publicarEstadoVaga(int idx) {
  Vaga vaga = vagas[idx];

  StaticJsonDocument<256> doc;
  doc["vaga_id"] = vaga.id;
  doc["andar"] = vaga.andar;
  doc["ocupada"] = vaga.ocupada;
  doc["preferencial"] = vaga.preferencial;
  doc["origem"] = "wokwi"; // identifica o Cenário 2 (sensor físico) no Node-RED

  char buffer[256];
  serializeJson(doc, buffer);

  // Envio do pacote para o Broker MQTT na VPS
  client.publish(MQTT_TOPIC_STATUS, buffer);

  atualizarLed(idx);

  // Exibição textual formatada no Serial Monitor
  Serial.print("[STATUS] VAGA ");
  Serial.print(vaga.id);
  Serial.print(" (");
  Serial.print(vaga.andar);
  Serial.print("): ");
  Serial.println(vaga.ocupada ? "OCUPADA [X]" : "LIVRE [ ]");
}

void setup() {
  Serial.begin(115200);

  pixels.begin();
  pixels.setBrightness(60);
  pixels.clear();
  pixels.show();

  for (int i = 0; i < NUM_VAGAS; i++) {
    pinMode(vagas[i].pino, INPUT_PULLUP);
  }

  setupWiFi();
  client.setServer(MQTT_SERVER, MQTT_PORT);

  // Realiza a leitura inicial de todas as vagas ao ligar
  reconnectMQTT();
  Serial.println("\n--- ESTADO INICIAL DAS VAGAS ---");
  for (int i = 0; i < NUM_VAGAS; i++) {
    vagas[i].ocupada = (digitalRead(vagas[i].pino) == LOW);
    publicarEstadoVaga(i);
    delay(50); // evita rajada excessiva no broker
  }
  Serial.println("--------------------------------\n");
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // Monitora alterações nos botões (mantenha pressionado no Wokwi para simular Ocupado)
  for (int i = 0; i < NUM_VAGAS; i++) {
    bool estadoAtual = (digitalRead(vagas[i].pino) == LOW);

    if (estadoAtual != vagas[i].ocupada) {
      vagas[i].ocupada = estadoAtual;
      publicarEstadoVaga(i);
    }
  }

  delay(200);
}
