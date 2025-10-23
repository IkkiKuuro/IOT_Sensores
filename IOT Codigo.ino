#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ====== CONFIG Wi-Fi ======
const char* WIFI_SSID = "IFCE";
const char* WIFI_PASS = "IFCE1234";

//========Comando Buzzer============
//IFCE_Iran/buzzer_control
//IFCE_Iran/display_msg



// ====== CONFIG MQTT ======
const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;
String clientId = "franzininho-" + String((uint32_t)ESP.getEfuseMac(), HEX);

// ====== DHT ======
#define DHTPIN 15
#define DHTTYPE DHT11 
DHT dht(DHTPIN, DHTTYPE);

// ====== TOPICOS MQTT ======
const char* TOPIC_BTN = "IFCE_Iran/botoes";
const char* TOPIC_BUZZER_CONTROL = "IFCE_Iran/buzzer_control";
const char* TOPIC_BUZZER_STATUS = "IFCE_Iran/buzzer_status";
const char* TOPIC_LED_STATUS = "IFCE_Iran/led_status";
const char* TOPIC_DISPLAY_MSG = "IFCE_Iran/display_msg";
const char* TOPIC_TEMPERATURA = "IFCE_Iran/temperatura";
const char* TOPIC_UMIDADE = "IFCE_Iran/umidade";
const char* TOPIC_LUMINOSIDADE = "IFCE_Iran/luminosidade";

WiFiClient espClient;
PubSubClient mqtt(espClient);

// ====== PINOS ======
#define LED_R_PIN 14
#define LED_G_PIN 13
#define LED_B_PIN 12
#define BUZZER_PIN 17

#define BTN1_PIN 2
#define BTN2_PIN 3
#define BTN3_PIN 4
#define BTN4_PIN 5
#define BTN5_PIN 6
#define BTN6_PIN 7

#define LDR_PIN 1  // Pino analógico para sensor de luminosidade (LDR)

// ====== DISPLAY OLED ======
#define OLED_SDA_PIN 8  // Se não funcionar, teste com 41
#define OLED_SCL_PIN 9  // Se não funcionar, teste com 42
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ====== VARIÁVEIS ======
bool lastBtn[6] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
bool ledState = false;
unsigned long lastLedToggle = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastSensorUpdate = 0;
bool displayOK = false;
String scrollingMsg = ""; // mensagem atual
unsigned long scrollTimer = 0;
int scrollX = SCREEN_WIDTH;

// ====== FUNÇÕES ======

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi conectado: " + WiFi.localIP().toString());
}

void showScrollingMessage(String msg) {
  if (!displayOK) return;

  scrollingMsg = msg;
  scrollX = SCREEN_WIDTH; // reinicia o letreiro
  Serial.println("[DISPLAY] Nova mensagem: " + msg);
}

void drawScrollingMessage() {
  if (!displayOK || scrollingMsg == "") return;

  if (millis() - scrollTimer > 50) {  // velocidade do scroll
    scrollTimer = millis();

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(scrollX, 28);
    display.print(scrollingMsg);
    display.display();

    scrollX--;
    if (scrollX < -((int)scrollingMsg.length() * 6)) {
      scrollX = SCREEN_WIDTH;  // reinicia o letreiro
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();

  Serial.printf("[MQTT] %s => %s\n", topic, msg.c_str());

  if (String(topic) == TOPIC_BUZZER_CONTROL) {
    int freq = msg.toInt();
    if (freq >= 50 && freq <= 5000) {
      tone(BUZZER_PIN, freq, 500);
      mqtt.publish(TOPIC_BUZZER_STATUS, ("Tocando " + String(freq) + "Hz").c_str());

      if (displayOK) {
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 20);
        display.printf("Buzzer\n%d Hz", freq);
        display.display();
      }
    } else {
      mqtt.publish(TOPIC_BUZZER_STATUS, "Frequencia invalida!");
    }
  }

  else if (String(topic) == TOPIC_DISPLAY_MSG) {
    showScrollingMessage(msg);
  }
}

void ensureMqtt() {
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Conectando...");
    if (mqtt.connect(clientId.c_str())) {
      Serial.println(" conectado!");
      mqtt.subscribe(TOPIC_BUZZER_CONTROL);
      mqtt.subscribe(TOPIC_DISPLAY_MSG); // Novo tópico

      if (displayOK) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 30);
        display.println("MQTT: Conectado!");
        display.display();
      }
    } else {
      Serial.println(" falhou, tentando novamente...");
      delay(2000);
    }
  }
}

void toggleLEDs() {
  ledState = !ledState;
  digitalWrite(LED_R_PIN, ledState);
  digitalWrite(LED_G_PIN, ledState);
  digitalWrite(LED_B_PIN, ledState);
  mqtt.publish(TOPIC_LED_STATUS, ledState ? "LIGADOS" : "DESLIGADOS");
}

