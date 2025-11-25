#include <Arduino.h>
#include <RadioLib.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <WebSerial.h>
#include <time.h>
#include "env.h"
#include "Logger.h"
#include "web_routes.h"

// --- Protocol and Security Definitions ---
const uint8_t SECRET_KEY[16] = {SECRET_KEY_BYTES};

#pragma pack(push, 1)
struct Packet {
   uint16_t volts;
   int16_t  temp; // Scaled by 100
   uint16_t counter;
   uint32_t mic;
};
#pragma pack(pop)

extern "C" int siphash(const void *in, const size_t inlen, const void *k, uint8_t *out, const size_t outlen);

// --- LoRa Configuration Parameters ---
#define LORA_FREQUENCY      868.525F
#define LORA_BANDWIDTH      125.0F
#define LORA_SPREADING_FACTOR 12   // Long range
#define LORA_CODING_RATE    5    // Long range (4/5)
#define LORA_SYNC_WORD      0x14
#define LORA_OUTPUT_POWER   0     // Max power
#define LORA_PREAMBLE       16

// --- MQTT Configuration ---
#define MQTT_PORT           1883
#define MQTT_TOPIC          "radiolib/lora/data"
#define MQTT_HEARTBEAT_TOPIC "radiolib/heartbeat"

// --- Global Objects ---
AsyncWebServer server(80);
SX1262 radio = new Module(8, 14, 12, 13);
WiFiClient espClient;
PubSubClient client(espClient);

// --- State Variables ---
volatile bool loraInterruptFlag = true;
uint16_t last_seen_counter = 0;
unsigned long lastHeartbeatTime = 0;

void IRAM_ATTR setFlag(void) {
  loraInterruptFlag = true;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Logger.print("Message arrived [");
  Logger.print(topic);
  Logger.print("] ");
  for (int i = 0; i < length; i++) {
    Logger.print((char)payload[i]);
  }
  Logger.println();
}

void reconnectMqtt() {
  while (!client.connected()) {
    Logger.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Logger.println("connected");
    } else {
      Logger.print("failed, rc=");
      Logger.print(client.state());
      Logger.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void WebSerialMessageHandler(uint8_t *data, size_t len) {
  WebSerial.println("Received Data...");
  String d = "";
  for(size_t i=0; i < len; i++){
    d += char(data[i]);
  }
  WebSerial.println(d);
}

String getFormattedTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "Time not set";
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

void setup() {
  Serial.begin(115200);
  
  // Set up the logger to use both Serial and WebSerial
  Logger.addOutput(&Serial);
  Logger.addOutput(&WebSerial);

  Logger.println("Booting Heltec V3 Receiver...");

  // Connect to Wi-Fi
  Logger.print("Connecting to WiFi ");
  Logger.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Logger.print(".");
  }
  Logger.println("\nWiFi connected");
  Logger.print("IP address: ");
  Logger.println(WiFi.localIP());

  // Initialize NTP
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  // Setup WebServer, WebSerial, and all web routes
  setupWebRoutes(server);
  WebSerial.begin(&server);
  WebSerial.onMessage(WebSerialMessageHandler);
  server.begin();

  // --- OTA Setup ---
  ArduinoOTA
    .onStart([]() { Logger.println("Start updating sketch"); })
    .onEnd([]() { Logger.println("\nEnd"); })
    .onProgress([](unsigned int progress, unsigned int total) { Logger.printf("Progress: %u%%\r", (progress / (total / 100))); })
    .onError([](ota_error_t error) {
      Logger.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Logger.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Logger.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Logger.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Logger.println("Receive Failed");
      else if (error == OTA_END_ERROR) Logger.println("End Failed");
    });
  ArduinoOTA.begin();

  // Set up MQTT
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqttCallback);

  int state = radio.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREADING_FACTOR, LORA_CODING_RATE, LORA_SYNC_WORD, LORA_OUTPUT_POWER, LORA_PREAMBLE, 1.8, false);
  if (state != RADIOLIB_ERR_NONE) {
    Logger.print("Radio configuration failed, code ");
    Logger.println(state);
    while (true);
  }

  Logger.println("Radio initialized successfully!");

  radio.setDio1Action(setFlag);

  Logger.println(F("Starting to listen for LoRa packets..."));
  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Logger.print(F("failed, code "));
    Logger.println(state);
    while (true);
  }
}

