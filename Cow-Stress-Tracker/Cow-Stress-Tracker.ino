#include <Wire.h>
#include <MPU6050_light.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "esp_sleep.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "time.h"
#include <WiFiManager.h>   
#include <Preferences.h>   
#include <Ticker.h>        
#include <math.h>
#include "OTP_Activity.h"

// ================= ตั้งค่าการนับก้าวและการนอน (ได้จาก Cow-Stress-Calibrator) =================
#define STEP_TRIGGER_G          0.35f   // แรงกระแทกเริ่มนับก้าว (g)
#define STEP_RESET_G            0.15f   // แรงรีเซ็ตก่อนนับก้าวถัดไป (g)
#define STEP_DELAY_MS           500UL   // ระยะเวลาขั้นต่ำระหว่างก้าว (ms)
#define ACCEL_SMOOTH_ALPHA      0.35f   // ตัวกรองสัญญาณความเร่ง (0.1-1.0)
#define LIE_ANGLE_DEG           45.0f   // มุมเอียงขาที่ถือว่านอน (องศา)
#define STAND_ANGLE_DEG         30.0f   // มุมเอียงขาที่ถือว่ายืน (องศา)
#define STAND_TO_LIE_DELAY_SEC  10UL    // หน่วงเวลาเปลี่ยนท่า ยืน -> นอน (วินาที)
#define LIE_TO_STAND_DELAY_SEC  5UL     // หน่วงเวลาเปลี่ยนท่า นอน -> ยืน (วินาที)
#define REF_GRAV_X              0.000f  // เวกเตอร์แรงโน้มถ่วงท่ายืนอ้างอิง แกน X
#define REF_GRAV_Y              0.000f  // เวกเตอร์แรงโน้มถ่วงท่ายืนอ้างอิง แกน Y
#define REF_GRAV_Z              1.000f  // เวกเตอร์แรงโน้มถ่วงท่ายืนอ้างอิง แกน Z

// ================= ตั้งค่าการตื่น/การหลับประหยัดพลังงาน =================
#define DELTA_THRESHOLD         0.08f   // แรงขยับขั้นต่ำที่ถือว่ายังมีการเคลื่อนไหว (g)
#define ACTIVE_WINDOW_MS        10000UL // เวลาอ่านค่าหลังไม่มีการขยับก่อนเข้าโหมดหลับ (10 วินาที)
#define SAMPLE_INTERVAL_MS      20UL    // อ่านค่า MPU6050 ที่ 50 Hz (ทุก 20 ms)
#define WAKE_INTERVAL           5000000    // 5 วินาที
#define LONG_SLEEP_INTERVAL     30000000   // 30 วินาที
#define MAX_IDLE_WAKES          10

// ================= ตั้งค่า Wi-Fi (WiFiManager) =================
// ไม่ต้องใส่ชื่อ/รหัส WiFi ในโค้ดแล้ว -> ตั้งค่าผ่านหน้าเว็บ "Cow-Stress-Tracker-Config"
#define CONFIG_AP_NAME         "Cow-Stress-Tracker-Config"
#define DEFAULT_WIFI_SSID      "TP-Link_0A08"  // ค่า WiFi เริ่มต้น (ใช้เมื่อยังไม่เคยตั้งค่าผ่านหน้าเว็บ)
#define DEFAULT_WIFI_PASS      "84940339"
#define CONFIG_PORTAL_TIMEOUT  180     // วินาที: หมดเวลาหน้าตั้งค่า แล้วทำงานต่อด้วยค่าเดิม
#define WIFI_CONNECT_TIMEOUT   30      // รอบ x 500ms = 15 วินาที (ตอนส่งข้อมูล)
#define DEFAULT_DEVICE_ID      1       // ใช้เมื่อยังไม่เคยตั้งค่า Device ID
#define CONFIG_BUTTON_PIN      9       // ปุ่ม BOOT (GPIO9) ใช้บังคับเข้าโหมดตั้งค่า
#define CONFIG_BUTTON_WINDOW   3000    // ms: ช่วงเวลาให้กดปุ่ม BOOT หลังเปิดเครื่อง/กด RESET
#define STATUS_LED_PIN         -1      // -1 = ไม่ใช้ LED (ถ้าบอร์ดมี LED ใส่เลขขาได้ เช่น 8)

