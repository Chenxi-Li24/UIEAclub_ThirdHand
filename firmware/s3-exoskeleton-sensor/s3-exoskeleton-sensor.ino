/*
  ThirdHand six-axis exoskeleton sensor

  Six GT magnetic angle sensors:
    OUT -> GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO6
    VCC -> 3V3, GND -> common GND

  UDP packet sent to the P4 controller:
    EXO:sequence,a1,a2,a3,a4,a5,a6,mv1,mv2,mv3,mv4,mv5,mv6
*/

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "sensor_config.h"

static const uint8_t SENSOR_PINS[SENSOR_COUNT] = {1, 2, 3, 4, 5, 6};

static Preferences s_preferences;
static WiFiUDP s_udp;
static IPAddress s_p4Address;
static IPAddress s_webProxyAddress;
static float s_zeroDeg[SENSOR_COUNT];
static int8_t s_direction[SENSOR_COUNT];
static uint16_t s_millivolts[SENSOR_COUNT];
static float s_rawDeg[SENSOR_COUNT];
static float s_jointDeg[SENSOR_COUNT];
static float s_filteredSensorDeg[SENSOR_COUNT];
static float s_unwrappedSensorDeg[SENSOR_COUNT];
static float s_filteredDerivative[SENSOR_COUNT];
static float s_lastMeasuredDeg[SENSOR_COUNT];
static bool s_filterInitialized[SENSOR_COUNT];
static uint32_t s_sequence = 0;
static uint32_t s_lastSendMs = 0;
static uint32_t s_lastSerialPrintMs = 0;
static uint32_t s_lastWifiAttemptMs = 0;
static uint32_t s_lastP4AckMs = 0;
static uint32_t s_lastWebAckMs = 0;
static bool s_udpBound = false;
static bool s_p4ControlEnabled = false;
static bool s_wifiWasConnected = false;

static void ledSet(uint8_t red, uint8_t green, uint8_t blue) {
  red = static_cast<uint8_t>(red * RGB_LED_BRIGHTNESS / 255);
  green = static_cast<uint8_t>(green * RGB_LED_BRIGHTNESS / 255);
  blue = static_cast<uint8_t>(blue * RGB_LED_BRIGHTNESS / 255);
  rgbLedWrite(RGB_LED_PIN, red, green, blue);
}

static void ledTick() {
  const bool blink = (millis() / 400) % 2;
  if (WiFi.status() != WL_CONNECTED) {
    ledSet(blink ? 255 : 0, blink ? 120 : 0, 0);
  } else if (s_lastWebAckMs == 0 || millis() - s_lastWebAckMs > WEB_ACK_TIMEOUT_MS) {
    ledSet(0, 0, blink ? 255 : 0);
  } else if (s_p4ControlEnabled) {
    ledSet(180, 0, 255);
  } else if (s_lastP4AckMs != 0 && millis() - s_lastP4AckMs <= P4_ACK_TIMEOUT_MS) {
    ledSet(0, 255, 255);
  } else {
    ledSet(0, 255, 0);
  }
}

static float wrapSignedDegrees(float degrees) {
  while (degrees > 180.0f) degrees -= 360.0f;
  while (degrees <= -180.0f) degrees += 360.0f;
  return degrees;
}

static float wrapUnsignedDegrees(float degrees) {
  while (degrees >= 360.0f) degrees -= 360.0f;
  while (degrees < 0.0f) degrees += 360.0f;
  return degrees;
}

static float lowPassAlpha(float cutoffHz, float dtSeconds) {
  const float tau = 1.0f / (2.0f * PI * cutoffHz);
  return dtSeconds / (dtSeconds + tau);
}

