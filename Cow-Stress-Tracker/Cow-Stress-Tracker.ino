#include <Wire.h>
#include <MPU6050_light.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "esp_sleep.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include "time.h"
#include <sys/time.h>
#include <WiFiManager.h>   
#include <Preferences.h>   
#include <Ticker.h>        
#include <math.h>
#include "OTP_Activity.h"

// ================= เวอร์ชันเฟิร์มแวร์ (สำหรับเช็กบนเว็บและอัปเดต OTA) =================
#define FIRMWARE_VERSION                "2.2.0"

// ================= ค่าเริ่มต้นการนับก้าวและการนอน (ปรับจูนและ Manual Calibrate จากหน้าเว็บได้) =================
#define STEP_TRIGGER_G                  0.25f   // แรงกระแทกเริ่มนับก้าวเริ่มต้น (g)
#define STEP_RESET_G                    0.12f   // แรงรีเซ็ตก่อนนับก้าวถัดไปเริ่มต้น (g)
#define STEP_DELAY_MS                   500UL   // ระยะเวลาขั้นต่ำระหว่างก้าว (ms)
#define ACCEL_SMOOTH_ALPHA              0.35f   // ตัวกรองสัญญาณความเร่ง (0.1-1.0)
#define LIE_ANGLE_DEG                   45.0f   // มุมเอียงขาที่ถือว่านอนเริ่มต้น (องศา)
#define STAND_ANGLE_DEG                 30.0f   // มุมเอียงขาที่ถือว่ายืนเริ่มต้น (องศา)
#define STAND_TO_LIE_DELAY_SEC          3UL     // หน่วงเวลาเปลี่ยนท่า ยืน -> นอน (วินาที)
#define LIE_TO_STAND_DELAY_SEC          3UL     // หน่วงเวลาเปลี่ยนท่า นอน -> ยืน (วินาที)
#define REF_GRAV_X                      0.000f  // เวกเตอร์แรงโน้มถ่วงท่ายืนอ้างอิงเริ่มต้น แกน X
#define REF_GRAV_Y                      0.000f  // เวกเตอร์แรงโน้มถ่วงท่ายืนอ้างอิงเริ่มต้น แกน Y
#define REF_GRAV_Z                      1.000f  // เวกเตอร์แรงโน้มถ่วงท่ายืนอ้างอิงเริ่มต้น แกน Z

// ================= ตั้งค่าการตื่น/การหลับประหยัดพลังงาน =================
#define DELTA_THRESHOLD                 0.08f      // แรงขยับขั้นต่ำที่ถือว่ายังมีการเคลื่อนไหวตอนยืน (g)
#define ACTIVE_WINDOW_MS                10000UL    // เวลาอ่านค่าหลังไม่มีการขยับตอนยืนก่อนเข้าโหมดหลับ (10 วินาที)
#define SAMPLE_INTERVAL_MS              20UL       // อ่านค่า MPU6050 ที่ 50 Hz (ทุก 20 ms)
#define WAKE_INTERVAL                   5000000ULL // หลับสั้น 5 วินาที (เมื่อเพิ่งหยุดเดินในท่ายืน)
#define LONG_SLEEP_INTERVAL             30000000ULL// หลับยาว 30 วินาที (เมื่อนอน หรือ ยืนนิ่งต่อเนื่อง)
#define MAX_IDLE_WAKES                  6          // จำนวนรอบที่ยืนนิ่งก่อนเข้าโหมดหลับยาว 30 วินาที
#define LYING_CHECK_SAMPLES             50         // ตอนวัวนอน: ตื่นมาเช็กมุมขา 50 ตัวอย่าง (1.0 วินาที) ถ้ายังนอนอยู่ให้หลับต่อทันทีโดยไม่รอลูป 10 วินาที
#define DEFAULT_SEND_INTERVAL_S         60UL       // รอบการเปิด Wi-Fi ส่งข้อมูลเริ่มต้น (60 วินาที - ปรับจากหน้าเว็บได้)
#define TRANSITION_GRACE_SEC            0.5f       // กันสั่นระหว่างจับเวลาเปลี่ยนท่า (0.5 วินาที)

// ================= ตั้งค่า Wi-Fi (WiFiManager) =================
// ไม่ต้องใส่ชื่อ/รหัส WiFi ในโค้ดแล้ว -> ตั้งค่าผ่านหน้าเว็บ "Cow-Stress-Tracker-Config"
#define CONFIG_AP_NAME                 "Cow-Stress-Tracker-Config"
#define DEFAULT_WIFI_SSID              "TP-Link_0A08"  // ค่า WiFi เริ่มต้น (ใช้เมื่อยังไม่เคยตั้งค่าผ่านหน้าเว็บ)
#define DEFAULT_WIFI_PASS              "84940339"
#define CONFIG_PORTAL_TIMEOUT          180     // วินาที: หมดเวลาหน้าตั้งค่า แล้วทำงานต่อด้วยค่าเดิม
#define WIFI_CONNECT_TIMEOUT           25      // รอบ x 400ms = 10 วินาที (ตอนส่งข้อมูล)
#define DEFAULT_DEVICE_ID              6       // ใช้เมื่อยังไม่เคยตั้งค่า Device ID
#define CONFIG_BUTTON_PIN              9       // ปุ่ม BOOT (GPIO9) ใช้บังคับเข้าโหมดตั้งค่า
#define CONFIG_BUTTON_WINDOW           3000    // ms: ช่วงเวลาให้กดปุ่ม BOOT หลังเปิดเครื่อง/กด RESET
#define STATUS_LED_PIN                 -1      // -1 = ไม่ใช้ LED (ถ้าบอร์ดมี LED ใส่เลขขาได้ เช่น 8)

// ================= ตั้งค่าเซิร์ฟเวอร์ =================
// เปลี่ยน DEFAULT_SERVER เป็นชื่อเว็บ Vercel ของคุณ หรือตั้งผ่าน WiFiManager ตอนกดปุ่ม BOOT ได้เลย
#define DEFAULT_SERVER                 "cow-stress-tracker.vercel.app"
#define DEFAULT_PATH                   "/api/iot/input"

String server  = DEFAULT_SERVER;
String pathphp = DEFAULT_PATH;

// ================= ตั้งค่าฮาร์ดแวร์ =================
#define ONE_WIRE_BUS        D3
#define I2C_SDA_PIN         D5
#define I2C_SCL_PIN         D4

// RTC memory (หน่วยความจำที่ไม่หายตอน Deep Sleep)
RTC_DATA_ATTR int      stepCount          = 0;
RTC_DATA_ATTR float    tempC              = 0.0f;
RTC_DATA_ATTR int      idleWakeCount      = 0;
RTC_DATA_ATTR float    prevMagnitude      = 1.0f;
RTC_DATA_ATTR bool     bigSleepMode       = false;