// ================= ตั้งค่าเซิร์ฟเวอร์ =================
String server = "sac.eng.kps.ku.ac.th";
String pathphp = "/Dairy-cattle-system-test/IoTDevice/Activity/input.php";

// ================= ตั้งค่าฮาร์ดแวร์ =================
#define ONE_WIRE_BUS        D3
#define I2C_SDA_PIN         D5
#define I2C_SCL_PIN         D4

// RTC memory (หน่วยความจำที่ไม่หายตอน Deep Sleep)
RTC_DATA_ATTR int   stepCount        = 0;
RTC_DATA_ATTR float tempC            = 0;
RTC_DATA_ATTR int   idleWakeCount    = 0;
RTC_DATA_ATTR float prevMagnitude    = 1.0f;
RTC_DATA_ATTR bool  bigSleepMode     = false;

// ตัวแปรเก็บค่าท่าทางและการนับการยืน/การนอน (ไม่หายตอน Deep Sleep)
RTC_DATA_ATTR float last_angX        = 0;
RTC_DATA_ATTR float last_angY        = 0;
RTC_DATA_ATTR float last_angZ        = 0;
RTC_DATA_ATTR int   last_stand       = 1;      // 1 = ยืน (Stand=1, Sleep=0), 0 = นอน (Stand=0, Sleep=1)
RTC_DATA_ATTR int   standCount       = 1;      // จำนวนครั้งที่เปลี่ยนเป็นท่ายืน
RTC_DATA_ATTR int   lieCount         = 0;      // จำนวนครั้งที่เปลี่ยนเป็นท่านอน
RTC_DATA_ATTR float rtcGravX         = REF_GRAV_X;
RTC_DATA_ATTR float rtcGravY         = REF_GRAV_Y;
RTC_DATA_ATTR float rtcGravZ         = REF_GRAV_Z;
RTC_DATA_ATTR float rtcTiltDeg       = 0.0f;
RTC_DATA_ATTR float rtcTransitionSec = 0.0f;   // สะสมเวลาหน่วงเปลี่ยนท่าต่อเนื่อง

MPU6050 mpu(Wire);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

unsigned long lastSampleTime   = 0;
unsigned long lastStepTime     = 0;
unsigned long lastMovementTime = 0;
unsigned long timer            = 0;
int           count            = 0;
bool          movementDetected = false;
bool          stepDetected     = false;
bool          stepArmed        = true;
float         dynMag           = 0.0f;

// ตัวแปรการตั้งค่า (โหลดจาก Flash ทุกครั้งที่บูต)
Preferences preferences;   // ตัวช่วยบันทึกค่าลง Flash
Ticker      ticker;        // ตัวช่วยกระพริบไฟ
int         deviceId  = DEFAULT_DEVICE_ID;
DeviceKey   deviceKey = getDeviceKey(DEFAULT_DEVICE_ID);
String      wifiSsid  = "";
String      wifiPass  = "";

// ================================================================
// คำนวณมุมเอียงต้นขาเทียบกับท่ายืนอ้างอิง (0 - 180 องศา)
// ================================================================
float computeLegTiltDeg(float gx, float gy, float gz) {
  float normG = sqrtf(gx * gx + gy * gy + gz * gz);
  float normR = sqrtf(REF_GRAV_X * REF_GRAV_X + REF_GRAV_Y * REF_GRAV_Y + REF_GRAV_Z * REF_GRAV_Z);
  if (normG < 0.001f || normR < 0.001f) return 0.0f;

  float dot = (gx * REF_GRAV_X + gy * REF_GRAV_Y + gz * REF_GRAV_Z) / (normG * normR);
  if (dot > 1.0f) dot = 1.0f;
  if (dot < -1.0f) dot = -1.0f;
  return acosf(dot) * (180.0f / PI);
}