static float filterSensorAngle(int index, float measuredDeg) {
  if (!s_filterInitialized[index]) {
    s_filterInitialized[index] = true;
    s_filteredSensorDeg[index] = measuredDeg;
    s_unwrappedSensorDeg[index] = measuredDeg;
    s_lastMeasuredDeg[index] = measuredDeg;
    s_filteredDerivative[index] = 0.0f;
    return measuredDeg;
  }

  const float dt = SEND_INTERVAL_MS / 1000.0f;
  const float measurementDelta = wrapSignedDegrees(measuredDeg - s_lastMeasuredDeg[index]);
  s_lastMeasuredDeg[index] = measuredDeg;
  s_unwrappedSensorDeg[index] += measurementDelta;

  const float derivative = measurementDelta / dt;
  const float derivativeAlpha = lowPassAlpha(FILTER_DERIVATIVE_CUTOFF_HZ, dt);
  s_filteredDerivative[index] +=
      derivativeAlpha * (derivative - s_filteredDerivative[index]);

  const float cutoff = FILTER_MIN_CUTOFF_HZ +
                       FILTER_BETA * fabsf(s_filteredDerivative[index]);
  const float signalAlpha = lowPassAlpha(cutoff, dt);
  s_filteredSensorDeg[index] +=
      signalAlpha * (s_unwrappedSensorDeg[index] - s_filteredSensorDeg[index]);
  return wrapUnsignedDegrees(s_filteredSensorDeg[index]);
}

static uint16_t readAverageMilliVolts(uint8_t pin) {
  uint32_t total = 0;
  for (int sample = 0; sample < ADC_SAMPLE_COUNT; ++sample) {
    total += analogReadMilliVolts(pin);
    delayMicroseconds(150);
  }
  return static_cast<uint16_t>(total / ADC_SAMPLE_COUNT);
}

static void sampleSensors() {
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    s_millivolts[i] = readAverageMilliVolts(SENSOR_PINS[i]);
    const float measuredDeg =
        constrain(s_millivolts[i] / ADC_FULL_SCALE_MV * 360.0f, 0.0f, 360.0f);
    s_rawDeg[i] = filterSensorAngle(i, measuredDeg);
    s_jointDeg[i] =
        wrapSignedDegrees(s_direction[i] * (s_rawDeg[i] - s_zeroDeg[i]));
  }
}

static void loadCalibration() {
  s_preferences.begin("exo-cal", false);
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    char zeroKey[4];
    char directionKey[4];
    snprintf(zeroKey, sizeof(zeroKey), "z%d", i);
    snprintf(directionKey, sizeof(directionKey), "d%d", i);
    s_zeroDeg[i] = s_preferences.getFloat(zeroKey, DEFAULT_ZERO_DEG[i]);
    s_direction[i] = s_preferences.getChar(directionKey, DEFAULT_DIRECTION[i]);
    if (s_direction[i] != 1 && s_direction[i] != -1) s_direction[i] = 1;
  }
}

static void saveChannelCalibration(int index) {
  char zeroKey[4];
  char directionKey[4];
  snprintf(zeroKey, sizeof(zeroKey), "z%d", index);
  snprintf(directionKey, sizeof(directionKey), "d%d", index);
  s_preferences.putFloat(zeroKey, s_zeroDeg[index]);
  s_preferences.putChar(directionKey, s_direction[index]);
}

static void printCalibration() {
  Serial.println("channel,pin,zero_deg,direction,raw_deg,joint_deg,mV");
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    Serial.printf("%d,%d,%.2f,%s,%.2f,%.2f,%u\n", i + 1, SENSOR_PINS[i],
                  s_zeroDeg[i], s_direction[i] > 0 ? "ccw+" : "cw+",
                  s_rawDeg[i], s_jointDeg[i], s_millivolts[i]);
  }
}

static void printHelp() {
  Serial.println("Commands:");
  Serial.println("  cal show");
  Serial.println("  cal zero <1-6>            capture current position as 0 deg");
  Serial.println("  cal zero <1-6> <0-360>    set sensor-domain zero angle");
  Serial.println("  cal dir <1-6> ccw|cw      select positive rotation direction");
  Serial.println("  cal reset                 restore all defaults");
  Serial.println("  net show                  show WiFi and P4 link state");
}