// ตัวแปรเก็บค่าท่าทางและการนับการยืน/การนอน (ไม่หายตอน Deep Sleep)
RTC_DATA_ATTR float    last_angX          = 0.0f;
RTC_DATA_ATTR float    last_angY          = 0.0f;
RTC_DATA_ATTR float    last_angZ          = 0.0f;
RTC_DATA_ATTR int      last_stand         = 1;      // 1 = ยืน (Stand=1, Sleep=0), 0 = นอน (Stand=0, Sleep=1)
RTC_DATA_ATTR int      standCount         = 1;      // จำนวนครั้งที่เปลี่ยนเป็นท่ายืน
RTC_DATA_ATTR int      lieCount           = 0;      // จำนวนครั้งที่เปลี่ยนเป็นท่านอน
RTC_DATA_ATTR float    rtcRefGravX        = REF_GRAV_X;
RTC_DATA_ATTR float    rtcRefGravY        = REF_GRAV_Y;
RTC_DATA_ATTR float    rtcRefGravZ        = REF_GRAV_Z;
RTC_DATA_ATTR float    rtcGravX           = REF_GRAV_X;
RTC_DATA_ATTR float    rtcGravY           = REF_GRAV_Y;
RTC_DATA_ATTR float    rtcGravZ           = REF_GRAV_Z;
RTC_DATA_ATTR float    rtcTiltDeg         = 0.0f;
RTC_DATA_ATTR float    rtcTransitionSec   = 0.0f;   // สะสมเวลาหน่วงเปลี่ยนท่าต่อเนื่อง
RTC_DATA_ATTR float    rtcGraceSec        = 0.0f;   // ตัวช่วยกันสั่นขณะเปลี่ยนท่า

// พารามิเตอร์ที่จูนและกด Manual Calibrate จากหน้าเว็บ (จำลงทั้ง RTC และ Flash)
RTC_DATA_ATTR float    rtcStepTriggerG    = STEP_TRIGGER_G;
RTC_DATA_ATTR float    rtcStepResetG      = STEP_RESET_G;
RTC_DATA_ATTR float    rtcLieAngleDeg     = LIE_ANGLE_DEG;
RTC_DATA_ATTR float    rtcStandAngleDeg   = STAND_ANGLE_DEG;
RTC_DATA_ATTR int      rtcCalSeq          = 0;
RTC_DATA_ATTR bool     rtcContinuousMode  = false;

// ตัวแปรเก็บค่า Gyro Offset ใน RTC เพื่อไม่ต้อง Calibrate ใหม่ตอนตื่น และไม่สั่งปิด/เปิด Gyro
RTC_DATA_ATTR bool     rtcGyroCalibrated  = false;
RTC_DATA_ATTR float    rtcGyroOffX        = 0.0f;
RTC_DATA_ATTR float    rtcGyroOffY        = 0.0f;
RTC_DATA_ATTR float    rtcGyroOffZ        = 0.0f;

// ตัวแปรควบคุมรอบการส่งข้อมูลและ Fast Wi-Fi Reconnect (ประหยัดแบตเตอรี่)
RTC_DATA_ATTR uint32_t rtcLastSendEpoch   = 0;
RTC_DATA_ATTR uint32_t rtcSendIntervalSec = DEFAULT_SEND_INTERVAL_S;
RTC_DATA_ATTR uint32_t rtcElapsedSleepSec = 0;
RTC_DATA_ATTR uint8_t  rtcBssid[6]        = {0, 0, 0, 0, 0, 0};
RTC_DATA_ATTR int32_t  rtcWifiChannel     = 0;
RTC_DATA_ATTR bool     rtcHasWifiHint     = false;

MPU6050 mpu(Wire);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

unsigned long lastSampleTime     = 0;
unsigned long lastStepTime       = 0;
unsigned long lastMovementTime   = 0;
unsigned long timer              = 0;
unsigned long wakeStartMs        = 0;
int           count              = 0;
int           wakeSamples        = 0;
bool          movementDetected   = false;
bool          stepDetected       = false;
bool          stepArmed          = true;
bool          postureJustChanged = false;
float         dynMag             = 0.0f;

// ตัวแปรการตั้งค่า (โหลดจาก Flash ทุกครั้งที่บูต)
Preferences preferences;   // ตัวช่วยบันทึกค่าลง Flash
Ticker      ticker;        // ตัวช่วยกระพริบไฟ
int         deviceId  = DEFAULT_DEVICE_ID;
DeviceKey   deviceKey = getDeviceKey(DEFAULT_DEVICE_ID);
String      wifiSsid  = "";
String      wifiPass  = "";

// ประกาศฟังก์ชันล่วงหน้า
void goToSleep();
void performOTAUpdate(const String &otaUrl, const String &expectedMd5, const String &targetVer);

// ================================================================
// คำนวณมุมเอียงต้นขาเทียบกับท่ายืนอ้างอิงใน RTC (0 - 180 องศา)
// ================================================================
float computeLegTiltDeg(float gx, float gy, float gz) {
  float normG = sqrtf(gx * gx + gy * gy + gz * gz);
  float normR = sqrtf(rtcRefGravX * rtcRefGravX + rtcRefGravY * rtcRefGravY + rtcRefGravZ * rtcRefGravZ);
  if (normG < 0.001f || normR < 0.001f) return 0.0f;

  float dot = (gx * rtcRefGravX + gy * rtcRefGravY + gz * rtcRefGravZ) / (normG * normR);
  if (dot > 1.0f) dot = 1.0f;
  if (dot < -1.0f) dot = -1.0f;
  return acosf(dot) * (180.0f / PI);
}

// ================================================================
// กู้คืนบัส I2C กรณีสาย SDA/SCL ค้างหลังตื่นจาก Deep Sleep (ป้องกันค้างลูปอนันต์)
// ================================================================
bool initMPUWithRecovery() {
  for (int attempt = 1; attempt <= 3; attempt++) {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    byte status = mpu.begin();
    if (status == 0) {
      return true;
    }
    Serial.printf("⚠️ MPU6050 status=%d (ครั้งที่ %d/3) -> กำลังรีเซ็ตบัส I2C...\n", status, attempt);
    Wire.end();
    pinMode(I2C_SCL_PIN, OUTPUT);
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    for (int i = 0; i < 9; i++) {
      digitalWrite(I2C_SCL_PIN, LOW);
      delayMicroseconds(10);
      digitalWrite(I2C_SCL_PIN, HIGH);
      delayMicroseconds(10);
    }
    delay(50);
  }
  return false;
}