// ================================================================
// การจำค่า Device ID / WiFi ลง Flash (Preferences)
// ================================================================
void loadSettings() {
  preferences.begin("cattle", false);
  deviceId = preferences.getInt("devID", DEFAULT_DEVICE_ID);
  wifiSsid = preferences.getString("ssid", DEFAULT_WIFI_SSID);
  wifiPass = preferences.getString("pass", DEFAULT_WIFI_PASS);
  preferences.end();

  if (!isValidDeviceId(deviceId)) deviceId = DEFAULT_DEVICE_ID;
  deviceKey = getDeviceKey(deviceId);
}

void saveSettings() {
  preferences.begin("cattle", false);
  preferences.putInt("devID", deviceId);
  preferences.putString("ssid", wifiSsid);
  preferences.putString("pass", wifiPass);
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
  Serial.println("แล้วเปิด http://192.168.4.1 เพื่อตั้งค่า WiFi และ Device ID");
#if STATUS_LED_PIN >= 0
  ticker.attach(0.2, tick);
#endif
}

// รอให้กดปุ่ม BOOT ภายใน CONFIG_BUTTON_WINDOW หลังบูต
// (ห้ามกดค้างตอนกด RESET เพราะชิปจะเข้าโหมดอัปโหลดโค้ดแทน)
bool configButtonPressed() {
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
  Serial.printf("👉 กดปุ่ม BOOT ภายใน %d วินาที เพื่อเข้าโหมดตั้งค่า WiFi / Device ID\n",
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
// เชื่อมต่อ WiFi ด้วยชื่อ/รหัสที่กำหนด (รอสูงสุด WIFI_CONNECT_TIMEOUT x 500ms)
// ================================================================
bool connectWiFi(const String &ssid, const String &pass) {
  if (ssid.length() == 0) return false;

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  // รีเซ็ตสถานะ Wi-Fi ก่อนเชื่อมต่อ (จำเป็นมากเวลาตื่นจาก Deep Sleep)
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid.c_str(), pass.c_str());

  // ลดกำลังส่ง Wi-Fi เพื่อป้องกันปัญหาไฟตก (Brownout) ตอนใช้แบตเตอรี่
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  // รอเชื่อมต่อ Wi-Fi สูงสุด 15 วินาที
  int wifi_timeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_timeout < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
    wifi_timeout++;
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

// ================================================================
// WiFiManager: ใช้เฉพาะตอนเปิดเครื่อง/กด RESET (ไม่ใช้ตอนตื่นจาก Deep Sleep)
// ================================================================
bool setupWiFiManager(bool forcePortal) {
  // ถ้าไม่กดปุ่ม BOOT -> ลองต่อ WiFi ที่บันทึกไว้ (หรือค่าเริ่มต้น TP-Link_0A08) ก่อน
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

  // สร้างช่องกรอก Device ID เพิ่มในหน้าตั้งค่า WiFi
  char idStr[4];
  snprintf(idStr, sizeof(idStr), "%d", deviceId);
  WiFiManagerParameter customDeviceId("did", "Device ID (1-20)", idStr, 3);
  wm.addParameter(&customDeviceId);

  // เปิดหน้าตั้งค่า (กดปุ่ม BOOT หรือ ต่อ WiFi เดิมไม่ได้)
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

  // อ่านค่า Device ID ใหม่ (เผื่อ user เปลี่ยนในหน้า config)
  int newID = String(customDeviceId.getValue()).toInt();
  if (isValidDeviceId(newID)) {
    if (newID != deviceId) Serial.println("📝 เปลี่ยน Device ID เป็น: " + String(newID));
    deviceId = newID;
  } else {
    Serial.println("⚠️ Device ID ไม่ถูกต้อง (ต้องเป็น 1-20) ใช้ค่าเดิม: " + String(deviceId));
  }
  deviceKey = getDeviceKey(deviceId);

  // จำชื่อ/รหัส WiFi ไว้ใช้ตอนตื่นจาก Deep Sleep
  wifiSsid = WiFi.SSID();
  wifiPass = WiFi.psk();
  saveSettings();
  return true;
}

// ฟังก์ชันสำหรับดึงเวลาปัจจุบันจากอินเทอร์เน็ต (NTP)
String getTimeStamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "0";
  }
  time_t now;
  time(&now);
  return String(now);
}