static void processSerialCommand(String line) {
  line.trim();
  if (line == "help") {
    printHelp();
    return;
  }
  if (line == "cal show") {
    printCalibration();
    return;
  }
  if (line == "net show") {
    Serial.printf("WiFi:%s IP:%s RSSI:%d UDP:%s WEB_ACK_AGE:%lu P4_ACK_AGE:%lu control:%s\n",
                  WiFi.status() == WL_CONNECTED ? "OK" : "OFFLINE",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                  s_udpBound ? "OK" : "OFFLINE",
                  s_lastWebAckMs == 0 ? ULONG_MAX : millis() - s_lastWebAckMs,
                  s_lastP4AckMs == 0 ? ULONG_MAX : millis() - s_lastP4AckMs,
                  s_p4ControlEnabled ? "ON" : "OFF");
    return;
  }
  if (line == "cal reset") {
    for (int i = 0; i < SENSOR_COUNT; ++i) {
      s_zeroDeg[i] = DEFAULT_ZERO_DEG[i];
      s_direction[i] = DEFAULT_DIRECTION[i];
      saveChannelCalibration(i);
    }
    Serial.println("OK: calibration reset");
    return;
  }

  int channel = 0;
  float zero = 0.0f;
  char direction[8] = {0};
  if (sscanf(line.c_str(), "cal zero %d %f", &channel, &zero) == 2) {
    if (channel < 1 || channel > SENSOR_COUNT || zero < 0.0f || zero > 360.0f) {
      Serial.println("ERR: channel 1-6, zero 0-360");
      return;
    }
    s_zeroDeg[channel - 1] = zero;
    saveChannelCalibration(channel - 1);
    Serial.printf("OK: channel %d zero=%.2f deg\n", channel, zero);
    return;
  }
  if (sscanf(line.c_str(), "cal zero %d", &channel) == 1) {
    if (channel < 1 || channel > SENSOR_COUNT) {
      Serial.println("ERR: channel must be 1-6");
      return;
    }
    sampleSensors();
    s_zeroDeg[channel - 1] = s_rawDeg[channel - 1];
    saveChannelCalibration(channel - 1);
    Serial.printf("OK: channel %d captured zero=%.2f deg\n", channel, s_zeroDeg[channel - 1]);
    return;
  }
  if (sscanf(line.c_str(), "cal dir %d %7s", &channel, direction) == 2) {
    if (channel < 1 || channel > SENSOR_COUNT ||
        (strcmp(direction, "ccw") != 0 && strcmp(direction, "cw") != 0)) {
      Serial.println("ERR: use cal dir <1-6> ccw|cw");
      return;
    }
    s_direction[channel - 1] = strcmp(direction, "ccw") == 0 ? 1 : -1;
    saveChannelCalibration(channel - 1);
    Serial.printf("OK: channel %d positive direction=%s\n", channel, direction);
    return;
  }
  Serial.println("ERR: unknown command; type help");
}

static void connectWifi() {
  WiFi.mode(WIFI_STA);
#if USE_STATIC_IP
  IPAddress local;
  IPAddress gateway;
  IPAddress mask;
  local.fromString(SENSOR_STATIC_IP);
  gateway.fromString(NETWORK_GATEWAY);
  mask.fromString(NETWORK_MASK);
  WiFi.config(local, gateway, mask);
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  s_lastWifiAttemptMs = millis();
  Serial.printf("[WiFi] connecting to %s\n", WIFI_SSID);
}

static void wifiTick() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    if (!s_wifiWasConnected) {
      Serial.printf("[WiFi] connected IP=%s RSSI=%d\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      s_wifiWasConnected = true;
    }
    if (!s_udpBound) {
      s_udpBound = s_udp.begin(SENSOR_UDP_PORT) == 1;
      Serial.printf("[UDP] local port %d: %s\n", SENSOR_UDP_PORT, s_udpBound ? "OK" : "FAILED");
    }
    return;
  }

  if (s_wifiWasConnected) {
    Serial.println("[WiFi] disconnected");
    s_wifiWasConnected = false;
    s_lastP4AckMs = 0;
    s_lastWebAckMs = 0;
    if (s_udpBound) s_udp.stop();
    s_udpBound = false;
  }
  if (millis() - s_lastWifiAttemptMs >= 10000) {
    WiFi.disconnect();
    connectWifi();
  }
}