// ================================================================
// การจำค่า Device ID / WiFi / Server ลง Flash (Preferences)
// ================================================================
void loadSettings() {
  preferences.begin("cattle", false);
  deviceId = preferences.getInt("devID", DEFAULT_DEVICE_ID);
  wifiSsid = preferences.getString("ssid", DEFAULT_WIFI_SSID);
  wifiPass = preferences.getString("pass", DEFAULT_WIFI_PASS);
  server   = preferences.getString("srv",  DEFAULT_SERVER);
  pathphp  = preferences.getString("path", DEFAULT_PATH);

  // ถ้าแก้ DEFAULT_DEVICE_ID ในโค้ดแล้วอัปโหลดใหม่ -> ใช้ค่าใหม่จากโค้ดทับค่าที่เคยจำไว้ใน Flash
  if (preferences.getInt("fwDevID", -1) != DEFAULT_DEVICE_ID) {
    Serial.printf("🆕 พบ DEFAULT_DEVICE_ID ใหม่จากโค้ด -> ใช้ Device ID %d\n", DEFAULT_DEVICE_ID);
    deviceId = DEFAULT_DEVICE_ID;
    preferences.putInt("fwDevID", DEFAULT_DEVICE_ID);
    preferences.putInt("devID", deviceId);
  }

  // ถ้าแก้ DEFAULT_SERVER ในโค้ดใหม่ -> อัปเดตค่าใน Flash ให้ตรงกัน
  String lastFwSrv = preferences.getString("fwSrv", "");
  if (lastFwSrv != String(DEFAULT_SERVER)) {
    server  = DEFAULT_SERVER;
    pathphp = DEFAULT_PATH;
    preferences.putString("fwSrv", DEFAULT_SERVER);
    preferences.putString("srv",   server);
    preferences.putString("path",  pathphp);
  }

  // โหลดค่าที่เคยกด Manual Calibrate ไว้ใน Flash (ถ้าตื่นจาก Deep Sleep ค่าจะอยู่ใน RTC แล้ว แต่โหลดสำรองตอนเปิดเครื่อง)
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
    rtcRefGravX       = preferences.getFloat("refGx", REF_GRAV_X);
    rtcRefGravY       = preferences.getFloat("refGy", REF_GRAV_Y);
    rtcRefGravZ       = preferences.getFloat("refGz", REF_GRAV_Z);
    rtcStepTriggerG   = preferences.getFloat("trig",  STEP_TRIGGER_G);
    rtcStepResetG     = preferences.getFloat("rst",   STEP_RESET_G);
    rtcLieAngleDeg    = preferences.getFloat("lie",   LIE_ANGLE_DEG);
    rtcStandAngleDeg  = preferences.getFloat("std",   STAND_ANGLE_DEG);
    rtcCalSeq         = preferences.getInt("calSeq",  0);
    rtcContinuousMode = preferences.getBool("cont",   false);
  }
  preferences.end();

  if (!isValidDeviceId(deviceId)) deviceId = DEFAULT_DEVICE_ID;
  if (server.length() == 0) server = DEFAULT_SERVER;
  if (pathphp.length() == 0) pathphp = DEFAULT_PATH;
  deviceKey = getDeviceKey(deviceId);
}

void saveSettings() {
  preferences.begin("cattle", false);
  preferences.putInt("devID", deviceId);
  preferences.putString("ssid", wifiSsid);
  preferences.putString("pass", wifiPass);
  preferences.putString("srv",  server);
  preferences.putString("path", pathphp);
  preferences.end();
}

void saveCalibrationToFlash() {
  preferences.begin("cattle", false);
  preferences.putFloat("refGx", rtcRefGravX);
  preferences.putFloat("refGy", rtcRefGravY);
  preferences.putFloat("refGz", rtcRefGravZ);
  preferences.putFloat("trig",  rtcStepTriggerG);
  preferences.putFloat("rst",   rtcStepResetG);
  preferences.putFloat("lie",   rtcLieAngleDeg);
  preferences.putFloat("std",   rtcStandAngleDeg);
  preferences.putInt("calSeq",  rtcCalSeq);
  preferences.putBool("cont",   rtcContinuousMode);
  preferences.end();
}

// ================================================================
// ไฟกระพริบตอนอยู่ในโหมดตั้งค่า (ถ้ากำหนด STATUS_LED_PIN)
// ================================================================
void tick() {
#if STATUS_LED_PIN >= 0
  digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
#endif
}

// เมื่อเข้าโหมด Config (ปล่อย WiFi) ให้ไฟกระพริบถี่ๆ
void configModeCallback(WiFiManager *myWiFiManager) {
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  Serial.println("📶 โหมดตั้งค่า (Config Mode)");
  Serial.println("กรุณาเชื่อมต่อ WiFi: " + myWiFiManager->getConfigPortalSSID());
  Serial.println("แล้วเปิด http://192.168.4.1 เพื่อตั้งค่า WiFi, Device ID และ Server URL");
#if STATUS_LED_PIN >= 0
  ticker.attach(0.2, tick);
#endif
}

// รอให้กดปุ่ม BOOT ภายใน CONFIG_BUTTON_WINDOW หลังบูต
bool configButtonPressed() {
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
  Serial.printf("👉 กดปุ่ม BOOT ภายใน %d วินาที เพื่อเข้าโหมดตั้งค่า WiFi / Device ID / Server\n",
                CONFIG_BUTTON_WINDOW / 1000);
  unsigned long start = millis();
  while (millis() - start < CONFIG_BUTTON_WINDOW) {
    if (digitalRead(CONFIG_BUTTON_PIN) == LOW) {
      delay(50); // กันสัญญาณเด้ง
      if (digitalRead(CONFIG_BUTTON_PIN) == LOW) return true;
    }
    delay(10);
  }
  return false;
}

// ================================================================
// เชื่อมต่อ WiFi แบบรวดเร็ว (ใช้ BSSID/Channel ที่จำไว้ใน RTC ก่อน)
// ================================================================
bool connectWiFi(const String &ssid, const String &pass) {
  if (ssid.length() == 0) return false;

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // ลดกำลังส่งป้องกันไฟตกตอนใช้แบตเตอรี่

  // 1. ลองเชื่อมต่อแบบ Fast Connect ด้วย Channel/BSSID เดิมก่อน
  if (rtcHasWifiHint && rtcWifiChannel > 0) {
    WiFi.begin(ssid.c_str(), pass.c_str(), rtcWifiChannel, rtcBssid, true);
    int fastTimeout = 0;
    while (WiFi.status() != WL_CONNECTED && fastTimeout < 10) {
      delay(300);
      Serial.print(".");
      fastTimeout++;
    }
  }

  // 2. ถ้า Fast Connect ไม่ติด ให้สแกนเชื่อมต่อแบบปกติ
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(false);
    delay(50);
    WiFi.begin(ssid.c_str(), pass.c_str());

    int wifi_timeout = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_timeout < WIFI_CONNECT_TIMEOUT) {
      delay(400);
      Serial.print(".");
      wifi_timeout++;
    }
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    uint8_t *bssid = WiFi.BSSID();
    if (bssid != nullptr) {
      memcpy(rtcBssid, bssid, 6);
      rtcWifiChannel = WiFi.channel();
      rtcHasWifiHint = true;
    }
    return true;
  }

  rtcHasWifiHint = false;
  return false;
}