// ฟังก์ชันสำหรับส่งข้อมูลขึ้นเซิร์ฟเวอร์
void sendDataToAPI() {
  if (wifiSsid.length() == 0) {
    Serial.println("⚠️ ยังไม่ได้ตั้งค่า WiFi (กด RESET แล้วกดปุ่ม BOOT เพื่อตั้งค่า), skip API call.");
    return;
  }

  if (connectWiFi(wifiSsid, wifiPass)) {
    Serial.println("WiFi connected!");

    // 1. ซิงค์เวลา
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // เวลาประเทศไทย GMT+7
    delay(2000); // รอรับเวลาแป๊บนึง
    String ts = getTimeStamp();

    if (ts != "0") {
      // 2. สร้างรหัส OTP (ใช้ Key ของ Device ID ที่ตั้งค่าไว้)
      int otp_val = genCode(ts, deviceId, deviceKey.code1, deviceKey.code2, deviceKey.prime);

      // 3. เตรียมส่งข้อมูล
      WiFiClientSecure client;
      client.setInsecure(); // ไม่เช็ค SSL
      HTTPClient http;

      // จัดรูปแบบ URL
      String url = "https://" + server + pathphp +
                   "?DID=" + String(deviceId) +
                   "&ACT=" + String(stepCount) +
                   "&ANGX=" + String(last_angX) +
                   "&ANGY=" + String(last_angY) +
                   "&ANGZ=" + String(last_angZ) +
                   "&Stand=" + String(last_stand) +
                   "&Sleep=" + String(last_stand == 1 ? 0 : 1) +
                   "&Temp=" + String(tempC) +
                   "&Time=" + ts +
                   "&Code=" + String(otp_val);

      Serial.println("Sending API Request: " + url);
      http.begin(client, url);
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);

        // ถ้ายิงสำเร็จ รีเซ็ตจำนวนก้าวกลับเป็นศูนย์สำหรับรอบถัดไป
        if(httpResponseCode == 200) {
           stepCount = 0;
        }
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    }
  } else {
    Serial.println("WiFi Connection Failed, skip API call.");
  }

  WiFi.disconnect(true); // ตัดเน็ตก่อนนอน
}