static void receiveLinkAcknowledgements() {
  if (!s_udpBound) return;
  for (int handled = 0; handled < 4; ++handled) {
    int packetSize = s_udp.parsePacket();
    if (packetSize <= 0) break;
    if (packetSize >= 64) {
      s_udp.flush();
      continue;
    }

    char reply[64] = {0};
    int length = s_udp.read(reinterpret_cast<uint8_t*>(reply), sizeof(reply) - 1);
    if (length <= 0) continue;

    unsigned long sequence = 0;
    int controlEnabled = 0;
    if (sscanf(reply, "EXO_ACK:%lu,%d", &sequence, &controlEnabled) == 2) {
      s_lastP4AckMs = millis();
      s_p4ControlEnabled = controlEnabled != 0;
    } else if (sscanf(reply, "WEB_ACK:%lu", &sequence) == 1) {
      s_lastWebAckMs = millis();
    }
  }
}

static void printSensorStatus(uint32_t sequence) {
  Serial.printf("[EXO] #%lu", static_cast<unsigned long>(sequence));
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    Serial.printf(" | J%d=%+.2fdeg (%umV)", i + 1, s_jointDeg[i], s_millivolts[i]);
  }
  Serial.println();
}

static void sendExoskeletonPacket() {
  char packet[256];
  const uint32_t sequence = s_sequence++;
  int used = snprintf(packet, sizeof(packet), "EXO:%lu", static_cast<unsigned long>(sequence));
  for (int i = 0; i < SENSOR_COUNT && used > 0 && used < static_cast<int>(sizeof(packet)); ++i) {
    used += snprintf(packet + used, sizeof(packet) - used, ",%.2f", s_jointDeg[i]);
  }
  for (int i = 0; i < SENSOR_COUNT && used > 0 && used < static_cast<int>(sizeof(packet)); ++i) {
    used += snprintf(packet + used, sizeof(packet) - used, ",%u", s_millivolts[i]);
  }

  if (s_udpBound && used > 0 && used < static_cast<int>(sizeof(packet))) {
    s_udp.beginPacket(s_p4Address, P4_COMMAND_PORT);
    s_udp.write(reinterpret_cast<const uint8_t*>(packet), strlen(packet));
    s_udp.endPacket();

    s_udp.beginPacket(s_webProxyAddress, WEB_TELEMETRY_PORT);
    s_udp.write(reinterpret_cast<const uint8_t*>(packet), strlen(packet));
    s_udp.endPacket();
  }

  if (millis() - s_lastSerialPrintMs >= SERIAL_PRINT_INTERVAL_MS) {
    s_lastSerialPrintMs = millis();
    printSensorStatus(sequence);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  ledSet(255, 0, 0);

  analogReadResolution(12);
  for (int i = 0; i < SENSOR_COUNT; ++i) {
    pinMode(SENSOR_PINS[i], INPUT);
    analogSetPinAttenuation(SENSOR_PINS[i], ADC_11db);
  }

  loadCalibration();
  s_p4Address.fromString(P4_CONTROLLER_IP);
  s_webProxyAddress.fromString(WEB_PROXY_IP);
  connectWifi();

  Serial.println("ThirdHand ESP32-S3 six-axis exoskeleton sensor");
  printHelp();
  sampleSensors();
  printCalibration();
}

void loop() {
  wifiTick();
  receiveLinkAcknowledgements();
  ledTick();

  while (Serial.available() > 0) {
    processSerialCommand(Serial.readStringUntil('\n'));
  }

  const uint32_t now = millis();
  if (now - s_lastSendMs >= SEND_INTERVAL_MS) {
    s_lastSendMs = now;
    sampleSensors();
    sendExoskeletonPacket();
  }
}
