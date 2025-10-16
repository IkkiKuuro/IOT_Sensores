#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ====== CONFIG Wi-Fi ======
const char* WIFI_SSID = "IFCE";
const char* WIFI_PASS = "IFCE1234";

// ====== CONFIG MQTT ======
const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;
String clientId = "franzininho-" + String((uint32_t)ESP.getEfuseMac(), HEX);

// ====== DHT ======
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

//========Comando Buzzer============
//IFCE_Iran/buzzer_control 100/1000/2000

// ====== TOPICOS MQTT ======
const char* TOPIC_BTN = "IFCE_Iran/botoes";
const char* TOPIC_TOUCH = "IFCE_Iran/touch";
const char* TOPIC_BUZZER_CONTROL = "IFCE_Iran/buzzer_control";
const char* TOPIC_BUZZER_STATUS = "IFCE_Iran/buzzer_status";
const char* TOPIC_LED_STATUS = "IFCE_Iran/led_status";

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

#define TOUCH1_PIN 8
#define TOUCH2_PIN 9
#define TOUCH3_PIN 10
#define TOUCH4_PIN 11
#define TOUCH5_PIN 16
#define TOUCH6_PIN 18
#define TOUCH_THRESHOLD 1000

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
bool lastTouch[6] = {false, false, false, false, false, false};
bool ledState = false;
unsigned long lastLedToggle = 0;
unsigned long lastDisplayUpdate = 0;
bool displayOK = false;

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
}

void ensureMqtt() {
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Conectando...");
    if (mqtt.connect(clientId.c_str())) {
      Serial.println(" conectado!");
      mqtt.subscribe(TOPIC_BUZZER_CONTROL);
      
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

void checkTouch() {
  int touchPins[6] = {TOUCH1_PIN, TOUCH2_PIN, TOUCH3_PIN, TOUCH4_PIN, TOUCH5_PIN, TOUCH6_PIN};
  for (int i = 0; i < 6; i++) {
    int val = touchRead(touchPins[i]);
    bool pressed = (val < TOUCH_THRESHOLD);
    if (pressed && !lastTouch[i]) {
      String topic = String(TOPIC_TOUCH) + "/TOUCH_" + String(i + 1);
      mqtt.publish(topic.c_str(), "ATIVADO");
      Serial.println("[TOUCH] " + topic + " -> " + String(val));
      tone(BUZZER_PIN, 1000 + (i * 50), 80);
    }
    lastTouch[i] = pressed;
  }
}

void updateDisplay() {
  if (!displayOK) return;
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Linha 1: Título
  display.setCursor(0, 0);
  display.println("=== FRANZININHO ===");
  
  // Linha 2: Status WiFi
  display.setCursor(0, 12);
  display.print("WiFi: ");
  if (WiFi.status() == WL_CONNECTED) {
    display.println("OK");
  } else {
    display.println("OFF");
  }
  
  // Linha 3: Status MQTT
  display.setCursor(0, 22);
  display.print("MQTT: ");
  display.println(mqtt.connected() ? "OK" : "OFF");
  
  // Linha 4: Status LED
  display.setCursor(0, 32);
  display.print("LEDs: ");
  display.println(ledState ? "ON" : "OFF");
  
  // Linha 5: Uptime
  display.setCursor(0, 42);
  display.print("Uptime: ");
  display.print(millis() / 1000);
  display.println("s");
  
  // Linha 6: Botões ativos
  display.setCursor(0, 52);
  display.print("BTN: ");
  bool anyPressed = false;
  for(int i = 0; i < 6; i++) {
    if(lastBtn[i] == LOW) {
      display.print(i+1);
      display.print(" ");
      anyPressed = true;
    }
  }
  if (!anyPressed) {
    display.print("---");
  }
  
  display.display();
}

void initDisplay() {
  Serial.println("\n[DISPLAY] Inicializando...");
  
  // Tenta primeiro com os pinos 8 e 9
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  delay(100);
  
  if(display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
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
  
  // Se não funcionar, tenta com pinos 41 e 42
  Serial.println("[DISPLAY] Tentando pinos alternativos...");
  Wire.begin(41, 42);
  delay(100);
  
  if(display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[DISPLAY] OK! (SDA=41, SCL=42)");
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
  
  // Se ainda não funcionar, escaneia I2C
  Serial.println("[DISPLAY] ERRO! Escaneando I2C...");
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  for(byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if(Wire.endTransmission() == 0) {
      Serial.printf("  -> Dispositivo encontrado em 0x%02X\n", addr);
    }
  }
  displayOK = false;
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== Franzininho WiFi LAB - MQTT Controle ===");
  Serial.println("Autor: IkkiKuuro");
  Serial.println("Data: 2025-10-16");

  // Configura pinos
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

  // Inicializa display
  initDisplay();
  delay(2000);

  // Conecta WiFi
  if (displayOK) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Conectando WiFi...");
    display.display();
  }
  
  connectWiFi();
  
  if (displayOK) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi: OK");
    display.println("");
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.display();
    delay(2000);
  }

  // Configura MQTT
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  
  if (displayOK) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Conectando MQTT...");
    display.display();
  }
  
  ensureMqtt();
  
  if (displayOK) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Sistema Online!");
    display.println("");
    display.println("Pronto para uso");
    display.display();
    delay(2000);
  }

  // Inicializa DHT
  dht.begin();
  
  Serial.println("\n[SETUP] Concluído!");
  Serial.println("=============================\n");
}

// ====== LOOP ======
void loop() {
  // Verifica conexões
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  
  if (!mqtt.connected()) {
    ensureMqtt();
  }
  
  mqtt.loop();

  unsigned long now = millis();
  
  // Toggle LEDs a cada 3 segundos
  if (now - lastLedToggle > 3000) {
    lastLedToggle = now;
    toggleLEDs();
  }

  // Atualiza display a cada 1 segundo
  if (now - lastDisplayUpdate > 1000) {
    lastDisplayUpdate = now;
    updateDisplay();
  }

  // Verifica botões e touch
  checkButtons();
  checkTouch();

  delay(30);
}