void goToSleep() {
  if (!stepDetected) {
    idleWakeCount++;
  }

  if (idleWakeCount >= MAX_IDLE_WAKES) {
    bigSleepMode  = true;
    idleWakeCount = 0;
  }

  // === ก่อนเข้าโหมดหลับลึก ให้ส่งข้อมูลขึ้นเซิร์ฟเวอร์ก่อน ===
  Serial.println("Sending data before sleep...");
  sendDataToAPI();

  if (bigSleepMode) {
    Serial.println("😴 Big sleep mode — sleeping 30 seconds...");
    Serial.flush();
    esp_sleep_enable_timer_wakeup(LONG_SLEEP_INTERVAL);
  } else {
    Serial.print("💤 Idle wake count: ");
    Serial.print(idleWakeCount);
    Serial.print("/");
    Serial.println(MAX_IDLE_WAKES);
    Serial.flush();
    esp_sleep_enable_timer_wakeup(WAKE_INTERVAL);
  }
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  unsigned long serialTimeout = millis();
  while (!Serial && millis() - serialTimeout < 2000);
  delay(100);

  Serial.println("\n\n================================");
  Serial.println("🐄 Cow Stress Tracker Booting...");
  Serial.println("================================");

  // โหลด Device ID / WiFi ที่บันทึกไว้
  loadSettings();
  Serial.printf("💾 Device ID: %d | WiFi: %s\n", deviceId,
                wifiSsid.length() ? wifiSsid.c_str() : "(ยังไม่ได้ตั้งค่า)");

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
    WiFi.disconnect(true); // ปิด WiFi ระหว่างนับก้าว เพื่อประหยัดไฟ
    Serial.printf("🔑 ใช้ Key ของ Device ID %d\n", deviceId);
  }

  Serial.println("Initializing I2C (Wire) on D5, D4...");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  Serial.println("Checking MPU6050...");
  byte status = mpu.begin();
  Serial.print("MPU6050 status: ");
  Serial.println(status);
  if (status != 0) Serial.println("❌ ไม่พบ MPU6050 — ตรวจสอบสาย SDA/SCL");
  while (status != 0) {}

  if (!firstBoot) {
    Serial.println("⏰ Woke from sleep");
    mpu.update();
    rtcGravX = mpu.getAccX();
    rtcGravY = mpu.getAccY();
    rtcGravZ = mpu.getAccZ();
  } else {
    Serial.println("🔌 First boot — calibrating MPU6050...");
    idleWakeCount    = 0;
    prevMagnitude    = 1.0f;
    bigSleepMode     = false;
    stepCount        = 0;
    standCount       = 1;
    lieCount         = 0;
    last_stand       = 1;
    rtcTransitionSec = 0.0f;
    delay(1000);
    mpu.calcOffsets(true, false); // Calibrate เฉพาะ Gyro เพื่อรักษามุมแรงโน้มถ่วงของขากับพื้นโลก
    mpu.update();
    rtcGravX = mpu.getAccX();
    rtcGravY = mpu.getAccY();
    rtcGravZ = mpu.getAccZ();
    Serial.println("Calibration done!\n");
  }

  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();

  lastSampleTime   = millis();
  lastMovementTime = millis();
  movementDetected = false;
  stepDetected     = false;
  stepArmed        = true;
}

