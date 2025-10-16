#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ====== CONFIG Wi-Fi ======
const char* WIFI_SSID = "brisa-237559";
const char* WIFI_PASS = "mgdlqcbn";

// ====== CONFIG MQTT ======
const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;
String clientId = "franzininho-" + String((uint32_t)ESP.getEfuseMac(), HEX);

// ====== DHT ======
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ====== TÓPICOS MQTT ======
const char* TOPIC_TEMP = "IFCE_Gustavo/temperatura";
const char* TOPIC_UMID = "IFCE_Gustavo/umidade";
const char* TOPIC_LDR = "IFCE_Gustavo/luminosidade";
const char* TOPIC_BTN = "IFCE_Gustavo/botoes";
const char* TOPIC_TOUCH = "IFCE_Gustavo/touch";
const char* TOPIC_LED_STATUS = "IFCE_Gustavo/led_status";
const char* TOPIC_RGB_STATUS = "IFCE_Gustavo/rgb_status";
const char* TOPIC_LCD_STATUS = "IFCE_Gustavo/lcd_status";
const char* TOPIC_BUZZER_STATUS = "IFCE_Gustavo/buzzer_status";
const char* TOPIC_STATUS = "IFCE_Gustavo/status";

WiFiClient espClient;
PubSubClient mqtt(espClient);

// ====== PINOS FRANZININHO WIFI LAB ======
#define BUZZER_PIN 17
#define LED_R_PIN 14
#define LED_G_PIN 13
#define LED_B_PIN 12
#define LDR_PIN 1
#define BTN1_PIN 7
#define BTN2_PIN 6
#define BTN3_PIN 5
#define BTN4_PIN 4
#define BTN5_PIN 3
#define BTN6_PIN 2

// Touch capacitivo
#define TOUCH1_PIN 1
#define TOUCH2_PIN 2
#define TOUCH3_PIN 3
#define TOUCH4_PIN 4
#define TOUCH5_PIN 5
#define TOUCH6_PIN 6
#define TOUCH_THRESHOLD 1000

// Display OLED
#define OLED_SDA_PIN 8
#define OLED_SCL_PIN 9
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ====== TIMERS AUTOMÁTICOS ======
unsigned long lastTempHumid = 0;
const unsigned long TEMP_HUMID_INTERVAL = 60000; // 1 minuto

unsigned long lastLDR = 0;
const unsigned long LDR_INTERVAL = 60000; // 1 minuto

unsigned long lastLEDToggle = 0;
const unsigned long LED_TOGGLE_INTERVAL = 5000; // 5 segundos

unsigned long lastRGBChange = 0;
const unsigned long RGB_CHANGE_INTERVAL = 2000; // 2 segundos

unsigned long lastBtnCheck = 0;
const unsigned long BTN_CHECK_INTERVAL = 50; // 50ms para debounce

unsigned long lastBuzzer = 0;
const unsigned long BUZZER_INTERVAL = 10000; // 10 segundos (frequência baixa)

// ====== VARIÁVEIS DE ESTADO ======
bool lastBtnStates[6] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
bool lastTouchStates[6] = {false, false, false, false, false, false};

bool ledState = false; // Estado do LED (ligado/desligado)
int rgbCurrentColor = 0; // 0=Vermelho, 1=Verde, 2=Azul

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

void displayMelhorDeTodos() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 10);
  display.println("FRANZININHO WIFI LAB");
  
  display.setTextSize(2);
  display.setCursor(25, 30);
  display.println("MELHOR");
  display.setCursor(15, 50);
  display.println("DE TODOS");
  
  display.display();
}

void publishTemperatureHumidity() {
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  if (!isnan(temp) && !isnan(humidity)) {
    mqtt.publish(TOPIC_TEMP, String(temp, 2).c_str(), true);
    mqtt.publish(TOPIC_UMID, String(humidity, 2).c_str(), true);
    Serial.printf("[DHT] Temp: %.2f°C, Umidade: %.2f%% (publicado)\n", temp, humidity);
  } else {
    Serial.println("[DHT] Erro na leitura do sensor!");
  }
}

