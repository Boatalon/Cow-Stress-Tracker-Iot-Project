#include <Wire.h>
#include <MPU6050_light.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

#include "MotionTracker.h"
#include "WebDashboard.h"

// ================= ตั้งค่า WiFi AP สำหรับโหมดจูนค่า =================
#define CALIB_AP_SSID       "Cow-Calibrator"
#define CALIB_AP_PASS       "12345678"

// ================= ตั้งค่าขาฮาร์ดแวร์ (Seeed XIAO ESP32-C3) =================
#define ONE_WIRE_BUS        D3
#define I2C_SDA_PIN         D5
#define I2C_SCL_PIN         D4

// ================= อัตราการอ่านค่าเซ็นเซอร์ =================
#define SAMPLE_INTERVAL_MS  20      // 50 Hz (ทุก 20 ms)
#define TEMP_INTERVAL_MS    2000    // อ่านอุณหภูมิทุก 2 วินาที
#define CHART_BUFFER_SIZE   300     // เก็บข้อมูลกราฟ/CSV ย้อนหลัง

struct ChartSample {
  uint32_t id;
  uint32_t ms;
  float    ax;
  float    ay;
  float    az;
  float    dynMag;
  float    tiltDeg;
  uint8_t  stand;
  uint8_t  stepFlag;
};

ChartSample chartBuf[CHART_BUFFER_SIZE];
uint32_t    nextSampleId = 1;
uint16_t    bufWriteIdx  = 0;
uint16_t    bufCount     = 0;

MPU6050           mpu(Wire);
OneWire           oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Preferences       prefs;
WebServer         webServer(80);
DNSServer         dnsServer;

MotionParams      motionParams;
MotionState       motionState;
float             currentTempC    = 0.0f;
unsigned long     lastSampleMs    = 0;
unsigned long     lastTempMs      = 0;
uint8_t           subSampleToggle = 0;
bool              pendingStepFlag = false;

// ================================================================
// โหลด / บันทึกพารามิเตอร์ลง Flash (Preferences)
// ================================================================
void loadCalibParams() {
  defaultMotionParams(motionParams);
  prefs.begin("cal-cfg", true);
  motionParams.stepTriggerG       = prefs.getFloat("trig", motionParams.stepTriggerG);
  motionParams.stepResetG         = prefs.getFloat("rst",  motionParams.stepResetG);
  motionParams.stepDelayMs        = prefs.getULong("dly",  motionParams.stepDelayMs);
  motionParams.smoothAlpha        = prefs.getFloat("alp",  motionParams.smoothAlpha);
  motionParams.lieAngleDeg        = prefs.getFloat("lie",  motionParams.lieAngleDeg);
  motionParams.standAngleDeg      = prefs.getFloat("std",  motionParams.standAngleDeg);
  motionParams.standToLieDelaySec = prefs.getULong("t2l",  motionParams.standToLieDelaySec);
  motionParams.lieToStandDelaySec = prefs.getULong("t2s",  motionParams.lieToStandDelaySec);
  motionParams.refGravX           = prefs.getFloat("rx",   motionParams.refGravX);
  motionParams.refGravY           = prefs.getFloat("ry",   motionParams.refGravY);
  motionParams.refGravZ           = prefs.getFloat("rz",   motionParams.refGravZ);
  prefs.end();
}

void saveCalibParams() {
  prefs.begin("cal-cfg", false);
  prefs.putFloat("trig", motionParams.stepTriggerG);
  prefs.putFloat("rst",  motionParams.stepResetG);
  prefs.putULong("dly",  motionParams.stepDelayMs);
  prefs.putFloat("alp",  motionParams.smoothAlpha);
  prefs.putFloat("lie",  motionParams.lieAngleDeg);
  prefs.putFloat("std",  motionParams.standAngleDeg);
  prefs.putULong("t2l",  motionParams.standToLieDelaySec);
  prefs.putULong("t2s",  motionParams.lieToStandDelaySec);
  prefs.putFloat("rx",   motionParams.refGravX);
  prefs.putFloat("ry",   motionParams.refGravY);
  prefs.putFloat("rz",   motionParams.refGravZ);
  prefs.end();
}

void pushSample(unsigned long nowMs, float ax, float ay, float az, bool stepFlag) {
  ChartSample &s = chartBuf[bufWriteIdx];
  s.id       = nextSampleId++;
  s.ms       = nowMs;
  s.ax       = ax;
  s.ay       = ay;
  s.az       = az;
  s.dynMag   = motionState.dynMag;
  s.tiltDeg  = motionState.tiltDeg;
  s.stand    = motionState.isStanding ? 1 : 0;
  s.stepFlag = stepFlag ? 1 : 0;

  bufWriteIdx = (bufWriteIdx + 1) % CHART_BUFFER_SIZE;
  if (bufCount < CHART_BUFFER_SIZE) bufCount++;
}