void checkButtons() {
  int btnPins[6] = {BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN, BTN5_PIN, BTN6_PIN};
  for (int i = 0; i < 6; i++) {
    bool current = digitalRead(btnPins[i]);
    if (lastBtn[i] == HIGH && current == LOW) {
      String topic = String(TOPIC_BTN) + "/BOTAO_" + String(i + 1);
      mqtt.publish(topic.c_str(), "PRESSIONADO");
      Serial.println("[BOTAO] " + topic);
      tone(BUZZER_PIN, 700 + (i * 50), 100);
    }
    lastBtn[i] = current;
  }
}

void readAndPublishSensors() {
  // Lê temperatura e umidade do DHT11
  float temp = dht.readTemperature();
  float humid = dht.readHumidity();
  
  // Lê luminosidade do LDR (0-4095 no ESP32)
  int ldrValue = analogRead(LDR_PIN);
  float luminosidade = map(ldrValue, 0, 4095, 0, 100); // Converte para porcentagem
  
  // Verifica se as leituras do DHT são válidas
  if (isnan(temp) || isnan(humid)) {
    Serial.println("[SENSOR] Erro ao ler DHT11!");
    mqtt.publish(TOPIC_TEMPERATURA, "ERRO");
    mqtt.publish(TOPIC_UMIDADE, "ERRO");
  } else {
    // Publica temperatura
    String tempStr = String(temp, 1) + "C";
    mqtt.publish(TOPIC_TEMPERATURA, tempStr.c_str());
    Serial.println("[SENSOR] Temperatura: " + tempStr);
    
    // Publica umidade
    String humidStr = String(humid, 1) + "%";
    mqtt.publish(TOPIC_UMIDADE, humidStr.c_str());
    Serial.println("[SENSOR] Umidade: " + humidStr);
  }
  
  // Publica luminosidade
  String lumStr = String(luminosidade, 0) + "%";
  mqtt.publish(TOPIC_LUMINOSIDADE, lumStr.c_str());
  Serial.println("[SENSOR] Luminosidade: " + lumStr);
}

void updateDisplay() {
  if (!displayOK || scrollingMsg != "") return;  // não atualiza se o letreiro estiver ativo
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.println("=== FRANZININHO ===");
  
  display.setCursor(0, 12);
  display.print("WiFi: ");
  display.println(WiFi.status() == WL_CONNECTED ? "OK" : "OFF");
  
  display.setCursor(0, 22);
  display.print("MQTT: ");
  display.println(mqtt.connected() ? "OK" : "OFF");
  
  display.setCursor(0, 32);
  display.print("LEDs: ");
  display.println(ledState ? "ON" : "OFF");
  
  display.setCursor(0, 42);
  display.print("Uptime: ");
  display.print(millis() / 1000);
  display.println("s");
  
  display.display();
}

void initDisplay() {
  Serial.println("\n[DISPLAY] Inicializando...");
  
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  delay(100);
  
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.printf("[DISPLAY] OK! (SDA=%d, SCL=%d)\n", OLED_SDA_PIN, OLED_SCL_PIN);
    displayOK = true;
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Franzininho WiFi");
    display.println("LAB - IoT");
    display.println("");
    display.println("Inicializando...");
    display.display();
    return;
  }

  Serial.println("[DISPLAY] ERRO! OLED nao encontrado!");
  displayOK = false;
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== Franzininho WiFi LAB - MQTT Controle ===");
  Serial.println("Autor: IkkiKuuro + Cauã");
  Serial.println("Data: 2025-10-16");

  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);
  pinMode(BTN5_PIN, INPUT_PULLUP);
  pinMode(BTN6_PIN, INPUT_PULLUP);

  pinMode(LDR_PIN, INPUT);  // Configura pino do LDR como entrada

  initDisplay();
  delay(2000);

  if (displayOK) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Conectando WiFi...");
    display.display();
  }
  
  connectWiFi();
  
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  
  ensureMqtt();

  dht.begin();
  
  Serial.println("\n[SETUP] Concluído!");
}

// ====== LOOP ======
void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) ensureMqtt();
  mqtt.loop();

  unsigned long now = millis();
  
  if (now - lastLedToggle > 3000) {
    lastLedToggle = now;
    toggleLEDs();
  }

  if (now - lastDisplayUpdate > 1000) {
    lastDisplayUpdate = now;
    updateDisplay();
  }

  // Atualiza sensores a cada 30 segundos
  if (now - lastSensorUpdate > 30000) {
    lastSensorUpdate = now;
    readAndPublishSensors();
  }

  drawScrollingMessage(); // Atualiza o letreiro
  checkButtons();

  delay(20);
}