// ================================================================
// WiFiManager: ใช้เฉพาะตอนเปิดเครื่อง/กด RESET (ไม่ใช้ตอนตื่นจาก Deep Sleep)
// ================================================================
bool setupWiFiManager(bool forcePortal) {
  if (!forcePortal) {
    if (connectWiFi(wifiSsid, wifiPass)) {
      Serial.println("✅ WiFi Connected! IP: " + WiFi.localIP().toString());
      saveSettings();
      return true;
    }
    Serial.println("⚠️ ต่อ WiFi \"" + wifiSsid + "\" ไม่ได้ -> เปิดหน้าตั้งค่า");
  }

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  WiFiManager wm;
  wm.setAPCallback(configModeCallback);
  wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);
  wm.setConnectTimeout(20);

  char idStr[4];
  snprintf(idStr, sizeof(idStr), "%d", deviceId);
  WiFiManagerParameter customDeviceId("did", "Device ID (1-20)", idStr, 3);
  WiFiManagerParameter customServer("srv", "Server Domain (เช่น cow-stress-tracker.vercel.app)", server.c_str(), 80);
  WiFiManagerParameter customPath("path", "API Path (เช่น /api/iot/input)", pathphp.c_str(), 100);

  wm.addParameter(&customDeviceId);
  wm.addParameter(&customServer);
  wm.addParameter(&customPath);

  bool connected = wm.startConfigPortal(CONFIG_AP_NAME);

  ticker.detach();
#if STATUS_LED_PIN >= 0
  digitalWrite(STATUS_LED_PIN, HIGH);
#endif

  if (!connected) {
    Serial.println("❌ ตั้งค่า/เชื่อมต่อ WiFi ไม่สำเร็จ -> ทำงานต่อด้วยค่าเดิม");
    return false;
  }

  Serial.println("✅ WiFi Connected! IP: " + WiFi.localIP().toString());

  int newID = String(customDeviceId.getValue()).toInt();
  if (isValidDeviceId(newID)) {
    if (newID != deviceId) Serial.println("📝 เปลี่ยน Device ID เป็น: " + String(newID));
    deviceId = newID;
  } else {
    Serial.println("⚠️ Device ID ไม่ถูกต้อง (ต้องเป็น 1-20) ใช้ค่าเดิม: " + String(deviceId));
  }
  deviceKey = getDeviceKey(deviceId);

  String newSrv = String(customServer.getValue());
  newSrv.trim();
  newSrv.replace("https://", "");
  newSrv.replace("http://", "");
  if (newSrv.endsWith("/")) newSrv.remove(newSrv.length() - 1);
  if (newSrv.length() > 0) server = newSrv;

  String newPath = String(customPath.getValue());
  newPath.trim();
  if (newPath.length() > 0) {
    if (!newPath.startsWith("/")) newPath = "/" + newPath;
    pathphp = newPath;
  }

  wifiSsid = WiFi.SSID();
  wifiPass = WiFi.psk();
  saveSettings();
  return true;
}

// ================================================================
// จัดการเวลา Unix Timestamp (ใช้ RTC ภายในก่อน ถ้ายังไม่มีค่อยซิงค์ NTP)
// ================================================================
uint32_t getValidEpoch() {
  time_t now;
  time(&now);
  if (now > 1700000000UL) {
    return (uint32_t)now;
  }

  Serial.print("⏳ Syncing NTP time");
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  struct tm timeinfo;
  int retry = 0;
  while (!getLocalTime(&timeinfo, 400) && retry < 8) {
    Serial.print(".");
    retry++;
  }
  Serial.println();
  time(&now);
  return (now > 1700000000UL) ? (uint32_t)now : 0;
}

void updateRtcFromServerEpoch(uint32_t serverEpoch) {
  if (serverEpoch > 1700000000UL) {
    struct timeval tv = { .tv_sec = (time_t)serverEpoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
  }
}

String extractJsonString(const String &json, const String &key) {
  String pattern = "\"" + key + "\":\"";
  int start = json.indexOf(pattern);
  if (start < 0) return "";
  start += pattern.length();
  int end = json.indexOf("\"", start);
  if (end < 0) return "";
  return json.substring(start, end);
}

long extractJsonLong(const String &json, const String &key, long defaultVal) {
  String pattern = "\"" + key + "\":";
  int start = json.indexOf(pattern);
  if (start < 0) return defaultVal;
  start += pattern.length();
  while (start < (int)json.length() && json[start] == ' ') start++;
  int end = start;
  while (end < (int)json.length() && (isDigit(json[end]) || json[end] == '-')) end++;
  if (end == start) return defaultVal;
  return json.substring(start, end).toInt();
}

float extractJsonFloat(const String &json, const String &key, float defaultVal) {
  String pattern = "\"" + key + "\":";
  int start = json.indexOf(pattern);
  if (start < 0) return defaultVal;
  start += pattern.length();
  while (start < (int)json.length() && json[start] == ' ') start++;
  int end = start;
  while (end < (int)json.length() && (isDigit(json[end]) || json[end] == '-' || json[end] == '.')) end++;
  if (end == start) return defaultVal;
  return json.substring(start, end).toFloat();
}

// ================================================================
// อัปเดตเฟิร์มแวร์ผ่านเน็ต (HTTP/HTTPS OTA Update พร้อมตรวจ MD5)
// ================================================================
void performOTAUpdate(const String &otaUrl, const String &expectedMd5, const String &targetVer) {
  if (otaUrl.length() == 0) return;

  Serial.println("\n==========================================");
  Serial.println("🚀 พบเฟิร์มแวร์ใหม่เวอร์ชัน: " + targetVer);
  Serial.println("📥 กำลังดาวน์โหลดจาก: " + otaUrl);
  Serial.println("==========================================");

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(30000);

  if (!http.begin(client, otaUrl)) {
    Serial.println("❌ OTA: ไม่สามารถเปิดการเชื่อมต่อ URL ได้");
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("❌ OTA: ดาวน์โหลดไม่สำเร็จ HTTP Code: %d\n", httpCode);
    http.end();
    return;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("❌ OTA: ขนาดไฟล์ไม่ถูกต้อง");
    http.end();
    return;
  }

  if (!Update.begin(contentLength)) {
    Serial.println("❌ OTA: พื้นที่ Flash ไม่เพียงพอสำหรับอัปเดต");
    http.end();
    return;
  }

  if (expectedMd5.length() == 32) {
    Update.setMD5(expectedMd5.c_str());
    Serial.println("🔐 ตั้งค่าตรวจ MD5: " + expectedMd5);
  }

  WiFiClient *stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);

  if (written == (size_t)contentLength && Update.end()) {
    if (Update.isFinished()) {
      Serial.println("✅ อัปเดตเฟิร์มแวร์สำเร็จ! กำลังรีบูตเข้าสู่เวอร์ชัน " + targetVer + "...");
      Serial.flush();
      http.end();
      delay(500);
      ESP.restart();
    }
  }

  Serial.printf("❌ OTA ล้มเหลว (เขียนได้ %u/%d bytes) Error: %s\n",
                (unsigned int)written, contentLength, Update.errorString());
  http.end();
}

