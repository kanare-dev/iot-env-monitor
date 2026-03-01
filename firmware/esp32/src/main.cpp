// ESP32 + BME280 Environment Monitor
// Phase 3: Wi-Fi + Prometheus HTTP exporter

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_BME280.h>
#include "config.h"

#define SDA_PIN 21
#define SCL_PIN 22
#define BME280_ADDR 0x76
#define HTTP_PORT 80
#define READ_INTERVAL_MS 5000

Adafruit_BME280 bme;
WebServer server(HTTP_PORT);

float temperature = 0;
float humidity = 0;
float pressure = 0;
bool hasValidReading = false;
unsigned long lastReadMs = 0;
unsigned long badReadCount = 0;

// BME280 spec range
#define TEMP_MIN -40.0f
#define TEMP_MAX  85.0f
#define HUM_MIN    0.0f
#define HUM_MAX  100.0f
#define PRES_MIN 300.0f
#define PRES_MAX 1100.0f

// Max change per reading interval (5s) — rejects transient spikes
#define TEMP_MAX_DELTA  5.0f
#define HUM_MAX_DELTA  10.0f
#define PRES_MAX_DELTA 20.0f

void scanI2C() {
  Serial.println("Scanning I2C bus...");
  int found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Found device at 0x%02X\n", addr);
      found++;
    }
  }
  Serial.printf("Scan complete: %d device(s) found\n\n", found);
}

bool initBME280() {
  Wire.end();
  delay(100);
  Wire.begin(SDA_PIN, SCL_PIN);

  for (int attempt = 1; attempt <= 3; attempt++) {
    Serial.printf("BME280 init attempt %d/3...\n", attempt);
    if (bme.begin(BME280_ADDR, &Wire)) return true;
    delay(500);
  }
  return false;
}

void connectWiFi() {
  IPAddress ip(STATIC_IP);
  IPAddress gateway(GATEWAY);
  IPAddress subnet(SUBNET);
  IPAddress dns(DNS_PRIMARY);

  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet, dns);
  Serial.printf("Connecting to %s (static IP: %s)", WIFI_SSID, ip.toString().c_str());
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++retries > 40) {
      Serial.println("\nERROR: Wi-Fi connection failed after 20s");
      Serial.println("Check SSID/password in config.h and restart.");
      while (true) {
        Serial.println("[waiting] Wi-Fi not connected - press EN to retry");
        delay(5000);
      }
    }
  }

  Serial.printf("\nConnected! IP: %s\n\n", WiFi.localIP().toString().c_str());
}

bool isValidReading(float t, float h, float p) {
  if (isnan(t) || isnan(h) || isnan(p)) return false;
  if (t < TEMP_MIN || t > TEMP_MAX) return false;
  if (h < HUM_MIN  || h > HUM_MAX)  return false;
  if (p < PRES_MIN || p > PRES_MAX) return false;

  if (hasValidReading) {
    if (fabsf(t - temperature) > TEMP_MAX_DELTA) return false;
    if (fabsf(h - humidity)    > HUM_MAX_DELTA)  return false;
    if (fabsf(p - pressure)    > PRES_MAX_DELTA) return false;
  }

  return true;
}

void readSensor() {
  float t = bme.readTemperature();
  float h = bme.readHumidity();
  float p = bme.readPressure() / 100.0F;

  if (isValidReading(t, h, p)) {
    temperature = t;
    humidity = h;
    pressure = p;
    hasValidReading = true;
    badReadCount = 0;
  } else {
    badReadCount++;
    Serial.printf("[WARN] Bad reading discarded (t=%.1f h=%.1f p=%.1f) count=%lu\n",
                  t, h, p, badReadCount);
  }
}

void handleMetrics() {
  readSensor();

  String body;
  body += "# HELP env_temperature_celsius Temperature in Celsius\n";
  body += "# TYPE env_temperature_celsius gauge\n";
  body += "env_temperature_celsius " + String(temperature, 2) + "\n";
  body += "# HELP env_humidity_percent Relative humidity in percent\n";
  body += "# TYPE env_humidity_percent gauge\n";
  body += "env_humidity_percent " + String(humidity, 2) + "\n";
  body += "# HELP env_pressure_hpa Atmospheric pressure in hPa\n";
  body += "# TYPE env_pressure_hpa gauge\n";
  body += "env_pressure_hpa " + String(pressure, 2) + "\n";
  body += "# HELP env_bad_reads_total Discarded invalid sensor readings\n";
  body += "# TYPE env_bad_reads_total counter\n";
  body += "env_bad_reads_total " + String(badReadCount) + "\n";

  server.send(200, "text/plain; charset=utf-8", body);
  Serial.println("[HTTP] GET /metrics");
}

void handleRoot() {
  readSensor();

  String html = "<html><head><title>ESP32 Env Monitor</title>"
    "<meta http-equiv='refresh' content='5'>"
    "<style>body{font-family:monospace;padding:2em}</style></head><body>"
    "<h2>ESP32 Environment Monitor</h2>"
    "<p>Temperature: " + String(temperature, 1) + " C</p>"
    "<p>Humidity: " + String(humidity, 1) + " %</p>"
    "<p>Pressure: " + String(pressure, 1) + " hPa</p>"
    "<p><a href='/metrics'>/metrics</a> (Prometheus)</p>"
    "</body></html>";

  server.send(200, "text/html", html);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found\n");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== IoT Environment Monitor ===\n");

  Wire.begin(SDA_PIN, SCL_PIN);
  scanI2C();

  if (!initBME280()) {
    Serial.println("ERROR: BME280 init failed after 3 attempts");
    Serial.println("Check wiring and press EN to restart.");
    while (true) {
      Serial.println("[waiting] BME280 not ready - press EN to retry");
      delay(5000);
    }
  }
  Serial.println("BME280 initialized successfully.\n");

  connectWiFi();

  server.on("/", handleRoot);
  server.on("/metrics", handleMetrics);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.printf("HTTP server started on port %d\n", HTTP_PORT);
  Serial.printf("  http://%s/\n", WiFi.localIP().toString().c_str());
  Serial.printf("  http://%s/metrics\n\n", WiFi.localIP().toString().c_str());
}

void loop() {
  server.handleClient();

  if (millis() - lastReadMs >= READ_INTERVAL_MS) {
    lastReadMs = millis();
    readSensor();
    Serial.printf("Temperature: %.1f C | Humidity: %.1f %% | Pressure: %.1f hPa\n",
                  temperature, humidity, pressure);
  }
}
