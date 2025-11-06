#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ====== CONFIG Wi-Fi ======
const char* WIFI_SSID = "IFCE";
const char* WIFI_PASS = "IFCE1234";

//========Comandos MQTT============
//IFCE_Iran/buzzer_control <frequencia>
//IFCE_Iran/display_msg <mensagem>
//IFCE_Iran/display_control CLEAR (limpa o display)
//IFCE_Iran/led_control RED/GREEN/BLUE/ALL ON/OFF/TOGGLE
//Exemplos: 
//  IFCE_Iran/led_control RED ON
//  IFCE_Iran/led_control GREEN OFF
//  IFCE_Iran/led_control BLUE TOGGLE
//  IFCE_Iran/led_control ALL ON
//  IFCE_Iran/display_control CLEAR



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
const char* TOPIC_LED_CONTROL = "IFCE_Iran/led_control";
const char* TOPIC_LED_STATUS = "IFCE_Iran/led_status";
const char* TOPIC_DISPLAY_MSG = "IFCE_Iran/display_msg";
const char* TOPIC_DISPLAY_CONTROL = "IFCE_Iran/display_control";
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
unsigned long btnPressTime[6] = {0, 0, 0, 0, 0, 0};
bool btnHoldReported[6] = {false, false, false, false, false, false};
bool ledRedState = false;
bool ledGreenState = false;
bool ledBlueState = false;
unsigned long lastDisplayUpdate = 0;
unsigned long lastSensorUpdate = 0;
bool displayOK = false;
String scrollingMsg = ""; // mensagem atual
unsigned long scrollTimer = 0;
int scrollX = SCREEN_WIDTH;

#define HOLD_TIME_MS 1000  // Tempo para considerar botão segurado (1 segundo)

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

void clearDisplay() {
  if (!displayOK) return;
  
  scrollingMsg = ""; // Limpa mensagem do letreiro
  display.clearDisplay();
  display.display();
  Serial.println("[DISPLAY] Display limpo!");
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

void controlLED(String color, String action) {
  color.toUpperCase();
  action.toUpperCase();
  
  if (color == "RED" || color == "VERMELHO" || color == "ALL") {
    if (action == "ON") ledRedState = true;
    else if (action == "OFF") ledRedState = false;
    else if (action == "TOGGLE") ledRedState = !ledRedState;
    digitalWrite(LED_R_PIN, ledRedState);
  }
  
  if (color == "GREEN" || color == "VERDE" || color == "ALL") {
    if (action == "ON") ledGreenState = true;
    else if (action == "OFF") ledGreenState = false;
    else if (action == "TOGGLE") ledGreenState = !ledGreenState;
    digitalWrite(LED_G_PIN, ledGreenState);
  }
  
  if (color == "BLUE" || color == "AZUL" || color == "ALL") {
    if (action == "ON") ledBlueState = true;
    else if (action == "OFF") ledBlueState = false;
    else if (action == "TOGGLE") ledBlueState = !ledBlueState;
    digitalWrite(LED_B_PIN, ledBlueState);
  }
  
  // Publica status atualizado
  String status = "R:";
  status += ledRedState ? "ON" : "OFF";
  status += " G:";
  status += ledGreenState ? "ON" : "OFF";
  status += " B:";
  status += ledBlueState ? "ON" : "OFF";
  mqtt.publish(TOPIC_LED_STATUS, status.c_str());
  Serial.println("[LED] " + status);
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

  else if (String(topic) == TOPIC_LED_CONTROL) {
    // Formato esperado: "RED ON" ou "GREEN OFF" ou "BLUE TOGGLE" ou "ALL ON"
    int spaceIndex = msg.indexOf(' ');
    if (spaceIndex > 0) {
      String color = msg.substring(0, spaceIndex);
      String action = msg.substring(spaceIndex + 1);
      controlLED(color, action);
    } else {
      mqtt.publish(TOPIC_LED_STATUS, "Formato invalido! Use: COR ACAO");
    }
  }

  else if (String(topic) == TOPIC_DISPLAY_MSG) {
    showScrollingMessage(msg);
  }

  else if (String(topic) == TOPIC_DISPLAY_CONTROL) {
    msg.toUpperCase();
    if (msg == "CLEAR" || msg == "LIMPAR") {
      clearDisplay();
      mqtt.publish(TOPIC_DISPLAY_CONTROL, "Display limpo!");
    }
  }
}

void ensureMqtt() {
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Conectando...");
    if (mqtt.connect(clientId.c_str())) {
      Serial.println(" conectado!");
      mqtt.subscribe(TOPIC_BUZZER_CONTROL);
      mqtt.subscribe(TOPIC_LED_CONTROL);
      mqtt.subscribe(TOPIC_DISPLAY_MSG);
      mqtt.subscribe(TOPIC_DISPLAY_CONTROL);

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



void checkButtons() {
  int btnPins[6] = {BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN, BTN5_PIN, BTN6_PIN};
  unsigned long now = millis();
  
  for (int i = 0; i < 6; i++) {
    bool current = digitalRead(btnPins[i]);
    String topic = String(TOPIC_BTN) + "/BOTAO_" + String(i + 1);
    
    // Detecta pressão inicial (HIGH -> LOW)
    if (lastBtn[i] == HIGH && current == LOW) {
      btnPressTime[i] = now;
      btnHoldReported[i] = false;
      mqtt.publish(topic.c_str(), "PRESSIONADO");
      Serial.println("[BOTAO] " + topic + " PRESSIONADO");
      tone(BUZZER_PIN, 700 + (i * 50), 100);
    }
    
    // Botão está sendo segurado
    else if (current == LOW && !btnHoldReported[i]) {
      unsigned long holdDuration = now - btnPressTime[i];
      if (holdDuration >= HOLD_TIME_MS) {
        mqtt.publish(topic.c_str(), "SEGURANDO");
        Serial.println("[BOTAO] " + topic + " SEGURANDO");
        btnHoldReported[i] = true;
        tone(BUZZER_PIN, 1000 + (i * 100), 200);
      }
    }
    
    // Botão foi solto
    else if (lastBtn[i] == LOW && current == HIGH) {
      unsigned long holdDuration = now - btnPressTime[i];
      if (holdDuration >= HOLD_TIME_MS) {
        mqtt.publish(topic.c_str(), "SOLTO_APOS_SEGURAR");
        Serial.println("[BOTAO] " + topic + " SOLTO (segurado por " + String(holdDuration) + "ms)");
      } else {
        mqtt.publish(topic.c_str(), "SOLTO");
        Serial.println("[BOTAO] " + topic + " SOLTO");
      }
      btnHoldReported[i] = false;
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
  int luminosidade = map(ldrValue, 0, 4095, 0, 100); // Converte para porcentagem (0-100%)
  luminosidade = constrain(luminosidade, 0, 100); // Garante que fica entre 0 e 100
  
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
  String lumStr = String(luminosidade) + "%";
  mqtt.publish(TOPIC_LUMINOSIDADE, lumStr.c_str());
  Serial.println("[SENSOR] Luminosidade: " + lumStr + " (LDR: " + String(ldrValue) + ")");
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
  display.print("LEDs: R:");
  display.print(ledRedState ? "1" : "0");
  display.print(" G:");
  display.print(ledGreenState ? "1" : "0");
  display.print(" B:");
  display.println(ledBlueState ? "1" : "0");
  
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