void publishLuminosity() {
  int ldrValue = analogRead(LDR_PIN);
  mqtt.publish(TOPIC_LDR, String(ldrValue).c_str(), true);
  Serial.printf("[LDR] Luminosidade: %d (publicado)\n", ldrValue);
}

void toggleLEDs() {
  ledState = !ledState;
  
  // Alterna todos os LEDs RGB juntos
  digitalWrite(LED_R_PIN, ledState ? HIGH : LOW);
  digitalWrite(LED_G_PIN, ledState ? HIGH : LOW);
  digitalWrite(LED_B_PIN, ledState ? HIGH : LOW);
  
  String ledStatus = ledState ? "LIGADO" : "DESLIGADO";
  mqtt.publish(TOPIC_LED_STATUS, ledStatus.c_str(), true);
  Serial.println("[LED] Status: " + ledStatus);
}

void changeRGBColor() {
  // Apaga todos os LEDs
  digitalWrite(LED_R_PIN, LOW);
  digitalWrite(LED_G_PIN, LOW);
  digitalWrite(LED_B_PIN, LOW);
  
  String colorName = "";
  
  // Acende apenas a cor atual
  switch (rgbCurrentColor) {
    case 0: // Vermelho
      digitalWrite(LED_R_PIN, HIGH);
      colorName = "VERMELHO";
      break;
    case 1: // Verde
      digitalWrite(LED_G_PIN, HIGH);
      colorName = "VERDE";
      break;
    case 2: // Azul
      digitalWrite(LED_B_PIN, HIGH);
      colorName = "AZUL";
      break;
  }
  
  mqtt.publish(TOPIC_RGB_STATUS, colorName.c_str(), true);
  Serial.println("[RGB] Cor atual: " + colorName);
  
  // Próxima cor
  rgbCurrentColor = (rgbCurrentColor + 1) % 3;
}

void checkButtons() {
  unsigned long now = millis();
  if (now - lastBtnCheck < BTN_CHECK_INTERVAL) return;
  lastBtnCheck = now;
  
  int btnPins[6] = {BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN, BTN5_PIN, BTN6_PIN};
  
  for (int i = 0; i < 6; i++) {
    bool currentState = digitalRead(btnPins[i]);
    
    // Detecta pressão (transição HIGH->LOW)
    if (lastBtnStates[i] == HIGH && currentState == LOW) {
      String btnMsg = "BOTAO_" + String(i + 1) + "_PRESSIONADO";
      mqtt.publish(TOPIC_BTN, btnMsg.c_str(), false);
      Serial.println("[BTN] " + btnMsg);
      
      // Som de confirmação para cada botão
      tone(BUZZER_PIN, 500 + (i * 100), 150);
    }
    
    lastBtnStates[i] = currentState;
  }
}

void checkTouch() {
  int touchPins[6] = {TOUCH1_PIN, TOUCH2_PIN, TOUCH3_PIN, TOUCH4_PIN, TOUCH5_PIN, TOUCH6_PIN};
  
  for (int i = 0; i < 6; i++) {
    int touchValue = touchRead(touchPins[i]);
    bool currentTouch = (touchValue < TOUCH_THRESHOLD);
    
    if (currentTouch != lastTouchStates[i]) {
      lastTouchStates[i] = currentTouch;
      
      if (currentTouch) {
        String touchMsg = "TOUCH_" + String(i + 1) + "_ATIVADO";
        mqtt.publish(TOPIC_TOUCH, touchMsg.c_str(), false);
        Serial.println("[TOUCH] " + touchMsg + " (valor: " + String(touchValue) + ")");
        
        // Som agudo para touch
        tone(BUZZER_PIN, 1200 + (i * 50), 100);
      }
    }
  }
}