// ================================================================
// HTTP Routes
// ================================================================
void handleRoot() {
  webServer.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handlePoll() {
  uint32_t sinceId = 0;
  if (webServer.hasArg("since")) {
    sinceId = (uint32_t)webServer.arg("since").toInt();
  }

  String json = "{";
  json += "\"stand\":" + String(motionState.isStanding ? 1 : 0) + ",";
  json += "\"tilt\":" + String(motionState.tiltDeg, 1) + ",";
  json += "\"dyn\":" + String(motionState.dynMag, 3) + ",";
  json += "\"temp\":" + String(currentTempC, 1) + ",";
  json += "\"steps\":" + String(motionState.stepCount) + ",";
  json += "\"lsteps\":" + String(motionState.legacyStepCount) + ",";
  json += "\"msteps\":" + String(motionState.manualStepCount) + ",";
  json += "\"scnt\":" + String(motionState.standCount) + ",";
  json += "\"lcnt\":" + String(motionState.lieCount) + ",";
  json += "\"sms\":" + String(motionState.standDurationMs) + ",";
  json += "\"lms\":" + String(motionState.lieDurationMs) + ",";
  json += "\"rem\":" + String(motionState.transitionRemainSec, 1) + ",";

  json += "\"p\":[";
  json += String(motionParams.stepTriggerG, 2) + ",";
  json += String(motionParams.stepResetG, 2) + ",";
  json += String(motionParams.stepDelayMs) + ",";
  json += String(motionParams.smoothAlpha, 2) + ",";
  json += String(motionParams.lieAngleDeg, 1) + ",";
  json += String(motionParams.standAngleDeg, 1) + ",";
  json += String(motionParams.standToLieDelaySec) + ",";
  json += String(motionParams.lieToStandDelaySec) + ",";
  json += String(motionParams.refGravX, 3) + ",";
  json += String(motionParams.refGravY, 3) + ",";
  json += String(motionParams.refGravZ, 3) + "],";

  json += "\"s\":[";
  uint16_t startOffset = (bufWriteIdx + CHART_BUFFER_SIZE - bufCount) % CHART_BUFFER_SIZE;
  bool first = true;
  int sent = 0;
  for (uint16_t i = 0; i < bufCount; i++) {
    uint16_t idx = (startOffset + i) % CHART_BUFFER_SIZE;
    const ChartSample &cs = chartBuf[idx];
    if (cs.id > sinceId && (bufCount - i) <= 40) {
      if (!first) json += ",";
      first = false;
      json += "[" + String(cs.id) + "," +
              String(cs.dynMag, 3) + "," +
              String(cs.tiltDeg, 1) + "," +
              String(cs.stepFlag) + "]";
      if (++sent >= 25) break;
    }
  }
  json += "]}";

  webServer.send(200, "application/json", json);
}

void handleUpdateParams() {
  if (webServer.hasArg("trig")) motionParams.stepTriggerG       = webServer.arg("trig").toFloat();
  if (webServer.hasArg("rst"))  motionParams.stepResetG         = webServer.arg("rst").toFloat();
  if (webServer.hasArg("dly"))  motionParams.stepDelayMs        = (unsigned long)webServer.arg("dly").toInt();
  if (webServer.hasArg("alp"))  motionParams.smoothAlpha        = webServer.arg("alp").toFloat();
  if (webServer.hasArg("lie"))  motionParams.lieAngleDeg        = webServer.arg("lie").toFloat();
  if (webServer.hasArg("std"))  motionParams.standAngleDeg      = webServer.arg("std").toFloat();
  if (webServer.hasArg("t2l"))  motionParams.standToLieDelaySec = (unsigned long)webServer.arg("t2l").toInt();
  if (webServer.hasArg("t2s"))  motionParams.lieToStandDelaySec = (unsigned long)webServer.arg("t2s").toInt();

  if (motionParams.stepResetG >= motionParams.stepTriggerG) {
    motionParams.stepResetG = max(0.05f, motionParams.stepTriggerG - 0.05f);
  }
  if (motionParams.standAngleDeg >= motionParams.lieAngleDeg) {
    motionParams.standAngleDeg = max(10.0f, motionParams.lieAngleDeg - 5.0f);
  }

  saveCalibParams();
  webServer.send(200, "application/json", "{\"ok\":true}");
}

void handleCalibrateStand() {
  motionCaptureStandingRef(motionState, motionParams);
  saveCalibParams();

  String res = "{\"rx\":" + String(motionParams.refGravX, 3) +
               ",\"ry\":" + String(motionParams.refGravY, 3) +
               ",\"rz\":" + String(motionParams.refGravZ, 3) + "}";
  webServer.send(200, "application/json", res);
}

void handleResetCounters() {
  motionResetCounters(motionState, millis());
  webServer.send(200, "application/json", "{\"ok\":true}");
}

void handleManualStep() {
  motionState.manualStepCount++;
  webServer.send(200, "application/json", "{\"ok\":true}");
}

void handleCsvDownload() {
  String csv = "id,time_ms,ax_g,ay_g,az_g,dyn_mag_g,tilt_deg,stand,step_detected\n";
  uint16_t startOffset = (bufWriteIdx + CHART_BUFFER_SIZE - bufCount) % CHART_BUFFER_SIZE;
  for (uint16_t i = 0; i < bufCount; i++) {
    uint16_t idx = (startOffset + i) % CHART_BUFFER_SIZE;
    const ChartSample &cs = chartBuf[idx];
    csv += String(cs.id) + "," +
           String(cs.ms) + "," +
           String(cs.ax, 3) + "," +
           String(cs.ay, 3) + "," +
           String(cs.az, 3) + "," +
           String(cs.dynMag, 3) + "," +
           String(cs.tiltDeg, 1) + "," +
           String(cs.stand) + "," +
           String(cs.stepFlag) + "\n";
  }
  webServer.sendHeader("Content-Disposition", "attachment; filename=cow_sensor_log.csv");
  webServer.send(200, "text/csv", csv);
}

// ================================================================
// Setup & Loop
// ================================================================
void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000);

  Serial.println("\n==========================================");
  Serial.println("🐄 Cow-Stress-Calibrator Starting...");
  Serial.println("==========================================");

  loadCalibParams();

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  byte status = mpu.begin();
  Serial.print("MPU6050 status: ");
  Serial.println(status);
  if (status != 0) {
    Serial.println("❌ ไม่พบ MPU6050 — ตรวจสอบสาย SDA (D5) / SCL (D4)");
  } else {
    Serial.println("Calibrating MPU6050 gyro...");
    delay(500);
    mpu.calcOffsets(true, false); // Calibrate เฉพาะ Gyro เพื่อเก็บค่าแรงโน้มถ่วงจริงบนแกน Accel
  }

  sensors.begin();
  sensors.setWaitForConversion(false); // อ่านอุณหภูมิแบบไม่บล็อก loop 50Hz
  sensors.requestTemperatures();

  mpu.update();
  motionInit(motionState, motionParams,
             mpu.getAccX(), mpu.getAccY(), mpu.getAccZ(), millis());

  // เปิด SoftAP ให้มือถือเกาะโดยตรง
  WiFi.mode(WIFI_AP);
  WiFi.softAP(CALIB_AP_SSID, CALIB_AP_PASS);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(53, "*", apIP);

  webServer.on("/", handleRoot);
  webServer.on("/api/poll", HTTP_GET, handlePoll);
  webServer.on("/api/params", HTTP_POST, handleUpdateParams);
  webServer.on("/api/calibrate", HTTP_POST, handleCalibrateStand);
  webServer.on("/api/reset", HTTP_POST, handleResetCounters);
  webServer.on("/api/manual_step", HTTP_POST, handleManualStep);
  webServer.on("/api/csv", HTTP_GET, handleCsvDownload);
  webServer.onNotFound(handleRoot);
  webServer.begin();

  Serial.println("✅ เปิด WiFi AP สำเร็จ!");
  Serial.print("📶 ชื่อ WiFi: "); Serial.println(CALIB_AP_SSID);
  Serial.print("🔑 รหัสผ่าน: "); Serial.println(CALIB_AP_PASS);
  Serial.print("🌐 เปิดเว็บที่: http://"); Serial.println(apIP);
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  unsigned long nowMs = millis();

  // อ่านค่า MPU6050 ที่ความถี่ 50 Hz (ทุก 20 ms)
  if (nowMs - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = nowMs;
    mpu.update();

    float ax = mpu.getAccX();
    float ay = mpu.getAccY();
    float az = mpu.getAccZ();

    bool stepNow = motionUpdate(motionState, motionParams, ax, ay, az, nowMs);
    if (stepNow) pendingStepFlag = true;

    // บันทึกลงกราฟที่ 25 Hz (ทุก 40 ms) เพื่อให้แสดงย้อนหลัง 10 วินาทีได้ลื่นไหล
    subSampleToggle ^= 1;
    if (subSampleToggle == 0) {
      pushSample(nowMs, ax, ay, az, pendingStepFlag);
      pendingStepFlag = false;
    }
  }

  // อ่านค่าอุณหภูมิ DS18B20 ทุก 2 วินาทีแบบไม่บล็อกการนับก้าว
  if (nowMs - lastTempMs >= TEMP_INTERVAL_MS) {
    lastTempMs = nowMs;
    float t = sensors.getTempCByIndex(0);
    if (t != DEVICE_DISCONNECTED_C && t > -40.0f && t < 85.0f) {
      currentTempC = t;
    }
    sensors.requestTemperatures();
  }
}

