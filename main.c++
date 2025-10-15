#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h> // Biblioteca para I2C
#include <Adafruit_GFX.h> // Biblioteca gráfica da Adafruit
#include <Adafruit_SSD1306.h> // Biblioteca para o display OLED SSD1306

// ====== CONFIG Wi-Fi ======
const char* WIFI_SSID = "IFCE";
const char* WIFI_PASS = "IFCE1234";

// ====== CONFIG MQTT (HiveMQ Public Broker) ======
const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883; // Sem TLS
// ID do cliente deve ser único no broker:
String clientId = "esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);

// ====== DHT ======
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ====== TÓPICOS ======
const char* TOPIC_TEMP = "IFCE_Iran/temperatura";
const char* TOPIC_UMID = "IFCE_Iran/umidade";
const char* TOPIC_LDR = "IFCE_Iran/ldr";
const char* TOPIC_BTN = "IFCE_Iran/botoes";
const char* TOPIC_LED = "IFCE_Iran/leds";
const char* TOPIC_CMD = "IFCE_Iran/cmd";
const char* TOPIC_STATUS = "IFCE_Iran/status";
const char* TOPIC_MSG = "IFCE_Iran/msg";

WiFiClient espClient;
PubSubClient mqtt(espClient);

// ====== TIMER ======
unsigned long lastPub = 0;
const unsigned long PUB_INTERVAL_MS = 10000; // 10 s

// ====== Pinos ======
#define BUZZER_PIN 17
#define LED_VERMELHO_PIN 14
#define LED_VERDE_PIN 13
#define LED_AZUL_PIN 12
#define LDR_PIN 1
#define BTN1_PIN 7
#define BTN2_PIN 6
#define BTN3_PIN 5
#define BTN4_PIN 4
#define BTN5_PIN 3
#define BTN6_PIN 2
#define OLED_SDA_PIN 8
#define OLED_SCL_PIN 9

// ====== CONFIG DISPLAY OLED ======
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ====== DECLARAÇÕES DE FUNÇÕES ======
void publishReadings();
void activateBuzzerAndLED(int freq);
void displayMessage(String msg);

// Função para conectar no Wi-Fi
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWi-Fi OK. IP: ");
  Serial.println(WiFi.localIP());
}

void messageCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.printf("[MQTT] Mensagem em %s: %s\n", topic, msg.c_str());
  
  String topicStr = String(topic);
  
  if (topicStr == TOPIC_CMD) {
    if (msg == "publish_now") {
      publishReadings();
    }
    else if (msg.startsWith("buzzer")) {
      int freq = 400;
      if (msg.length() > 7) {
        freq = msg.substring(7).toInt();
      }
      activateBuzzerAndLED(freq);
    }
    else if (msg.startsWith("led_vermelho")) {
      digitalWrite(LED_VERMELHO_PIN, msg.endsWith("1") ? HIGH : LOW);
    }
    else if (msg.startsWith("led_verde")) {
      digitalWrite(LED_VERDE_PIN, msg.endsWith("1") ? HIGH : LOW);
    }
    else if (msg.startsWith("led_azul")) {
      digitalWrite(LED_AZUL_PIN, msg.endsWith("1") ? HIGH : LOW);
    }
  }
  else if (topicStr == TOPIC_MSG) {
    displayMessage(msg);
  }
}

void activateBuzzerAndLED(int freq) {
  tone(BUZZER_PIN, freq);
  delay(1000);
  noTone(BUZZER_PIN);
  
  digitalWrite(LED_VERMELHO_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_VERMELHO_PIN, LOW);
}

void ensureMqtt() {
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Conectando... ");
    bool ok = mqtt.connect(
      clientId.c_str(),
      TOPIC_STATUS,
      0,
      true,
      "offline"
    );
    if (ok) {
      Serial.println("OK");
      mqtt.subscribe(TOPIC_CMD);
      mqtt.subscribe(TOPIC_MSG);
      mqtt.publish(TOPIC_STATUS, "online", true);
    } else {
      Serial.printf("falha rc=%d; tentando em 3s\n", mqtt.state());
      delay(3000);
    }
  }
}

void publishReadings() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("[DHT] Falha na leitura");
    return;
  }
  
  char bufTemp[16], bufHum[16];
  dtostrf(t, 4, 2, bufTemp);
  dtostrf(h, 4, 2, bufHum);
  mqtt.publish(TOPIC_TEMP, bufTemp, true);
  mqtt.publish(TOPIC_UMID, bufHum, true);
  
  int ldrValue = analogRead(LDR_PIN);
  char bufLdr[16];
  itoa(ldrValue, bufLdr, 10);
  mqtt.publish(TOPIC_LDR, bufLdr, true);

  String btnStatus = "";
  btnStatus += !digitalRead(BTN1_PIN) ? "1" : "0";
  btnStatus += !digitalRead(BTN2_PIN) ? "1" : "0";
  btnStatus += !digitalRead(BTN3_PIN) ? "1" : "0";
  btnStatus += !digitalRead(BTN4_PIN) ? "1" : "0";
  btnStatus += !digitalRead(BTN5_PIN) ? "1" : "0";
  btnStatus += !digitalRead(BTN6_PIN) ? "1" : "0";
  mqtt.publish(TOPIC_BTN, btnStatus.c_str(), true);
  
  String ledStatus = "";
  ledStatus += digitalRead(LED_VERMELHO_PIN) ? "1" : "0";
  ledStatus += digitalRead(LED_VERDE_PIN) ? "1" : "0";
  ledStatus += digitalRead(LED_AZUL_PIN) ? "1" : "0";
  mqtt.publish(TOPIC_LED, ledStatus.c_str(), true);
  
  Serial.printf("[PUB] %s = %s °C | %s = %s %% | %s = %s | %s = %s | %s = %s\n",
    TOPIC_TEMP, bufTemp, TOPIC_UMID, bufHum, TOPIC_LDR, 
    bufLdr, TOPIC_BTN, btnStatus.c_str(), TOPIC_LED, ledStatus.c_str());
}

void displayMessage(String msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(msg);
  display.display();
}

void setup() {
  Serial.begin(115200);
  
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  
  dht.begin();
  connectWiFi();
  
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_VERMELHO_PIN, OUTPUT);
  pinMode(LED_VERDE_PIN, OUTPUT);
  pinMode(LED_AZUL_PIN, OUTPUT);
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);
  pinMode(BTN5_PIN, INPUT_PULLUP);
  pinMode(BTN6_PIN, INPUT_PULLUP);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("Falha ao inicializar o display OLED"));
    while (1);
  }
  display.clearDisplay();
  display.display();
  
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(messageCallback);
  ensureMqtt();
  Serial.printf("Cliente MQTT = %s\n", clientId.c_str());
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  
  if (!mqtt.connected()) {
    ensureMqtt();
  }
  
  mqtt.loop();
  
  unsigned long now = millis();
  if (now - lastPub >= PUB_INTERVAL_MS) {
    lastPub = now;
    publishReadings();
  }
}