void playLowFrequencyBuzzer() {
  // Frequência muito baixa (100Hz)
  tone(BUZZER_PIN, 100, 500);
  mqtt.publish(TOPIC_BUZZER_STATUS, "BUZZER_BAIXA_FREQUENCIA_100HZ", true);
  Serial.println("[BUZZER] Tocando frequência baixa (100Hz)");
}

void ensureMqtt() {
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Conectando...");
    
    if (mqtt.connect(clientId.c_str(), TOPIC_STATUS, 0, true, "offline")) {
      Serial.println(" Conectado!");
      
      mqtt.publish(TOPIC_STATUS, "online", true);
      mqtt.publish(TOPIC_LCD_STATUS, "MELHOR_DE_TODOS", true);
      
      Serial.println("Para receber todas as mensagens, inscreva-se em: IFCE_Gustavo/#");
      
    } else {
      Serial.printf(" Erro rc=%d. Tentando novamente...\n", mqtt.state());
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== FRANZININHO WIFI - MODO AUTOMÁTICO ===");
  Serial.println("Para monitorar tudo no HiveMQ, use: IFCE_Gustavo/#");
  
  // Inicializa I2C e Display
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERRO: Display OLED não encontrado!");
    while(1) delay(1000);
  }
  
  // Mostra "Melhor de todos" no display
  displayMelhorDeTodos();
  
  // Configura pinos
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  
  // Botões com pull-up
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);
  pinMode(BTN5_PIN, INPUT_PULLUP);
  pinMode(BTN6_PIN, INPUT_PULLUP);
  
  // Inicializa DHT
  dht.begin();
  
  // Conecta WiFi e MQTT
  connectWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  ensureMqtt();
  
  // Publicações iniciais
  publishTemperatureHumidity();
  publishLuminosity();
  changeRGBColor();
  playLowFrequencyBuzzer();
  
  Serial.println("=== SISTEMA AUTOMÁTICO ATIVO ===");
  Serial.println("Temperatura/Umidade: a cada 1 minuto");
  Serial.println("Luminosidade: a cada 1 minuto");
  Serial.println("LEDs ligam/desligam: a cada 5 segundos");
  Serial.println("RGB muda cor: a cada 2 segundos");
  Serial.println("Buzzer baixa frequência: a cada 10 segundos");
  Serial.println("Botões e Touch: detecção imediata");
}

void loop() {
  // Verifica conexões
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado! Reconectando...");
    connectWiFi();
  }
  
  if (!mqtt.connected()) {
    Serial.println("MQTT desconectado! Reconectando...");
    ensureMqtt();
  }
  
  mqtt.loop();
  
  unsigned long now = millis();
  
  // ===== TEMPERATURA E UMIDADE (1 MINUTO) =====
  if (now - lastTempHumid >= TEMP_HUMID_INTERVAL) {
    lastTempHumid = now;
    publishTemperatureHumidity();
  }
  
  // ===== LUMINOSIDADE (1 MINUTO) =====
  if (now - lastLDR >= LDR_INTERVAL) {
    lastLDR = now;
    publishLuminosity();
  }
  
  // ===== LEDs LIGA/DESLIGA (5 SEGUNDOS) =====
  if (now - lastLEDToggle >= LED_TOGGLE_INTERVAL) {
    lastLEDToggle = now;
    toggleLEDs();
  }
  
  // ===== RGB MUDA COR (2 SEGUNDOS) =====
  if (now - lastRGBChange >= RGB_CHANGE_INTERVAL) {
    lastRGBChange = now;
    changeRGBColor();
  }
  
  // ===== BUZZER BAIXA FREQUÊNCIA (10 SEGUNDOS) =====
  if (now - lastBuzzer >= BUZZER_INTERVAL) {
    lastBuzzer = now;
    playLowFrequencyBuzzer();
  }
  
  // ===== BOTÕES E TOUCH (IMEDIATO) =====
  checkButtons();
  checkTouch();
  
  delay(10);
}