void loop() {
  unsigned long nowMs = millis();

  // อ่านและคำนวณที่ความถี่คงที่ 50 Hz (ทุก 20 ms)
  if (nowMs - lastSampleTime < SAMPLE_INTERVAL_MS) {
    return;
  }
  float dtSec = (nowMs - lastSampleTime) / 1000.0f;
  lastSampleTime = nowMs;
  count++;

  mpu.update();

  float ax        = mpu.getAccX();
  float ay        = mpu.getAccY();
  float az        = mpu.getAccZ();
  float magnitude = sqrtf(sq(ax) + sq(ay) + sq(az));

  // เก็บค่ามุมองศาล่าสุดไว้เผื่อส่งตอนหลับ
  last_angX = mpu.getAngleX();
  last_angY = mpu.getAngleY();
  last_angZ = mpu.getAngleZ();

  // 1. แยกเวกเตอร์แรงโน้มถ่วงโลกด้วย Low-pass filter
  const float gravAlpha = 0.04f;
  rtcGravX = (1.0f - gravAlpha) * rtcGravX + gravAlpha * ax;
  rtcGravY = (1.0f - gravAlpha) * rtcGravY + gravAlpha * ay;
  rtcGravZ = (1.0f - gravAlpha) * rtcGravZ + gravAlpha * az;

  // 2. คำนวณความเร่งของการขยับขา (Dynamic Acceleration) และกรองสัญญาณ
  float dynX      = ax - rtcGravX;
  float dynY      = ay - rtcGravY;
  float dynZ      = az - rtcGravZ;
  float rawDynMag = sqrtf(dynX * dynX + dynY * dynY + dynZ * dynZ);
  dynMag = (1.0f - ACCEL_SMOOTH_ALPHA) * dynMag + ACCEL_SMOOTH_ALPHA * rawDynMag;

  // 3. คำนวณมุมเอียงต้นขาเทียบกับท่ายืนอ้างอิง (REF_GRAV_X, Y, Z)
  rtcTiltDeg = computeLegTiltDeg(rtcGravX, rtcGravY, rtcGravZ);

  // 4. ตรวจจับการเปลี่ยนท่า (ยืน <-> นอน) พร้อมหน่วงเวลาแยกขาเข้าและขาออก
  if (last_stand == 1) {
    // สถานะปัจจุบัน: ยืน -> เช็กว่าเอียงเกิน LIE_ANGLE_DEG ต่อเนื่องครบ STAND_TO_LIE_DELAY_SEC หรือยัง
    if (rtcTiltDeg >= LIE_ANGLE_DEG) {
      rtcTransitionSec += dtSec;
      if (rtcTransitionSec >= (float)STAND_TO_LIE_DELAY_SEC) {
        last_stand       = 0; // เปลี่ยนเป็น "นอน" (ล็อกการนับก้าว)
        lieCount++;
        rtcTransitionSec = 0.0f;
        Serial.println("🟣 เปลี่ยนสถานะเป็น: นอน (LYING) -> ล็อกการนับก้าว");
      }
    } else {
      rtcTransitionSec = 0.0f;
    }
  } else {
    // สถานะปัจจุบัน: นอน -> เช็กว่าขากลับมาตั้งตรง <= STAND_ANGLE_DEG ครบ LIE_TO_STAND_DELAY_SEC หรือยัง
    if (rtcTiltDeg <= STAND_ANGLE_DEG) {
      rtcTransitionSec += dtSec;
      if (rtcTransitionSec >= (float)LIE_TO_STAND_DELAY_SEC) {
        last_stand       = 1; // เปลี่ยนเป็น "ยืน" (ปลดล็อกการนับก้าว)
        standCount++;
        rtcTransitionSec = 0.0f;
        Serial.println("🟢 เปลี่ยนสถานะเป็น: ยืน (STANDING) -> เปิดการนับก้าว");
      }
    } else {
      rtcTransitionSec = 0.0f;
    }
  }

  // 5. เช็กการเคลื่อนไหวเพื่อต่อเวลาไม่ให้เพิ่งเข้า Deep Sleep
  float magnitudeDelta = fabsf(magnitude - prevMagnitude);
  if (count > 5 && magnitudeDelta > DELTA_THRESHOLD) {
    lastMovementTime = nowMs;
    movementDetected = true;
  }
  prevMagnitude = magnitude;

  // 6. การนับก้าว:
  //    - รีเซ็ตจังหวะเมื่อแรงลดลงต่ำกว่า STEP_RESET_G
  //    - นับเฉพาะตอนยืน (last_stand == 1) เท่านั้น! ถ้านอนอยู่จะไม่นับก้าวเด็ดขาด
  if (dynMag <= STEP_RESET_G) {
    stepArmed = true;
  }

  if (last_stand == 1 &&
      stepArmed &&
      dynMag >= STEP_TRIGGER_G &&
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

  // --- Sleep Check (ไม่มีการขยับเกิน ACTIVE_WINDOW_MS และไม่ได้อยู่ในช่วงหน่วงรอเปลี่ยนท่า) ---
  if (nowMs - lastMovementTime > ACTIVE_WINDOW_MS && rtcTransitionSec == 0.0f) {
    goToSleep();
  }

  if (nowMs - timer > 200) {
    Serial.print(last_stand == 1 ? "[STAND] " : "[LIE-LOCKED] ");
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