void loop() {
  ArduinoOTA.handle();
  WebSerial.loop();

  if (!client.connected()) {
    reconnectMqtt();
  }
  client.loop();

  // Heartbeat Logic
  unsigned long currentMillis = millis();
  if (currentMillis - lastHeartbeatTime >= 60000) {
    lastHeartbeatTime = currentMillis;
    
    StaticJsonDocument<256> heartbeatDoc;
    heartbeatDoc["uptime"] = currentMillis / 1000;
    heartbeatDoc["ip"] = WiFi.localIP().toString();
    heartbeatDoc["rssi"] = WiFi.RSSI();
    heartbeatDoc["timestamp"] = getFormattedTime();

    char heartbeatBuffer[256];
    serializeJson(heartbeatDoc, heartbeatBuffer);
    client.publish(MQTT_HEARTBEAT_TOPIC, heartbeatBuffer);
    //Logger.println("Heartbeat published");
  }

  if (loraInterruptFlag) {
    loraInterruptFlag = false;

    int packet_len = radio.getPacketLength();
    
    if (packet_len != sizeof(Packet)) {
        Logger.printf("Received packet of unexpected size: %d bytes\n", packet_len);
        radio.startReceive();
        return;
    }

    Packet p;
    int state = radio.readData((uint8_t*)&p, sizeof(Packet));

    if (state == RADIOLIB_ERR_NONE) {
      if ((p.counter > last_seen_counter) || 
            ((p.counter == 0) && (last_seen_counter == 0))) {
        last_seen_counter = p.counter;
      } else {
        const uint16_t WRAP_WINDOW = 5000;
        bool is_potential_wrap = (last_seen_counter > (65535 - WRAP_WINDOW)) && (p.counter < WRAP_WINDOW);

        if (is_potential_wrap) {
          Logger.println("Counter wrap-around detected.");
          last_seen_counter = p.counter;
        } else {
          const uint16_t REBOOT_THRESHOLD = 2000;
          bool is_potential_reboot = (last_seen_counter - p.counter) > REBOOT_THRESHOLD;

          if (is_potential_reboot) {
            Logger.println("Probable sender reboot detected. Accepting new counter.");
            last_seen_counter = p.counter;
          } else {
            Logger.printf("Replay attack detected! Received: %d, Last seen: %d\n", p.counter, last_seen_counter);
            radio.startReceive();
            return;
          }
        }
      }

      uint64_t calculated_mic_64 = 0;
      siphash((uint8_t*)&p, 6, SECRET_KEY, (uint8_t*)&calculated_mic_64, sizeof(calculated_mic_64));
      uint32_t calculated_mic_32 = (uint32_t)calculated_mic_64;
      
      if (p.mic != calculated_mic_32) {
          Logger.printf("MIC mismatch!  Received: 0x%08X, Calculated: 0x%08X\n", p.mic, calculated_mic_32);
          radio.startReceive();
          return;
      }

      Logger.println("Received valid packet!");

      float temp_c = p.temp / 100.0;
      Logger.printf("  Voltage: %d mV\n", p.volts);
      Logger.printf("  Temp:    %.2f C\n", temp_c);
      Logger.printf("  Counter: %d\n", p.counter);
      Logger.printf("  RSSI:    %.2f dBm\n", radio.getRSSI());
      Logger.printf("  SNR:     %.2f dB\n", radio.getSNR());

      StaticJsonDocument<256> doc;
      doc["volts"] = p.volts;
      doc["temp"] = temp_c;
      doc["counter"] = p.counter;
      doc["rssi"] = radio.getRSSI();
      doc["snr"] = radio.getSNR();

      char jsonBuffer[256];
      serializeJson(doc, jsonBuffer);

      Logger.print("Publishing to MQTT: ");
      Logger.println(jsonBuffer);

      if (!client.publish(MQTT_TOPIC, jsonBuffer)) {
        Logger.println("Failed to publish message");
      }

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      Logger.println(F("CRC error!"));
    } else {
      Logger.print(F("Failed to read packet, code "));
      Logger.println(state);
    }

    radio.startReceive();
  }
}