// ================================================================
// ตรวจสอบว่าถึงรอบที่ควรเปิด Wi-Fi ส่งข้อมูลหรือยัง
// ================================================================
bool shouldSendData(bool forceSend) {
  if (forceSend || postureJustChanged || rtcLastSendEpoch == 0) {
    return true;
  }
  // ตรวจจากเวลาที่ผ่านไปจริงในการหลับและตื่นแต่ละรอบ (แม่นยำแม้เซิร์ฟเวอร์ไม่ได้ส่ง server_time กลับ)
  if (rtcElapsedSleepSec >= rtcSendIntervalSec) {
    return true;
  }
  time_t now;
  time(&now);
  if (now > 1700000000UL && (uint32_t)now >= rtcLastSendEpoch) {
    return ((uint32_t)now - rtcLastSendEpoch) >= rtcSendIntervalSec;
  }
  return false;
}

// ================================================================
// ฟังก์ชันสำหรับส่งข้อมูลขึ้นเซิร์ฟเวอร์ (ใช้รหัส OTP เดิม + รองรับ OTA)
// ================================================================
void sendDataToAPI(bool forceSend = false) {
  if (!shouldSendData(forceSend)) {
    Serial.printf("⏭️ ยังไม่ถึงรอบส่ง Wi-Fi (ผ่านไป %u/%u วินาที | ก้าวสะสม: %d)\n",
                  (unsigned int)rtcElapsedSleepSec, (unsigned int)rtcSendIntervalSec, stepCount);
    return;
  }

  if (wifiSsid.length() == 0) {
    Serial.println("⚠️ ยังไม่ได้ตั้งค่า WiFi (กด RESET แล้วกดปุ่ม BOOT เพื่อตั้งค่า), skip API call.");
    return;
  }

  // อ่านอุณหภูมิล่าสุดก่อนส่งข้อมูล
  float latestTemp = sensors.getTempCByIndex(0);
  if (latestTemp != DEVICE_DISCONNECTED_C && latestTemp > -40.0f && latestTemp < 85.0f) {
    tempC = latestTemp;
  }

  if (connectWiFi(wifiSsid, wifiPass)) {
    long rssi = WiFi.RSSI();
    Serial.printf("📶 WiFi connected! (RSSI: %ld dBm)\n", rssi);

    uint32_t nowEpoch = getValidEpoch();
    String ts = (nowEpoch > 0) ? String(nowEpoch) : "0";

    if (ts != "0") {
      // สร้างรหัส OTP ด้วยสมการและ Key เดิมทุกประการ
      int otp_val = genCode(ts, deviceId, deviceKey.code1, deviceKey.code2, deviceKey.prime);

      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient http;
      http.setTimeout(10000);

      String url = "https://" + server + pathphp +
                   "?DID=" + String(deviceId) +
                   "&ACT=" + String(stepCount) +
                   "&ANGX=" + String(last_angX, 2) +
                   "&ANGY=" + String(last_angY, 2) +
                   "&ANGZ=" + String(last_angZ, 2) +
                   "&GX=" + String(rtcGravX, 3) +
                   "&GY=" + String(rtcGravY, 3) +
                   "&GZ=" + String(rtcGravZ, 3) +
                   "&Tilt=" + String(rtcTiltDeg, 1) +
                   "&Stand=" + String(last_stand) +
                   "&Sleep=" + String(last_stand == 1 ? 0 : 1) +
                   "&Temp=" + String(tempC, 2) +
                   "&Time=" + ts +
                   "&Code=" + String(otp_val) +
                   "&FW=" + String(FIRMWARE_VERSION) +
                   "&RSSI=" + String(rssi);

      Serial.println("🌐 Sending API Request: " + url);
      http.begin(client, url);
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {
        Serial.print("✅ HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println("📩 Payload: " + payload);

        if (httpResponseCode == 200) {
          stepCount          = 0;
          postureJustChanged = false;
          rtcElapsedSleepSec = 0;

          // อัปเดตเวลา RTC จากเซิร์ฟเวอร์ (ถ้ามีส่งกลับมาใน JSON)
          long srvTime = extractJsonLong(payload, "server_time", 0);
          if (srvTime > 1700000000L) {
            updateRtcFromServerEpoch((uint32_t)srvTime);
            rtcLastSendEpoch = (uint32_t)srvTime;
          } else {
            rtcLastSendEpoch = nowEpoch;
          }

          // อัปเดตความถี่การส่งข้อมูลจากหน้าเว็บ (จำกัดช่วง 5 - 3600 วินาที)
          long newSendSec = extractJsonLong(payload, "send_sec", 0);
          if (newSendSec >= 5 && newSendSec <= 3600) {
            rtcSendIntervalSec = (uint32_t)newSendSec;
          }

          // อัปเดตโหมดส่งต่อเนื่อง (Continuous Calibrate Mode) vs โหมดประหยัดไฟ (Deep Sleep)
          long contMode = extractJsonLong(payload, "cont_mode", -1);
          if (contMode == 0 || contMode == 1) {
            bool newCont = (contMode == 1);
            if (newCont != rtcContinuousMode) {
              rtcContinuousMode = newCont;
              saveCalibrationToFlash();
              Serial.printf("⚡ เปลี่ยนโหมดการทำงานจากหน้าเว็บ: %s\n",
                            rtcContinuousMode ? "ส่งต่อเนื่องทุก 5s (ไม่หลับ)" : "ประหยัดพลังงาน (Deep Sleep)");
            }
          }

          // ตรวจสอบคำสั่ง Manual Calibrate & จูนค่าพารามิเตอร์จากหน้าเว็บ
          long srvCalSeq = extractJsonLong(payload, "cal_seq", -1);
          if (srvCalSeq > 0 && srvCalSeq != rtcCalSeq) {
            rtcCalSeq = (int)srvCalSeq;
            float rx = extractJsonFloat(payload, "ref_gx", rtcRefGravX);
            float ry = extractJsonFloat(payload, "ref_gy", rtcRefGravY);
            float rz = extractJsonFloat(payload, "ref_gz", rtcRefGravZ);

            // ถ้าเว็บส่ง (0,0,0) แปลว่าสั่งให้บอร์ดวัดค่าเฉลี่ย 20 ตัวอย่างสดๆ ตอนนี้เลย
            if (fabsf(rx) < 0.001f && fabsf(ry) < 0.001f && fabsf(rz) < 0.001f) {
              float sGx = 0.0f, sGy = 0.0f, sGz = 0.0f;
              for (int i = 0; i < 20; i++) {
                mpu.update();
                sGx += mpu.getAccX();
                sGy += mpu.getAccY();
                sGz += mpu.getAccZ();
                delay(10);
              }
              float n = sqrtf(sGx * sGx + sGy * sGy + sGz * sGz);
              if (n > 0.01f) {
                rtcRefGravX = sGx / n;
                rtcRefGravY = sGy / n;
                rtcRefGravZ = sGz / n;
              }
            } else {
              rtcRefGravX = rx;
              rtcRefGravY = ry;
              rtcRefGravZ = rz;
            }

            rtcStepTriggerG  = extractJsonFloat(payload, "trig", rtcStepTriggerG);
            rtcStepResetG    = extractJsonFloat(payload, "rst",  rtcStepResetG);
            rtcLieAngleDeg   = extractJsonFloat(payload, "lie",  rtcLieAngleDeg);
            rtcStandAngleDeg = extractJsonFloat(payload, "std",  rtcStandAngleDeg);

            rtcTiltDeg       = computeLegTiltDeg(rtcGravX, rtcGravY, rtcGravZ);
            last_stand       = (rtcTiltDeg >= rtcLieAngleDeg) ? 0 : 1;
            rtcTransitionSec = 0.0f;
            saveCalibrationToFlash();

            Serial.printf("🎯 รับคำสั่ง Manual Calibrate #%d จากหน้าเว็บ! Ref=(%.2f, %.2f, %.2f) | Trig=%.2fg | Lie=%.0f° Stand=%.0f°\n",
                          rtcCalSeq, rtcRefGravX, rtcRefGravY, rtcRefGravZ,
                          rtcStepTriggerG, rtcLieAngleDeg, rtcStandAngleDeg);
          }

          // ตรวจสอบคำสั่งอัปเดตเฟิร์มแวร์ OTA จากเว็บ
          String otaVer = extractJsonString(payload, "ota_ver");
          String otaUrl = extractJsonString(payload, "ota_url");
          String otaMd5 = extractJsonString(payload, "ota_md5");
          if (otaUrl.length() > 0 && otaVer.length() > 0 && otaVer != String(FIRMWARE_VERSION)) {
            http.end();
            performOTAUpdate(otaUrl, otaMd5, otaVer);
          }
        }
      } else {
        Serial.print("❌ HTTP Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    }
  } else {
    Serial.println("⚠️ WiFi Connection Failed, skip API call.");
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF); // ปิดวิทยุ Wi-Fi ทันทีหลังส่งเสร็จเพื่อประหยัดไฟ
}

// ================================================================
// เข้าโหมดหลับลึก (Deep Sleep)
// ================================================================
void goToSleep() {
  uint32_t awakeSec = (millis() - wakeStartMs) / 1000UL;
  if (awakeSec < 1) awakeSec = 1;

  if (last_stand == 0) {
    // ตอนวัวนอน: หลับยาว 30 วินาทีเสมอ ไม่ต้องตื่นถี่
    bigSleepMode  = true;
    idleWakeCount = 0;
  } else {
    // ตอนวัวยืน: ถ้าไม่มีก้าวเดินสะสมครบ MAX_IDLE_WAKES ให้เข้าโหมดหลับยาว
    if (!stepDetected) {
      idleWakeCount++;
    }
    if (idleWakeCount >= MAX_IDLE_WAKES) {
      bigSleepMode  = true;
      idleWakeCount = 0;
    }
  }

  uint32_t nextSleepSec = (last_stand == 0 || bigSleepMode) ? 30UL : 5UL;
  rtcElapsedSleepSec += awakeSec + nextSleepSec;

  // ส่งข้อมูลเฉพาะเมื่อถึงรอบเวลา หรือเมื่อเพิ่งเปลี่ยนท่า ยืน <-> นอน
  sendDataToAPI(false);

  if (last_stand == 0 || bigSleepMode) {
    Serial.println("😴 Deep Sleep 30 วินาที (โหมดประหยัดพลังงาน)...");
    Serial.flush();
    esp_sleep_enable_timer_wakeup(LONG_SLEEP_INTERVAL);
  } else {
    Serial.printf("💤 Deep Sleep 5 วินาที (Idle wake %d/%d)...\n", idleWakeCount, MAX_IDLE_WAKES);
    Serial.flush();
    esp_sleep_enable_timer_wakeup(WAKE_INTERVAL);
  }
  esp_deep_sleep_start();
}

void setup() {
  wakeStartMs = millis();
  Serial.begin(115200);
  unsigned long serialTimeout = millis();
  while (!Serial && millis() - serialTimeout < 1500);
  delay(50);

  Serial.println("\n\n========================================");
  Serial.printf("🐄 Cow Stress Tracker v%s Booting...\n", FIRMWARE_VERSION);
  Serial.println("========================================");

  loadSettings();
  Serial.printf("💾 Device ID: %d | WiFi: %s | Server: %s%s\n",
                deviceId,
                wifiSsid.length() ? wifiSsid.c_str() : "(ยังไม่ได้ตั้งค่า)",
                server.c_str(), pathphp.c_str());

  esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();
  bool firstBoot = (wakeReason != ESP_SLEEP_WAKEUP_TIMER);

  // ตั้งค่า WiFi / Device ID เฉพาะตอนเปิดเครื่องหรือกด RESET
  if (firstBoot) {
#if STATUS_LED_PIN >= 0
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);
#endif
    bool forcePortal = configButtonPressed();
    setupWiFiManager(forcePortal);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF); // ปิด WiFi ระหว่างวัดเซ็นเซอร์เพื่อประหยัดไฟ
    Serial.printf("🔑 ใช้ Key ของ Device ID %d\n", deviceId);
  }

  // เริ่มต้น DS18B20 แบบไม่บล็อก
  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();

  // เริ่มต้น MPU6050 พร้อมระบบกู้คืนบัส I2C
  Serial.println("Initializing I2C (Wire) on D5, D4...");
  if (!initMPUWithRecovery()) {
    Serial.println("❌ ไม่พบ MPU6050 หลังลองกู้บัส 3 ครั้ง -> เข้าโหมดหลับ 10 วินาทีเพื่อลองใหม่รอบหน้า");
    Serial.flush();
    esp_sleep_enable_timer_wakeup(10000000ULL);
    esp_deep_sleep_start();
  }

  if (!firstBoot) {
    Serial.printf("⏰ ตื่นจาก Deep Sleep (สถานะปัจจุบัน: %s)\n",
                  last_stand == 1 ? "ยืน [STAND - เปิดรับค่าเซ็นเซอร์]" : "นอน [LYING - เช็กเฉพาะการลุกยืน]");
    if (rtcGyroCalibrated) {
      mpu.setGyroOffsets(rtcGyroOffX, rtcGyroOffY, rtcGyroOffZ);
    }
    mpu.update();
  } else {
    Serial.println("🔌 เปิดเครื่องครั้งแรก (First boot) — กำลัง Calibrate MPU6050...");
    idleWakeCount      = 0;
    prevMagnitude      = 1.0f;
    bigSleepMode       = false;
    stepCount          = 0;
    standCount         = 1;
    lieCount           = 0;
    last_stand         = 1; // เริ่มต้นที่ท่ายืนเสมอ!
    rtcTransitionSec   = 0.0f;
    rtcGraceSec        = 0.0f;
    rtcLastSendEpoch   = 0;
    rtcElapsedSleepSec = 0;

    delay(800);
    mpu.calcOffsets(true, false); // Calibrate เฉพาะ Gyro
    rtcGyroOffX       = mpu.getGyroXoffset();
    rtcGyroOffY       = mpu.getGyroYoffset();
    rtcGyroOffZ       = mpu.getGyroZoffset();
    rtcGyroCalibrated = true;

    // เก็บค่าเฉลี่ยแรงโน้มถ่วง 20 ตัวอย่างตอนเปิดเครื่อง
    float sumGx = 0.0f, sumGy = 0.0f, sumGz = 0.0f;
    for (int i = 0; i < 20; i++) {
      mpu.update();
      sumGx += mpu.getAccX();
      sumGy += mpu.getAccY();
      sumGz += mpu.getAccZ();
      delay(10);
    }
    rtcGravX  = sumGx / 20.0f;
    rtcGravY  = sumGy / 20.0f;
    rtcGravZ  = sumGz / 20.0f;
    last_angX = mpu.getAngleX();
    last_angY = mpu.getAngleY();
    last_angZ = mpu.getAngleZ();

    // ใช้ค่าท่ายืนอ้างอิงที่เคยกด Manual Calibrate ไว้ใน Flash (ไม่ใช้ Auto-Calibrate ตอนเปิดเครื่อง)
    rtcTiltDeg = computeLegTiltDeg(rtcGravX, rtcGravY, rtcGravZ);
    Serial.printf("✅ โหลดค่า Manual Calibrate จาก Flash: RefGrav=(%.2f, %.2f, %.2f) | มุมปัจจุบัน Tilt: %.1f° (กด Calibrate ใหม่ได้จากหน้าเว็บ)\n",
                  rtcRefGravX, rtcRefGravY, rtcRefGravZ, rtcTiltDeg);

    float initTemp = sensors.getTempCByIndex(0);
    if (initTemp != DEVICE_DISCONNECTED_C && initTemp > -40.0f && initTemp < 85.0f) {
      tempC = initTemp;
    }
    Serial.println("📡 ส่งข้อมูลชุดแรกขึ้นเซิร์ฟเวอร์ทันทีเพื่อซิงค์ค่า Manual Calibrate และตรวจสอบ OTA...");
    sendDataToAPI(true);
  }

  lastSampleTime     = millis();
  lastMovementTime   = millis();
  movementDetected   = false;
  stepDetected       = false;
  stepArmed          = true;
  postureJustChanged = false;
  wakeSamples        = 0;
}

void loop() {
  unsigned long nowMs = millis();

  // อ่านค่าที่ความถี่ 50 Hz (ทุก 20 ms)
  if (nowMs - lastSampleTime < SAMPLE_INTERVAL_MS) {
    return;
  }
  float dtSec = (nowMs - lastSampleTime) / 1000.0f;
  lastSampleTime = nowMs;
  count++;
  wakeSamples++;

  mpu.update();

  float ax = mpu.getAccX();
  float ay = mpu.getAccY();
  float az = mpu.getAccZ();

  // เก็บค่ามุมองศาล่าสุด (Gyro เปิดปกติ ไม่มีการสั่งปิด/เปิดใหม่)
  last_angX = mpu.getAngleX();
  last_angY = mpu.getAngleY();
  last_angZ = mpu.getAngleZ();

  // 1. แยกเวกเตอร์แรงโน้มถ่วงโลกด้วย Low-pass filter
  float gravAlpha = (last_stand == 0 && wakeSamples <= 20) ? 0.15f : 0.04f;
  rtcGravX = (1.0f - gravAlpha) * rtcGravX + gravAlpha * ax;
  rtcGravY = (1.0f - gravAlpha) * rtcGravY + gravAlpha * ay;
  rtcGravZ = (1.0f - gravAlpha) * rtcGravZ + gravAlpha * az;

  // 2. คำนวณมุมเอียงต้นขาเทียบกับท่ายืนอ้างอิงที่กด Manual Calibrate ไว้ (rtcRefGravX, Y, Z)
  rtcTiltDeg = computeLegTiltDeg(rtcGravX, rtcGravY, rtcGravZ);

  // หากเปิด "โหมดส่งต่อเนื่อง (Continuous Calibrate Mode)" จากหน้าเว็บ:
  // ส่งค่าขึ้นเว็บทุก 5 วินาทีโดยไม่เข้า Deep Sleep เพื่อให้กด Manual Calibrate และดูผลสดบนหน้าเว็บได้ทันที
  if (rtcContinuousMode && (nowMs - wakeStartMs >= 5000UL)) {
    wakeStartMs = nowMs;
    sendDataToAPI(true);
    lastSampleTime = millis();
    return;
  }

  // =========================================================================
  // กรณีที่ 1: สถานะปัจจุบันคือ "นอน (last_stand == 0)"
  // -> ไม่นับก้าว และไม่รันลูปรับค่าเซ็นเซอร์ 10 วินาที!
  // -> เช็กมุมขา 1.0 วินาที (50 ตัวอย่าง): ถ้าขายังเอียงนอนอยู่ ให้หลับต่อทันที (ถ้าไม่ได้เปิดโหมดส่งต่อเนื่อง)
  // =========================================================================
  if (last_stand == 0) {
    if (wakeSamples < LYING_CHECK_SAMPLES) {
      return; // เก็บตัวอย่างมุมขา 1.0 วินาที
    }

    if (rtcTiltDeg <= rtcStandAngleDeg) {
      rtcTransitionSec += dtSec;
      rtcGraceSec = 0.0f;
      if (nowMs - timer > 200) {
        Serial.printf("[LYING -> กำลังลุกยืน %.1f/%lus] Tilt: %.1f°\n",
                      rtcTransitionSec, LIE_TO_STAND_DELAY_SEC, rtcTiltDeg);
        timer = nowMs;
      }
      if (rtcTransitionSec >= (float)LIE_TO_STAND_DELAY_SEC) {
        last_stand         = 1; // เปลี่ยนสถานะเป็น "ยืน" สำเร็จ!
        standCount++;
        rtcTransitionSec   = 0.0f;
        postureJustChanged = true;
        bigSleepMode       = false;
        idleWakeCount      = 0;
        lastMovementTime   = nowMs;
        Serial.println("🟢 เปลี่ยนสถานะเป็น: ยืน (STANDING) -> เริ่มรับค่าเซ็นเซอร์และนับก้าว");
      }
      return;
    } else {
      if (rtcTransitionSec > 0.0f) {
        rtcGraceSec += dtSec;
        if (rtcGraceSec <= TRANSITION_GRACE_SEC) {
          return;
        }
      }
      rtcTransitionSec = 0.0f;
      rtcGraceSec      = 0.0f;
      if (rtcContinuousMode) {
        if (nowMs - timer > 500) {
          Serial.printf("[CONTINUOUS - LYING] Tilt: %.1f° (> %.0f°) | รอส่งข้อมูลสดทุก 5s...\n",
                        rtcTiltDeg, rtcStandAngleDeg);
          timer = nowMs;
        }
        return;
      }
      Serial.printf("😴 วัวนอนอยู่ (Tilt: %.1f° > %.0f°) -> ไม่รับค่าเซ็นเซอร์/ไม่นับก้าว เข้าสู่ Deep Sleep ทันที\n",
                    rtcTiltDeg, rtcStandAngleDeg);
      goToSleep();
      return;
    }
  }

  // =========================================================================
  // กรณีที่ 2: สถานะปัจจุบันคือ "ยืน (last_stand == 1)"
  // -> รับค่าเซ็นเซอร์ 50 Hz และนับก้าวตามปกติ
  // =========================================================================
  float magnitude = sqrtf(sq(ax) + sq(ay) + sq(az));

  float dynX      = ax - rtcGravX;
  float dynY      = ay - rtcGravY;
  float dynZ      = az - rtcGravZ;
  float rawDynMag = sqrtf(dynX * dynX + dynY * dynY + dynZ * dynZ);
  dynMag = (1.0f - ACCEL_SMOOTH_ALPHA) * dynMag + ACCEL_SMOOTH_ALPHA * rawDynMag;

  if (rtcTiltDeg >= rtcLieAngleDeg) {
    rtcTransitionSec += dtSec;
    rtcGraceSec = 0.0f;
    if (rtcTransitionSec >= (float)STAND_TO_LIE_DELAY_SEC) {
      last_stand         = 0; // เปลี่ยนเป็น "นอน"
      lieCount++;
      rtcTransitionSec   = 0.0f;
      postureJustChanged = true;
      if (rtcContinuousMode) {
        Serial.println("🟣 เปลี่ยนสถานะเป็น: นอน (LYING) [โหมดส่งต่อเนื่อง]");
        sendDataToAPI(true);
        lastSampleTime = millis();
        return;
      }
      Serial.println("🟣 เปลี่ยนสถานะเป็น: นอน (LYING) -> หยุดรับค่าเซ็นเซอร์และเข้าสู่ Deep Sleep ทันที");
      goToSleep();
      return;
    }
  } else if (rtcTransitionSec > 0.0f) {
    rtcGraceSec += dtSec;
    if (rtcGraceSec > TRANSITION_GRACE_SEC) {
      rtcTransitionSec = 0.0f;
      rtcGraceSec      = 0.0f;
    }
  }

  // เช็กการเคลื่อนไหวตอนยืนเพื่อต่อเวลาไม่ให้เพิ่งเข้า Deep Sleep ขณะกำลังเดิน
  float magnitudeDelta = fabsf(magnitude - prevMagnitude);
  if (count > 5 && magnitudeDelta > DELTA_THRESHOLD) {
    lastMovementTime = nowMs;
    movementDetected = true;
  }
  prevMagnitude = magnitude;

  // การนับก้าว (ทำงานเฉพาะตอนยืนเท่านั้น ใช้ค่าที่จูนจากหน้าเว็บ)
  if (dynMag <= rtcStepResetG) {
    stepArmed = true;
  }

  if (stepArmed &&
      dynMag >= rtcStepTriggerG &&
      (nowMs - lastStepTime >= STEP_DELAY_MS)) {
    stepCount++;
    stepArmed        = false;
    lastStepTime     = nowMs;
    lastMovementTime = nowMs;
    movementDetected = true;
    stepDetected     = true;
    idleWakeCount    = 0;
    bigSleepMode     = false;
    Serial.println("*** STEP DETECTED ***");
  }

  // ถ้าวัวยืนนิ่งเกิน ACTIVE_WINDOW_MS (10 วินาที) และไม่ได้อยู่ในโหมดส่งต่อเนื่อง -> เข้าโหมดหลับ
  if (!rtcContinuousMode && nowMs - lastMovementTime > ACTIVE_WINDOW_MS && rtcTransitionSec == 0.0f) {
    goToSleep();
    return;
  }

  if (nowMs - timer > 200) {
    Serial.print("[STAND] ");
    Serial.print("Tilt: "); Serial.print(rtcTiltDeg, 1); Serial.print("°");
    Serial.print("\t| Dyn: "); Serial.print(dynMag, 2); Serial.print("g");
    Serial.print("\t| Steps: "); Serial.print(stepCount);
    Serial.print("\t| StandCnt: "); Serial.print(standCount);
    Serial.print(" LieCnt: "); Serial.print(lieCount);

    if (count >= 50) { // ทุก ~1 วินาที
      float t = sensors.getTempCByIndex(0);
      if (t != DEVICE_DISCONNECTED_C && t > -40.0f && t < 85.0f) {
        tempC = t;
      }
      sensors.requestTemperatures();
      count = 0;
    }

    if (tempC != DEVICE_DISCONNECTED_C && tempC != 0) {
      Serial.print("\t| Temp: "); Serial.print(tempC, 1); Serial.println(" °C");
    } else {
      Serial.println();
    }

    timer = nowMs;
  }
}
