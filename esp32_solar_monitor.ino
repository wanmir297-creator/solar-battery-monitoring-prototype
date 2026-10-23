/*
  Solar Battery Monitoring & Automatic Protection — ESP32 starter firmware
  Assumptions: 12 V lead-acid prototype; verify all thresholds and calibrations.
  Sensors: voltage divider on GPIO34, DS18B20 on GPIO4, optional ACS712 on GPIO35.
  Relay: GPIO26; assumes HIGH energizes relay and NO contact feeds the load.
*/

#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WebServer.h>

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

constexpr int PIN_BATTERY_VOLTAGE = 34; // ADC input only
constexpr int PIN_CURRENT_SENSOR  = 35; // optional ACS712; ADC input only
constexpr int PIN_ONEWIRE         = 4;
constexpr int PIN_LOAD_RELAY      = 26;

// Set false if your relay module is active-LOW. Verify on the bench first.
constexpr bool RELAY_ACTIVE_HIGH = true;
constexpr bool USE_CURRENT_SENSOR = false; // enable only after calibration

// Example thresholds for a 12 V lead-acid demo; follow your battery datasheet.
constexpr float LOW_CUTOFF_V = 11.8;
constexpr float RECONNECT_V  = 12.6;
constexpr float TEMP_CUTOFF_C = 55.0;
constexpr float TEMP_RECONNECT_C = 45.0;

// Divider: 100k from battery+ to ADC, 27k from ADC to GND.
constexpr float DIVIDER_RATIO = (100.0 + 27.0) / 27.0;
// Calibrate this using a multimeter; ESP32 ADC is not precision instrumentation.
constexpr float ADC_REFERENCE_V = 3.3;
constexpr int ADC_MAX_COUNTS = 4095;

// ACS712 calibration depends on variant (5A/20A/30A) and supply.
// Set sensitivity and zero-current voltage for the exact module before enabling.
constexpr float ACS_ZERO_V = 2.50;
constexpr float ACS_SENSITIVITY_V_PER_A = 0.100; // example: ACS712 20A; verify!

OneWire oneWire(PIN_ONEWIRE);
DallasTemperature tempSensor(&oneWire);
WebServer server(80);

bool loadEnabled = false;
bool lowBatteryFault = false;
bool highTempFault = false;
float batteryVoltage = 0.0;
float batteryCurrent = 0.0;
float batteryTempC = NAN;
unsigned long lastSampleMs = 0;

void setRelay(bool enableLoad) {
  loadEnabled = enableLoad;
  bool outputHigh = enableLoad ? RELAY_ACTIVE_HIGH : !RELAY_ACTIVE_HIGH;
  digitalWrite(PIN_LOAD_RELAY, outputHigh ? HIGH : LOW);
}

float readBatteryVoltage() {
  // Average a few samples to reduce noise; calibrate against a multimeter.
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(PIN_BATTERY_VOLTAGE);
    delay(2);
  }
  float adcCounts = sum / 16.0;
  float adcVolts = (adcCounts / ADC_MAX_COUNTS) * ADC_REFERENCE_V;
  return adcVolts * DIVIDER_RATIO;
}

float readCurrentAmps() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(PIN_CURRENT_SENSOR);
    delay(2);
  }
  float adcVolts = ((sum / 16.0) / ADC_MAX_COUNTS) * ADC_REFERENCE_V;
  return (adcVolts - ACS_ZERO_V) / ACS_SENSITIVITY_V_PER_A;
}

void updateProtection() {
  // Latch faults until reconnect conditions are met to prevent relay chatter.
  if (batteryVoltage <= LOW_CUTOFF_V) lowBatteryFault = true;
  if (!isnan(batteryTempC) && batteryTempC >= TEMP_CUTOFF_C) highTempFault = true;

  if (lowBatteryFault && batteryVoltage >= RECONNECT_V) lowBatteryFault = false;
  if (highTempFault && !isnan(batteryTempC) && batteryTempC <= TEMP_RECONNECT_C) {
    highTempFault = false;
  }

  bool safeToRun = !lowBatteryFault && !highTempFault && !isnan(batteryTempC);
  setRelay(safeToRun);
}

String statusJson() {
  String status = "NORMAL";
  if (lowBatteryFault && highTempFault) status = "LOW_BATTERY_AND_HIGH_TEMP";
  else if (lowBatteryFault) status = "LOW_BATTERY_CUTOFF";
  else if (highTempFault) status = "HIGH_TEMP_CUTOFF";
  else if (isnan(batteryTempC)) status = "TEMP_SENSOR_ERROR";
  String json = "{";
  json += "\"voltage_v\":" + String(batteryVoltage, 2) + ",";
  json += "\"current_a\":" + String(batteryCurrent, 2) + ",";
  json += "\"temperature_c\":" + String(batteryTempC, 1) + ",";
  json += "\"load_enabled\":" + String(loadEnabled ? "true" : "false") + ",";
  json += "\"status\":\"" + status + "\"";
  json += "}";
  return json;
}

void handleRoot() {
  server.send(200, "text/plain",
    "Solar Battery Monitor ESP32 API\nGET /api/status\n"
    "Connect a dashboard to this endpoint on the same network.");
}

void handleStatus() {
  server.send(200, "application/json", statusJson());
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LOAD_RELAY, OUTPUT);

  // Fail-safe startup: de-energize relay so a NO-wired load starts disconnected.
  digitalWrite(PIN_LOAD_RELAY, RELAY_ACTIVE_HIGH ? LOW : HIGH);
  loadEnabled = false;

  analogReadResolution(12);
  tempSensor.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi unavailable; local protection still runs.");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.begin();
}

void loop() {
  server.handleClient();

  if (millis() - lastSampleMs >= 1000) {
    lastSampleMs = millis();

    batteryVoltage = readBatteryVoltage();
    tempSensor.requestTemperatures();
    batteryTempC = tempSensor.getTempCByIndex(0);

    if (USE_CURRENT_SENSOR) batteryCurrent = readCurrentAmps();
    else batteryCurrent = 0.0; // dashboard/API marks this as unavailable by project documentation

    // If the DS18B20 is disconnected, its library may return -127 °C.
    if (batteryTempC < -100.0 || batteryTempC > 125.0) batteryTempC = NAN;

    updateProtection();

    Serial.println(statusJson());
  }
}
